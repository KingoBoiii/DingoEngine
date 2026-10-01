#include "depch.h"
#include "DingoEngine/Graphics/Renderer3D.h"
#include "DingoEngine/Asset/UnmanagedShaderWatch.h"

#include <glm/gtc/matrix_inverse.hpp>

namespace
{
#include "Renderer3D_Lit.glsl.inl"

	constexpr const char* k_LitShaderName = "Renderer3DMeshShader";

	// The source file when this build can see it, so the AssetManager can hot-reload it;
	// otherwise the copy compiled into the library.
	Dingo::Shader* CreateLitShader()
	{
#ifdef DE_ENGINE_SHADER_DIR
		const std::filesystem::path sourcePath = std::filesystem::path(u8"" DE_ENGINE_SHADER_DIR) / "Renderer3D_Lit.glsl";
		std::error_code ec;
		if (std::filesystem::exists(sourcePath, ec))
			return Dingo::Shader::CreateFromFile(k_LitShaderName, sourcePath);
#endif
		const std::string source(reinterpret_cast<const char*>(k_Renderer3D_Lit_glsl), sizeof(k_Renderer3D_Lit_glsl));
		return Dingo::Shader::CreateFromSource(k_LitShaderName, source);
	}
}

namespace Dingo
{

	Renderer3D* Renderer3D::Create(const Renderer3DParams& params)
	{
		Renderer3D* renderer = new Renderer3D(params);
		renderer->Initialize();
		return renderer;
	}

	Renderer3D::~Renderer3D()
	{
		Internal::UnwatchUnmanagedShader(m_Shader);
	}

	void Renderer3D::Initialize()
	{
		m_Shader = CreateLitShader();
		Internal::WatchUnmanagedShader(m_Shader);

		m_Layout = VertexLayout()
			.SetStride(sizeof(Vertex))
			.AddAttribute("a_Position", Format::RGB32_FLOAT, offsetof(Vertex, Position))
			.AddAttribute("a_Normal", Format::RGB32_FLOAT, offsetof(Vertex, Normal))
			.AddAttribute("a_Color", Format::RGBA32_FLOAT, offsetof(Vertex, Color))
			.AddAttribute("a_TexCoord", Format::RG32_FLOAT, offsetof(Vertex, TexCoord));

		m_Material = Material::Create(MaterialParams()
			.SetDebugName("Renderer3D_Material")
			.SetShader(m_Shader)
			// No back-face culling: front-face winding differs between the Vulkan and
			// D3D back-ends, so culling that looks right on one culls the visible faces
			// on the other. Disabling it keeps the renderer backend-agnostic (this is
			// what the Breakout3D / Physics3D mesh batchers did too); depth testing
			// still resolves occlusion correctly.
			.SetCullMode(CullMode::None));
		// SetUniform now so the binding-1 UBO exists before the first draw (same reasoning
		// as the DungeonCrawler3D glow material). Default params are black/0, matching the
		// shader's additive no-op, so this doesn't change existing default-material output.
		m_Material->SetUniform(MaterialData{});

		// Camera + light live in a shared scene UBO (binding 0) bound on every material,
		// rather than baked into the default material — so custom materials receive the
		// camera/light too. Uploaded each BeginScene.
		m_CameraData.LightDirection = glm::vec4(m_Params.LightDirection, 0.0f);
		m_CameraData.Ambient = glm::vec4(m_Params.Ambient, 0.0f, 0.0f, 0.0f);
		// Volatile constant buffer — written into the frame's command list each
		// BeginScene (via Renderer::Upload), not pre-uploaded here.
		m_SceneUniformBuffer = GraphicsBuffer::CreateUniformBuffer(sizeof(CameraData), "Renderer3D_SceneUBO");

		// Built-in unit primitives for the DrawBox/DrawSphere conveniences.
		m_BoxMesh = Mesh::CreateBox();
		m_SphereMesh = Mesh::CreateSphere(0.5f, 16, 16);
	}

	void Renderer3D::Shutdown()
	{
		for (GraphicsBuffer*& buffer : m_BatchVertexBuffers)
			DestroyAndDelete(buffer);
		for (GraphicsBuffer*& buffer : m_BatchIndexBuffers)
			DestroyAndDelete(buffer);
		m_BatchVertexBuffers.clear();
		m_BatchIndexBuffers.clear();
		m_Batches.clear();
		m_DrawOrder.clear();

		delete m_BoxMesh;
		delete m_SphereMesh;
		m_BoxMesh = nullptr;
		m_SphereMesh = nullptr;

		DestroyAndDelete(m_SceneUniformBuffer);
		DestroyAndDelete(m_Material);
		Internal::UnwatchUnmanagedShader(m_Shader);
		DestroyAndDelete(m_Shader);
	}

	void Renderer3D::BeginScene(const PerspectiveCamera& camera)
	{
		BeginScene(camera.GetViewProjectionMatrix());
	}

	void Renderer3D::BeginScene(const glm::mat4& viewProjection)
	{
		m_CameraData.ViewProjection = viewProjection;
		// Write the volatile scene UBO into this frame's command list, before any draw
		// binds it (CommandList::UploadBuffer, the same path material UBOs use).
		Renderer::Upload(m_SceneUniformBuffer, &m_CameraData, sizeof(CameraData));

		// The default material's binding-1 UBO is a VOLATILE constant buffer: NVRHI
		// requires a write into every frame that binds it, so this upload must be
		// unconditional — do NOT dirty-gate it (skipping the write on unchanged frames
		// floods "binding volatile constant buffer before writing" errors).
		MaterialData materialData;
		materialData.EmissiveColor = glm::vec4(m_Material->GetEmissiveColor(), 0.0f);
		materialData.EmissiveStrength = glm::vec4(m_Material->GetEmissiveStrength(), 0.0f, 0.0f, 0.0f);
		m_Material->SetUniform(materialData);

		m_Statistics = {};

		// Reset the per-material batches, keeping their storage for reuse. Every chunk, not
		// just last scene's: SubmitMesh takes a spare chunk before growing.
		for (auto& [material, matBatch] : m_Batches)
		{
			for (MeshChunk& chunk : matBatch.Chunks)
			{
				chunk.Vertices.clear();
				chunk.Indices.clear();
			}
			matBatch.ChunksInUse = 0;
			matBatch.OverflowWarned = false;
			matBatch.Enqueued = false;
		}
		m_DrawOrder.clear();
		m_SceneActive = true;
	}

	void Renderer3D::EndScene()
	{
		if (!m_SceneActive)
			return; // guard against EndScene() without BeginScene() (or a double call)

		m_SceneActive = false;

		const Renderer3DCapabilities& caps = m_Params.Capabilities;
		uint32_t batchIndex = 0;

		// One indexed draw per batch, each from its own pooled (vertex, index) buffer so no
		// shared buffer is re-uploaded between draws.
		for (Material* material : m_DrawOrder)
		{
			MaterialBatch& matBatch = m_Batches[material];
			for (uint32_t chunkIndex = 0; chunkIndex < matBatch.ChunksInUse; ++chunkIndex)
			{
				MeshChunk& chunk = matBatch.Chunks[chunkIndex];
				if (chunk.Indices.empty())
					continue;

				// Grow the buffer pool on demand; each pooled pair holds a full-capacity batch.
				// DirectUpload = false: the writes go into the frame's command list (as
				// Renderer2D's batches do) instead of each spinning up a throwaway command list
				// and its own queue submit.
				if (batchIndex >= m_BatchVertexBuffers.size())
				{
					m_BatchVertexBuffers.push_back(GraphicsBuffer::CreateVertexBuffer(sizeof(Vertex) * caps.MaxVertices, nullptr, false, "Renderer3D_BatchVB"));
					m_BatchIndexBuffers.push_back(GraphicsBuffer::CreateIndexBuffer(sizeof(uint32_t) * caps.MaxIndices, nullptr, false, "Renderer3D_BatchIB", GraphicsFormat::Uint32));
				}

				GraphicsBuffer* vertexBuffer = m_BatchVertexBuffers[batchIndex];
				GraphicsBuffer* indexBuffer = m_BatchIndexBuffers[batchIndex];

				Renderer::Upload(vertexBuffer, chunk.Vertices.data(), static_cast<uint32_t>(chunk.Vertices.size() * sizeof(Vertex)));
				Renderer::Upload(indexBuffer, chunk.Indices.data(), static_cast<uint32_t>(chunk.Indices.size() * sizeof(uint32_t)));

				// Bind the shared camera/light UBO at binding 0 for this material, then draw.
				material->SetSceneUniformBuffer(m_SceneUniformBuffer);
				Renderer::DrawIndexed(material, m_Layout, vertexBuffer, indexBuffer, static_cast<uint32_t>(chunk.Indices.size()));
				++m_Statistics.DrawCalls;

				++batchIndex;
			}
		}
	}

	void Renderer3D::Clear(const glm::vec4& clearColor)
	{
		Renderer::Clear(clearColor);
	}

	void Renderer3D::SetDirectionalLight(const glm::vec3& direction, float ambient)
	{
		m_Params.LightDirection = direction;
		m_Params.Ambient = ambient;
		m_CameraData.LightDirection = glm::vec4(direction, 0.0f);
		m_CameraData.Ambient = glm::vec4(ambient, 0.0f, 0.0f, 0.0f);
		// Uploaded to the scene UBO in BeginScene (called next by the SceneRenderer).
	}

	void Renderer3D::SubmitMesh(const Mesh* mesh, const glm::mat4& transform, const glm::vec4& color, Material* material)
	{
		if (!m_SceneActive || !mesh)
			return;

		Material* batchMaterial = material ? material : m_Material;
		MaterialBatch& matBatch = m_Batches[batchMaterial];
		if (!matBatch.Enqueued)
		{
			matBatch.Enqueued = true;
			m_DrawOrder.push_back(batchMaterial);
		}

		const std::vector<MeshVertex>& vertices = mesh->GetVertices();
		const std::vector<uint32_t>& indices = mesh->GetIndices();
		const Renderer3DCapabilities& caps = m_Params.Capabilities;

		// A mesh bigger than an empty batch fits no batch at all, so spilling cannot help.
		if (vertices.size() > caps.MaxVertices || indices.size() > caps.MaxIndices)
		{
			// DE_CORE_ASSERT takes a plain message, not a format string — the vert/index
			// counts are in the warn below.
			DE_CORE_ASSERT(!caps.AssertOnOverflow,
				"Renderer3D mesh exceeds a single batch's capacity and AssertOnOverflow is set. Raise Renderer3DCapabilities or submit a smaller mesh.");

			if (!matBatch.OverflowWarned)
			{
				DE_CORE_WARN("Renderer3D mesh exceeds a single batch's capacity ({} verts / {} indices); dropping this mesh. Raise Renderer3DCapabilities.MaxVertices/MaxIndices.",
					caps.MaxVertices, caps.MaxIndices);
				matBatch.OverflowWarned = true;
			}
			++m_Statistics.DroppedMeshes;
			return;
		}

		if (matBatch.ChunksInUse == 0)
		{
			if (matBatch.Chunks.empty())
				matBatch.Chunks.emplace_back();
			matBatch.ChunksInUse = 1;
		}

		MeshChunk* chunk = &matBatch.Chunks[matBatch.ChunksInUse - 1];
		if (chunk->Vertices.size() + vertices.size() > caps.MaxVertices ||
			chunk->Indices.size() + indices.size() > caps.MaxIndices)
		{
			if (matBatch.ChunksInUse == matBatch.Chunks.size())
				matBatch.Chunks.emplace_back();
			chunk = &matBatch.Chunks[matBatch.ChunksInUse];
			++matBatch.ChunksInUse;
		}

		// Normals need the inverse-transpose so non-uniform scale (stretched walls)
		// doesn't skew them; positions just use the model matrix.
		const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(transform));
		const uint32_t vertexOffset = static_cast<uint32_t>(chunk->Vertices.size());

		for (const MeshVertex& v : vertices)
		{
			Vertex vertex;
			vertex.Position = glm::vec3(transform * glm::vec4(v.Position, 1.0f));
			vertex.Normal = normalMatrix * v.Normal;
			vertex.Color = color;
			vertex.TexCoord = v.TexCoord;
			chunk->Vertices.push_back(vertex);
		}

		for (uint32_t index : indices)
			chunk->Indices.push_back(index + vertexOffset);

		++m_Statistics.SubmittedMeshes;
		m_Statistics.VertexCount += static_cast<uint32_t>(vertices.size());
		m_Statistics.IndexCount += static_cast<uint32_t>(indices.size());
	}

	void Renderer3D::DrawBox(const glm::mat4& transform, const glm::vec4& color)
	{
		SubmitMesh(m_BoxMesh, transform, color);
	}

	void Renderer3D::DrawSphere(const glm::mat4& transform, const glm::vec4& color)
	{
		SubmitMesh(m_SphereMesh, transform, color);
	}

}
