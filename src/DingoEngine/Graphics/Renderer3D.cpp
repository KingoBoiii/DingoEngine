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

	// The camera is the one point a view-projection sends to clip (0, 0, k, 0), so it is the
	// inverse image of that direction. An orthographic camera sits at infinity: w is 0 there,
	// and the result is the direction towards it. A singular matrix yields an arbitrary direction
	// rather than NaNs.
	glm::vec4 CameraPositionFromViewProjection(const glm::mat4& viewProjection)
	{
		const glm::vec4 eye = glm::inverse(viewProjection) * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
		if (std::abs(eye.w) > 1e-6f)
			return glm::vec4(glm::vec3(eye) / eye.w, 1.0f);

		const float length = glm::length(glm::vec3(eye));
		return length > 0.0f ? glm::vec4(-glm::vec3(eye) / length, 0.0f) : glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
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

		// Camera + lights live in a shared scene UBO (binding 0) bound on every material,
		// rather than baked into the default material — so custom materials receive them too.
		// Volatile constant buffer — written into the frame's command list each EndScene (via
		// Renderer::Upload), not pre-uploaded here.
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
		BeginSceneInternal(camera.GetViewProjectionMatrix(), glm::vec4(camera.GetPosition(), 1.0f));
	}

	void Renderer3D::BeginScene(const glm::mat4& viewProjection)
	{
		BeginSceneInternal(viewProjection, CameraPositionFromViewProjection(viewProjection));
	}

	void Renderer3D::BeginSceneInternal(const glm::mat4& viewProjection, const glm::vec4& cameraPosition)
	{
		m_CameraData.ViewProjection = viewProjection;
		m_CameraData.CameraPosition = cameraPosition;

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

		// Written into this frame's command list ahead of every draw that binds it
		// (CommandList::UploadBuffer, the same path material UBOs use).
		ResolveSceneLights();
		Renderer::Upload(m_SceneUniformBuffer, &m_CameraData, sizeof(CameraData));

		m_CameraData.AmbientColor = glm::vec4(0.0f);
		m_CameraData.LightCounts = glm::ivec4(0);
		m_SceneLightSubmitted = false;
		m_DroppedLights = 0;

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

	void Renderer3D::SubmitLight(const DirectionalLight& light)
	{
		m_SceneLightSubmitted = true;

		int& count = m_CameraData.LightCounts.x;
		if (count >= static_cast<int>(k_MaxDirectionalLights))
		{
			DE_CORE_ASSERT(!m_Params.Capabilities.AssertOnOverflow,
				"Renderer3D: more directional lights than k_MaxDirectionalLights and AssertOnOverflow is set.");

			if (!m_LightOverflowWarned)
			{
				DE_CORE_WARN("Renderer3D: a scene submitted more than {} directional lights; the extra ones are dropped.", k_MaxDirectionalLights);
				m_LightOverflowWarned = true;
			}
			++m_DroppedLights;
			return;
		}

		m_CameraData.DirectionalLights[count] = { glm::vec4(light.Direction, 0.0f), glm::vec4(light.Color * light.Intensity, 0.0f) };
		++count;
	}

	void Renderer3D::SetAmbientLight(const glm::vec3& color, float intensity)
	{
		m_SceneLightSubmitted = true;
		m_CameraData.AmbientColor = glm::vec4(color * intensity, 0.0f);
	}

	void Renderer3D::SetDirectionalLight(const glm::vec3& direction, float ambient)
	{
		m_Params.LightDirection = direction;
		m_Params.Ambient = ambient;
	}

	void Renderer3D::ResolveSceneLights()
	{
		if (!m_SceneLightSubmitted)
		{
			// The default light's ambient + (1 - ambient) * N·L, as one white light over a white
			// ambient.
			const float ambient = m_Params.Ambient;
			m_CameraData.AmbientColor = glm::vec4(glm::vec3(ambient), 0.0f);
			m_CameraData.DirectionalLights[0] = { glm::vec4(m_Params.LightDirection, 0.0f), glm::vec4(glm::vec3(1.0f - ambient), 0.0f) };
			m_CameraData.LightCounts.x = 1;
		}

		const int directionalCount = m_CameraData.LightCounts.x;
		m_CameraData.LightDirection = directionalCount > 0 ? m_CameraData.DirectionalLights[0].Direction : glm::vec4(m_Params.LightDirection, 0.0f);
		const glm::vec4& ambientColor = m_CameraData.AmbientColor;
		m_CameraData.Ambient = glm::vec4(std::max({ ambientColor.r, ambientColor.g, ambientColor.b }), 0.0f, 0.0f, 0.0f);

		m_Statistics.DirectionalLights = static_cast<uint32_t>(directionalCount);
		m_Statistics.DroppedLights = m_DroppedLights;
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
