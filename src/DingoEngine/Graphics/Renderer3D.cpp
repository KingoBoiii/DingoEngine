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
		const Renderer3DCapabilities& caps = m_Params.Capabilities;

		m_VertexBuffer = GraphicsBuffer::CreateVertexBuffer(sizeof(Vertex) * caps.MaxVertices, nullptr, true, "Renderer3D_VB");
		m_IndexBuffer = GraphicsBuffer::CreateIndexBuffer(sizeof(uint32_t) * caps.MaxIndices, nullptr, true, "Renderer3D_IB", GraphicsFormat::Uint32);

		m_VertexBase = new Vertex[caps.MaxVertices];
		m_IndexBase = new uint32_t[caps.MaxIndices];

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

		m_CameraData.LightDirection = glm::vec4(m_Params.LightDirection, 0.0f);
		m_CameraData.Ambient = glm::vec4(m_Params.Ambient, 0.0f, 0.0f, 0.0f);
		m_Material->SetUniform(m_CameraData);

		// Built-in unit primitives for the DrawBox/DrawSphere conveniences.
		m_BoxMesh = Mesh::CreateBox();
		m_SphereMesh = Mesh::CreateSphere(0.5f, 16, 16);
	}

	void Renderer3D::Shutdown()
	{
		delete[] m_VertexBase;
		delete[] m_IndexBase;
		m_VertexBase = nullptr;
		m_IndexBase = nullptr;

		delete m_BoxMesh;
		delete m_SphereMesh;
		m_BoxMesh = nullptr;
		m_SphereMesh = nullptr;

		if (m_VertexBuffer) { m_VertexBuffer->Destroy(); m_VertexBuffer = nullptr; }
		if (m_IndexBuffer) { m_IndexBuffer->Destroy(); m_IndexBuffer = nullptr; }
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
		m_Material->SetUniform(m_CameraData);

		m_VertexPtr = m_VertexBase;
		m_IndexPtr = m_IndexBase;
		m_IndexCount = 0;
		m_VertexOffset = 0;
		m_SceneActive = true;
		m_OverflowWarned = false; // re-arm the one-shot overflow warning for this scene
	}

	void Renderer3D::EndScene()
	{
		Flush();
		m_SceneActive = false;
	}

	void Renderer3D::Clear(const glm::vec4& clearColor)
	{
		Renderer::Clear(clearColor);
	}

	void Renderer3D::SubmitMesh(const Mesh* mesh, const glm::mat4& transform, const glm::vec4& color)
	{
		if (!m_SceneActive || !mesh)
			return;

		const std::vector<MeshVertex>& vertices = mesh->GetVertices();
		const std::vector<uint32_t>& indices = mesh->GetIndices();

		const uint32_t usedVertices = static_cast<uint32_t>(m_VertexPtr - m_VertexBase);
		const uint32_t usedIndices = m_IndexCount;
		if (usedVertices + vertices.size() > m_Params.Capabilities.MaxVertices ||
			usedIndices + indices.size() > m_Params.Capabilities.MaxIndices)
		{
			if (!m_OverflowWarned)
			{
				DE_CORE_WARN("Renderer3D batch capacity exceeded ({} verts / {} indices); dropping further meshes this scene. Raise Renderer3DCapabilities.",
					m_Params.Capabilities.MaxVertices, m_Params.Capabilities.MaxIndices);
				m_OverflowWarned = true;
			}
			return;
		}

		// Normals need the inverse-transpose so non-uniform scale (stretched walls)
		// doesn't skew them; positions just use the model matrix.
		const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(transform));

		for (const MeshVertex& v : vertices)
		{
			m_VertexPtr->Position = glm::vec3(transform * glm::vec4(v.Position, 1.0f));
			m_VertexPtr->Normal = normalMatrix * v.Normal;
			m_VertexPtr->Color = color;
			m_VertexPtr++;
		}

		for (uint32_t index : indices)
			*m_IndexPtr++ = index + m_VertexOffset;

		m_VertexOffset += static_cast<uint32_t>(vertices.size());
		m_IndexCount += static_cast<uint32_t>(indices.size());
	}

	void Renderer3D::DrawBox(const glm::mat4& transform, const glm::vec4& color)
	{
		SubmitMesh(m_BoxMesh, transform, color);
	}

	void Renderer3D::DrawSphere(const glm::mat4& transform, const glm::vec4& color)
	{
		SubmitMesh(m_SphereMesh, transform, color);
	}

	void Renderer3D::Flush()
	{
		if (m_IndexCount == 0)
			return;

		const uint32_t vertexDataSize = static_cast<uint32_t>(
			reinterpret_cast<uint8_t*>(m_VertexPtr) - reinterpret_cast<uint8_t*>(m_VertexBase));

		m_VertexBuffer->Upload(m_VertexBase, vertexDataSize);
		m_IndexBuffer->Upload(m_IndexBase, m_IndexCount * sizeof(uint32_t));

		Renderer::DrawIndexed(m_Material, m_Layout, m_VertexBuffer, m_IndexBuffer, m_IndexCount);
	}

}
