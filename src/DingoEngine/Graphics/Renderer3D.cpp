#include "depch.h"
#include "DingoEngine/Graphics/Renderer3D.h"

#include <glm/gtc/matrix_inverse.hpp>

namespace
{
	// Flat-shaded mesh shader with a single directional light. Vertices arrive in
	// world space (the renderer transforms them on the CPU while batching), so the
	// vertex stage only applies the camera and forwards the lit inputs.
	constexpr const char* k_MeshShaderSource = R"(
#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;

layout(std140, binding = 0) uniform CameraData
{
	mat4 ViewProjection;
	vec4 LightDirection; // xyz = direction the light travels
	vec4 Ambient;        // x = ambient strength
};

layout(location = 0) out vec3 v_Normal;
layout(location = 1) out vec4 v_Color;
layout(location = 2) out vec3 v_LightDir;
layout(location = 3) out float v_Ambient;

void main()
{
	gl_Position = ViewProjection * vec4(a_Position, 1.0);
	v_Normal    = a_Normal;
	v_Color     = a_Color;
	v_LightDir  = LightDirection.xyz;
	v_Ambient   = Ambient.x;
}

#type fragment
#version 450

layout(location = 0) in vec3 v_Normal;
layout(location = 1) in vec4 v_Color;
layout(location = 2) in vec3 v_LightDir;
layout(location = 3) in float v_Ambient;

layout(location = 0) out vec4 o_Color;

void main()
{
	vec3 normal = normalize(v_Normal);
	vec3 toLight = normalize(-v_LightDir);
	float diffuse = max(dot(normal, toLight), 0.0);
	float lighting = v_Ambient + (1.0 - v_Ambient) * diffuse;
	o_Color = vec4(v_Color.rgb * lighting, v_Color.a);
}
)";
}

namespace Dingo
{

	Renderer3D* Renderer3D::Create(const Renderer3DParams& params)
	{
		Renderer3D* renderer = new Renderer3D(params);
		renderer->Initialize();
		return renderer;
	}

	void Renderer3D::Initialize()
	{
		m_Shader = Shader::CreateFromSource("Renderer3DMeshShader", k_MeshShaderSource);

		m_Layout = VertexLayout()
			.SetStride(sizeof(Vertex))
			.AddAttribute("a_Position", Format::RGB32_FLOAT, offsetof(Vertex, Position))
			.AddAttribute("a_Normal", Format::RGB32_FLOAT, offsetof(Vertex, Normal))
			.AddAttribute("a_Color", Format::RGBA32_FLOAT, offsetof(Vertex, Color));

		m_Material = Material::Create(MaterialParams()
			.SetDebugName("Renderer3D_Material")
			.SetShader(m_Shader)
			// No back-face culling: front-face winding differs between the Vulkan and
			// D3D back-ends, so culling that looks right on one culls the visible faces
			// on the other. Disabling it keeps the renderer backend-agnostic (this is
			// what the Breakout3D / Physics3D mesh batchers did too); depth testing
			// still resolves occlusion correctly.
			.SetCullMode(CullMode::None));

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
		for (GraphicsBuffer* buffer : m_BatchVertexBuffers)
			if (buffer) buffer->Destroy();
		for (GraphicsBuffer* buffer : m_BatchIndexBuffers)
			if (buffer) buffer->Destroy();
		m_BatchVertexBuffers.clear();
		m_BatchIndexBuffers.clear();
		m_Batches.clear();

		delete m_BoxMesh;
		delete m_SphereMesh;
		m_BoxMesh = nullptr;
		m_SphereMesh = nullptr;

		if (m_SceneUniformBuffer) { m_SceneUniformBuffer->Destroy(); m_SceneUniformBuffer = nullptr; }
		if (m_Material) { m_Material->Destroy(); delete m_Material; m_Material = nullptr; }
		if (m_Shader) { m_Shader->Destroy(); m_Shader = nullptr; }
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

		m_Statistics = {};

		// Reset the per-material batches, keeping their storage for reuse.
		for (auto& [material, batch] : m_Batches)
		{
			batch.Vertices.clear();
			batch.Indices.clear();
			batch.OverflowWarned = false;
		}
		m_SceneActive = true;
	}

	void Renderer3D::EndScene()
	{
		if (!m_SceneActive)
			return; // guard against EndScene() without BeginScene() (or a double call)

		m_SceneActive = false;

		const Renderer3DCapabilities& caps = m_Params.Capabilities;
		uint32_t batchIndex = 0;

		// One indexed draw per material, each from its own pooled (vertex, index) buffer
		// so no shared buffer is re-uploaded between draws.
		for (auto& [material, batch] : m_Batches)
		{
			if (batch.Indices.empty())
				continue;

			// Grow the buffer pool on demand; each pooled pair holds a full-capacity batch.
			if (batchIndex >= m_BatchVertexBuffers.size())
			{
				m_BatchVertexBuffers.push_back(GraphicsBuffer::CreateVertexBuffer(sizeof(Vertex) * caps.MaxVertices, nullptr, true, "Renderer3D_BatchVB"));
				m_BatchIndexBuffers.push_back(GraphicsBuffer::CreateIndexBuffer(sizeof(uint32_t) * caps.MaxIndices, nullptr, true, "Renderer3D_BatchIB", GraphicsFormat::Uint32));
			}

			GraphicsBuffer* vertexBuffer = m_BatchVertexBuffers[batchIndex];
			GraphicsBuffer* indexBuffer = m_BatchIndexBuffers[batchIndex];

			vertexBuffer->Upload(batch.Vertices.data(), static_cast<uint32_t>(batch.Vertices.size() * sizeof(Vertex)));
			indexBuffer->Upload(batch.Indices.data(), static_cast<uint32_t>(batch.Indices.size() * sizeof(uint32_t)));

			// Bind the shared camera/light UBO at binding 0 for this material, then draw.
			material->SetSceneUniformBuffer(m_SceneUniformBuffer);
			Renderer::DrawIndexed(material, m_Layout, vertexBuffer, indexBuffer, static_cast<uint32_t>(batch.Indices.size()));
			++m_Statistics.DrawCalls;

			++batchIndex;
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

		MeshBatch& batch = m_Batches[material ? material : m_Material];

		const std::vector<MeshVertex>& vertices = mesh->GetVertices();
		const std::vector<uint32_t>& indices = mesh->GetIndices();

		if (batch.Vertices.size() + vertices.size() > m_Params.Capabilities.MaxVertices ||
			batch.Indices.size() + indices.size() > m_Params.Capabilities.MaxIndices)
		{
			// Opt-in hard-fail so a blown vertex budget can't ship silently (see
			// Renderer3DCapabilities::AssertOnOverflow). Compiled out in release, where this
			// falls through to the warn-once-and-drop below. DE_CORE_ASSERT takes a plain
			// message (not a format string) — the vert/index counts are in the warn below.
			DE_CORE_ASSERT(!m_Params.Capabilities.AssertOnOverflow,
				"Renderer3D batch capacity exceeded and AssertOnOverflow is set. Raise Renderer3DCapabilities or submit fewer meshes.");

			if (!batch.OverflowWarned)
			{
				DE_CORE_WARN("Renderer3D batch capacity exceeded for a material ({} verts / {} indices); dropping further meshes this scene. Raise Renderer3DCapabilities.",
					m_Params.Capabilities.MaxVertices, m_Params.Capabilities.MaxIndices);
				batch.OverflowWarned = true;
			}
			++m_Statistics.DroppedMeshes;
			return;
		}

		// Normals need the inverse-transpose so non-uniform scale (stretched walls)
		// doesn't skew them; positions just use the model matrix.
		const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(transform));
		const uint32_t vertexOffset = static_cast<uint32_t>(batch.Vertices.size());

		for (const MeshVertex& v : vertices)
		{
			Vertex vertex;
			vertex.Position = glm::vec3(transform * glm::vec4(v.Position, 1.0f));
			vertex.Normal = normalMatrix * v.Normal;
			vertex.Color = color;
			batch.Vertices.push_back(vertex);
		}

		for (uint32_t index : indices)
			batch.Indices.push_back(index + vertexOffset);

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
