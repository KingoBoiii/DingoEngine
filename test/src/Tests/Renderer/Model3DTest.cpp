#include "Model3DTest.h"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

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

	void Model3DTest::Initialize()
	{
		m_Shader = Shader::CreateFromSource("Model3DShader", k_ShaderSrc);

		m_Layout = VertexLayout()
			.SetStride(sizeof(MeshVertex))
			.AddAttribute("a_Position", Format::RGB32_FLOAT, offsetof(MeshVertex, Position))
			.AddAttribute("a_Normal",   Format::RGB32_FLOAT, offsetof(MeshVertex, Normal))
			.AddAttribute("a_TexCoord", Format::RG32_FLOAT,  offsetof(MeshVertex, TexCoord));

		m_Camera = PerspectiveCamera(45.0f, m_AspectRatio, 0.1f, 200.0f);
		m_Camera.SetPosition({ 0.0f, 1.5f, 4.0f });
		m_Camera.SetTarget({ 0.0f, 0.0f, 0.0f });

		LoadModel(m_PathBuf);
	}

	void Model3DTest::LoadModel(const std::string& path)
	{
		UnloadModel();
		m_LoadFailed = false;

		m_Model = Model::LoadFromFile(path);
		if (!m_Model)
		{
			m_LoadFailed = true;
			return;
		}

		for (const auto& sm : m_Model->GetSubMeshes())
		{
			GpuSubMesh gpu;
			gpu.IndexCount = sm.MeshData->GetIndexCount();

			const auto& verts   = sm.MeshData->GetVertices();
			const auto& indices = sm.MeshData->GetIndices();

			gpu.VB = GraphicsBuffer::CreateVertexBuffer(verts.size()   * sizeof(MeshVertex), nullptr, true, "Model3D_VB");
			gpu.IB = GraphicsBuffer::CreateIndexBuffer( indices.size() * sizeof(uint32_t),   nullptr, true, "Model3D_IB", GraphicsFormat::Uint32);
			gpu.VB->Upload(verts.data(),   static_cast<uint64_t>(verts.size())   * sizeof(MeshVertex));
			gpu.IB->Upload(indices.data(), static_cast<uint64_t>(indices.size()) * sizeof(uint32_t));

			Texture* diffuse = sm.DiffuseTexture ? sm.DiffuseTexture : Renderer::GetWhiteTexture();

			gpu.Mat = Material::Create(MaterialParams()
				.SetDebugName("Model3D_Mat")
				.SetShader(m_Shader)
				.SetCullMode(CullMode::None));
			gpu.Mat->SetTexture(0, diffuse);
			gpu.Mat->SetSampler(0, Renderer::GetClampSampler());

			m_GpuSubMeshes.push_back(gpu);
		}
	}

	void Model3DTest::UnloadModel()
	{
		for (auto& gsm : m_GpuSubMeshes)
		{
			if (gsm.Mat) { gsm.Mat->Destroy(); delete gsm.Mat; }
			if (gsm.VB)  { gsm.VB->Destroy(); }
			if (gsm.IB)  { gsm.IB->Destroy(); }
		}
		m_GpuSubMeshes.clear();

		if (m_Model) { m_Model->Destroy(); delete m_Model; m_Model = nullptr; }
	}

	void Model3DTest::Update(float deltaTime)
	{
		m_Rotation += deltaTime * 30.0f;

		Renderer::Clear(m_ClearColor);

		if (m_GpuSubMeshes.empty())
			return;

		TransformUBO ubo;
		ubo.ViewProjection = m_Camera.GetViewProjectionMatrix();
		ubo.Model = glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation), glm::vec3(0.0f, 1.0f, 0.0f));

		for (auto& gsm : m_GpuSubMeshes)
		{
			gsm.Mat->SetUniform(ubo);
			Renderer::DrawIndexed(gsm.Mat, m_Layout, gsm.VB, gsm.IB, gsm.IndexCount);
		}
	}

	void Model3DTest::Cleanup()
	{
		UnloadModel();

		if (m_Shader) { m_Shader->Destroy(); m_Shader = nullptr; }
	}

	void Model3DTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
		m_Camera.SetAspectRatio(m_AspectRatio);
	}

	void Model3DTest::ImGuiRender()
	{
		GraphicsTest::ImGuiRender();
		ImGui::Separator();

		ImGui::InputText("Path", m_PathBuf, sizeof(m_PathBuf));
		if (ImGui::Button("Load"))
			LoadModel(m_PathBuf);

		ImGui::Separator();

		if (m_LoadFailed)
		{
			ImGui::TextColored({ 1.0f, 0.3f, 0.3f, 1.0f }, "Failed to load model.");
			return;
		}

		if (!m_Model)
		{
			ImGui::TextDisabled("No model loaded.");
			return;
		}

		ImGui::Text("SubMeshes: %u", m_Model->GetSubMeshCount());
		const auto& submeshes = m_Model->GetSubMeshes();
		for (uint32_t i = 0; i < static_cast<uint32_t>(submeshes.size()); ++i)
		{
			const auto& sm = submeshes[i];
			ImGui::Text("  [%u] verts=%u  idx=%u  tex=%s", i,
				sm.MeshData->GetVertexCount(),
				sm.MeshData->GetIndexCount(),
				sm.DiffuseTexture ? "yes" : "no");
		}
	}

}
