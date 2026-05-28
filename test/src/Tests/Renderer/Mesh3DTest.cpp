#include "Mesh3DTest.h"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <algorithm>

namespace
{
	static constexpr const char* k_ShaderSrc = R"(
#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

layout(std140, binding = 0) uniform CameraData {
    mat4 ViewProjection;
    mat4 Model;
};

layout(location = 0) out vec3 v_Normal;
layout(location = 1) out vec2 v_TexCoord;

void main()
{
    gl_Position = ViewProjection * Model * vec4(a_Position, 1.0);
    v_Normal    = mat3(Model) * a_Normal;
    v_TexCoord  = a_TexCoord;
}

#type fragment
#version 450

layout(location = 0) in  vec3 v_Normal;
layout(location = 1) in  vec2 v_TexCoord;
layout(location = 0) out vec4 o_Color;

layout(set = 0, binding = 1) uniform texture2D u_Diffuse;
layout(set = 0, binding = 2) uniform sampler   u_Sampler;

void main()
{
    vec4  tex     = texture(sampler2D(u_Diffuse, u_Sampler), v_TexCoord);
    vec3  light   = normalize(vec3(0.5, 1.0, 0.5));
    float diffuse = max(dot(normalize(v_Normal), light), 0.0);
    o_Color = vec4(tex.rgb * (0.3 + diffuse * 0.7), tex.a);
}
)";
}

namespace Dingo
{

	void Mesh3DTest::Initialize()
	{
		m_BoxMesh    = Mesh::CreateBox();
		m_SphereMesh = Mesh::CreateSphere();

		uint32_t maxVerts   = (std::max)(m_BoxMesh->GetVertexCount(),  m_SphereMesh->GetVertexCount());
		uint32_t maxIndices = (std::max)(m_BoxMesh->GetIndexCount(),   m_SphereMesh->GetIndexCount());

		m_VB = GraphicsBuffer::CreateVertexBuffer(maxVerts   * sizeof(MeshVertex), nullptr, true, "Mesh3D_VB");
		m_IB = GraphicsBuffer::CreateIndexBuffer( maxIndices * sizeof(uint32_t),   nullptr, true, "Mesh3D_IB", GraphicsFormat::Uint32);

		m_Shader = Shader::CreateFromSource("Mesh3DShader", k_ShaderSrc);

		m_Layout = VertexLayout()
			.SetStride(sizeof(MeshVertex))
			.AddAttribute("a_Position", Format::RGB32_FLOAT, offsetof(MeshVertex, Position))
			.AddAttribute("a_Normal",   Format::RGB32_FLOAT, offsetof(MeshVertex, Normal))
			.AddAttribute("a_TexCoord", Format::RG32_FLOAT,  offsetof(MeshVertex, TexCoord));

		m_Material = Material::Create(MaterialParams()
			.SetDebugName("Mesh3D_Material")
			.SetShader(m_Shader)
			.SetCullMode(CullMode::None));

		m_Material->SetTexture(0, Renderer::GetWhiteTexture());
		m_Material->SetSampler(0, Renderer::GetClampSampler());

		UploadMesh(m_BoxMesh);

		m_Camera = PerspectiveCamera(45.0f, m_AspectRatio, 0.1f, 100.0f);
		m_Camera.SetPosition({ 0.0f, 1.5f, 3.0f });
		m_Camera.SetTarget({ 0.0f, 0.0f, 0.0f });
	}

	void Mesh3DTest::UploadMesh(Mesh* mesh)
	{
		const auto& verts   = mesh->GetVertices();
		const auto& indices = mesh->GetIndices();
		m_VB->Upload(verts.data(),   static_cast<uint64_t>(verts.size())   * sizeof(MeshVertex));
		m_IB->Upload(indices.data(), static_cast<uint64_t>(indices.size()) * sizeof(uint32_t));
		m_IndexCount = mesh->GetIndexCount();
	}

	void Mesh3DTest::Update(float deltaTime)
	{
		m_Rotation += deltaTime * 45.0f;

		TransformUBO ubo;
		ubo.ViewProjection = m_Camera.GetViewProjectionMatrix();
		ubo.Model = glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation), glm::vec3(0.0f, 1.0f, 0.0f));
		m_Material->SetUniform(ubo);

		Renderer::Clear(m_ClearColor);
		Renderer::DrawIndexed(m_Material, m_Layout, m_VB, m_IB, m_IndexCount);
	}

	void Mesh3DTest::Cleanup()
	{
		delete m_BoxMesh;
		delete m_SphereMesh;
		m_BoxMesh    = nullptr;
		m_SphereMesh = nullptr;

		if (m_Material) { m_Material->Destroy(); delete m_Material; m_Material = nullptr; }
		if (m_Shader)   { m_Shader->Destroy();                       m_Shader   = nullptr; }
		if (m_VB)       { m_VB->Destroy();                           m_VB       = nullptr; }
		if (m_IB)       { m_IB->Destroy();                           m_IB       = nullptr; }
	}

	void Mesh3DTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
		m_Camera.SetAspectRatio(m_AspectRatio);
	}

	void Mesh3DTest::ImGuiRender()
	{
		GraphicsTest::ImGuiRender();
		ImGui::Separator();

		bool wasShowingSphere = m_ShowSphere;
		if (ImGui::RadioButton("Box",    !m_ShowSphere)) m_ShowSphere = false;
		ImGui::SameLine();
		if (ImGui::RadioButton("Sphere",  m_ShowSphere)) m_ShowSphere = true;

		if (m_ShowSphere != wasShowingSphere)
			UploadMesh(m_ShowSphere ? m_SphereMesh : m_BoxMesh);

		ImGui::Text("Vertices: %u  Indices: %u", m_IndexCount,
			m_ShowSphere ? m_SphereMesh->GetVertexCount() : m_BoxMesh->GetVertexCount());
	}

}
