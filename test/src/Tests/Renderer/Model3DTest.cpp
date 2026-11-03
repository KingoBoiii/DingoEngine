#include "Model3DTest.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>

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

	constexpr const char* k_ColorQuadsPath = "assets/models/ColorQuads/ColorQuads.gltf";

	bool NearColor(const glm::vec4& a, const glm::vec4& b)
	{
		return glm::all(glm::lessThanEqual(glm::abs(a - b), glm::vec4(1e-4f)));
	}
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

		const ApplicationCommandLineArgs& args = Application::Get().GetCommandLineArgs();
		if (auto path = args.Get("model"); path && !path->empty())
			std::snprintf(m_PathBuf, sizeof(m_PathBuf), "%.*s", static_cast<int>(path->size()), path->data());
		if (auto angle = args.Get("model-angle"); angle && !angle->empty())
		{
			m_Rotation   = std::strtof(std::string(*angle).c_str(), nullptr);
			m_AutoRotate = false;
		}

		LoadModel(m_PathBuf);
		RunBaseColorChecks();
	}

	void Model3DTest::RunBaseColorChecks()
	{
		m_Checks.clear();

		{
			std::unique_ptr<Model> model(Model::LoadFromFile(k_ColorQuadsPath));
			const bool loaded = model && model->GetSubMeshCount() == 2;
			Check(loaded && NearColor(model->GetSubMeshes()[0].BaseColor, { 0.8f, 0.2f, 0.1f, 1.0f })
				&& NearColor(model->GetSubMeshes()[1].BaseColor, { 0.1f, 0.3f, 0.9f, 0.5f }),
				"a glTF submesh's BaseColor is its material's baseColorFactor, alpha included");
		}
		{
			std::unique_ptr<Model> model(Model::LoadFromFile("assets/models/ColorQuads/ColorQuads.obj"));
			Check(model && model->GetSubMeshCount() == 1 && NearColor(model->GetSubMeshes()[0].BaseColor, { 0.2f, 0.7f, 0.3f, 0.75f }),
				"an OBJ submesh's BaseColor falls back to the diffuse colour (Kd) and its opacity (d)");
		}
		{
			std::unique_ptr<Model> model(Model::LoadFromFile("assets/models/Duck/Duck.gltf"));
			Check(model && model->GetSubMeshCount() > 0 && model->GetSubMeshes()[0].DiffuseTexture
				&& NearColor(model->GetSubMeshes()[0].BaseColor, glm::vec4(1.0f)),
				"a textured submesh without a colour factor keeps a white BaseColor");
		}

		const std::filesystem::path folder = TestTempDirectory("DingoModel3DTest");
		const std::filesystem::path copy = folder / "ColorQuads.gltf";
		std::error_code error;
		std::filesystem::create_directories(folder, error);
		std::ifstream source(k_ColorQuadsPath, std::ios::binary);
		std::string text((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
		std::ofstream(copy, std::ios::binary | std::ios::trunc) << text;

		std::unique_ptr<Model> model(Model::LoadFromFile(copy));
		const size_t factor = text.find("0.8,");
		bool reloaded = false;
		if (model && factor != std::string::npos)
		{
			text.replace(factor, 4, "0.4,");
			std::ofstream(copy, std::ios::binary | std::ios::trunc) << text;
			reloaded = model->Reload();
		}
		const glm::vec4 color = model && model->GetSubMeshCount() > 0 ? model->GetSubMeshes()[0].BaseColor : glm::vec4(0.0f);
		Check(reloaded && NearColor(color, { 0.4f, 0.2f, 0.1f, 1.0f }),
			std::format("a model reload takes the file's new BaseColor (read {:.2f} {:.2f} {:.2f} {:.2f})", color.r, color.g, color.b, color.a));
		model.reset();
		std::filesystem::remove_all(folder, error);
	}

	void Model3DTest::FitCamera()
	{
		glm::vec3 minBounds((std::numeric_limits<float>::max)());
		glm::vec3 maxBounds((std::numeric_limits<float>::lowest)());
		for (const auto& sm : m_Model->GetSubMeshes())
		{
			for (const MeshVertex& v : sm.Mesh->GetVertices())
			{
				minBounds = (glm::min)(minBounds, v.Position);
				maxBounds = (glm::max)(maxBounds, v.Position);
			}
		}
		if (minBounds.x > maxBounds.x)
			return;

		m_Pivot = 0.5f * (minBounds + maxBounds);
		const float radius   = (std::max)(0.5f * glm::length(maxBounds - minBounds), 1e-3f);
		const float distance = 1.15f * radius / std::sin(glm::radians(0.5f * m_Camera.GetFOV()));

		m_Camera.SetClip(distance * 0.01f, distance * 4.0f);
		m_Camera.SetTarget(m_Pivot);
		m_Camera.SetPosition(m_Pivot + distance * glm::normalize(glm::vec3(0.0f, 0.375f, 1.0f)));
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
			gpu.IndexCount = sm.Mesh->GetIndexCount();

			const auto& verts   = sm.Mesh->GetVertices();
			const auto& indices = sm.Mesh->GetIndices();

			gpu.VB = GraphicsBuffer::CreateVertexBuffer(verts.size()   * sizeof(MeshVertex), nullptr, BufferUpload::Direct, "Model3D_VB");
			gpu.IB = GraphicsBuffer::CreateIndexBuffer(indices.size() * sizeof(uint32_t), IndexFormat::Uint32, nullptr, BufferUpload::Direct, "Model3D_IB");
			gpu.VB->Upload(verts.data(),   static_cast<uint64_t>(verts.size())   * sizeof(MeshVertex));
			gpu.IB->Upload(indices.data(), static_cast<uint64_t>(indices.size()) * sizeof(uint32_t));

			Texture* diffuse = sm.DiffuseTexture ? sm.DiffuseTexture : Renderer::GetWhiteTexture();

			gpu.Mat = Material::Create(MaterialParams()
				.SetDebugName("Model3D_Mat")
				.SetShader(m_Shader)
				.SetCullMode(CullMode::Back));
			gpu.Mat->SetTexture(0, diffuse);
			gpu.Mat->SetSampler(0, Renderer::GetClampSampler());

			m_GpuSubMeshes.push_back(gpu);
		}

		FitCamera();
	}

	void Model3DTest::UnloadModel()
	{
		for (auto& gsm : m_GpuSubMeshes)
		{
			DestroyAndDelete(gsm.Mat);
			DestroyAndDelete(gsm.VB);
			DestroyAndDelete(gsm.IB);
		}
		m_GpuSubMeshes.clear();

		DestroyAndDelete(m_Model);
	}

	void Model3DTest::Update(float deltaTime)
	{
		if (m_AutoRotate)
			m_Rotation += deltaTime * m_RotationSpeed;

		Renderer::Clear(m_ClearColor);

		if (m_GpuSubMeshes.empty())
			return;

		TransformUBO ubo;
		ubo.ViewProjection = m_Camera.GetViewProjectionMatrix();
		ubo.Model = glm::translate(glm::mat4(1.0f), glm::vec3(m_Pivot.x, 0.0f, m_Pivot.z))
			* glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation), glm::vec3(0.0f, 1.0f, 0.0f))
			* glm::translate(glm::mat4(1.0f), glm::vec3(-m_Pivot.x, 0.0f, -m_Pivot.z));

		for (auto& gsm : m_GpuSubMeshes)
		{
			gsm.Mat->SetUniform(ubo);
			Renderer::DrawIndexed(gsm.Mat, m_Layout, gsm.VB, gsm.IB, gsm.IndexCount);
		}
	}

	void Model3DTest::Cleanup()
	{
		UnloadModel();

		DestroyAndDelete(m_Shader);
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
		ImGui::SameLine();
		if (ImGui::Button("Fit camera") && m_Model)
			FitCamera();

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

		ImGui::Text("Checks");
		for (const CheckResult& check : m_Checks)
		{
			ImGui::TextColored(check.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
				"[%s] %s", check.Passed ? "PASS" : "FAIL", check.Name.c_str());
		}
		ImGui::Separator();

		ImGui::Text("SubMeshes: %u", m_Model->GetSubMeshCount());
		const auto& submeshes = m_Model->GetSubMeshes();
		for (uint32_t i = 0; i < static_cast<uint32_t>(submeshes.size()); ++i)
		{
			const auto& sm = submeshes[i];
			ImGui::Text("  [%u] verts=%u  idx=%u  tex=%s  skin=%s  color=%.2f %.2f %.2f %.2f", i,
				sm.Mesh->GetVertexCount(),
				sm.Mesh->GetIndexCount(),
				sm.DiffuseTexture ? "yes" : "no",
				sm.Mesh->HasSkin() ? "yes" : "no",
				sm.BaseColor.r, sm.BaseColor.g, sm.BaseColor.b, sm.BaseColor.a);
		}

		if (const Skeleton* skeleton = m_Model->GetSkeleton())
		{
			ImGui::Separator();
			if (ImGui::TreeNode("Skeleton", "Skeleton: %u joints", skeleton->GetJointCount()))
			{
				for (uint32_t j = 0; j < skeleton->GetJointCount(); ++j)
				{
					const Joint& joint = skeleton->GetJoint(j);
					int depth = 0;
					for (int32_t p = joint.Parent; p >= 0; p = skeleton->GetJoint(p).Parent)
						++depth;
					ImGui::Text("%*s%s", depth * 2, "", joint.Name.c_str());
				}
				ImGui::TreePop();
			}

			ImGui::Text("Clips: %u", m_Model->GetAnimationCount());
			for (uint32_t c = 0; c < m_Model->GetAnimationCount(); ++c)
			{
				const AnimationClip* clip = m_Model->GetAnimation(c);
				ImGui::Text("  %s  %.2f s  %zu channels", clip->GetName().c_str(), clip->GetDuration(), clip->GetChannels().size());
			}
		}

		ImGui::Separator();
		ImGui::Text("Camera");

		glm::vec3 pos = m_Camera.GetPosition();
		if (ImGui::DragFloat3("Position", glm::value_ptr(pos), 0.1f))
			m_Camera.SetPosition(pos);

		float fov = m_Camera.GetFOV();
		if (ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f))
			m_Camera.SetFOV(fov);

		ImGui::Separator();
		ImGui::Text("Rotation");

		ImGui::Checkbox("Auto Rotate", &m_AutoRotate);
		ImGui::SliderFloat("Speed", &m_RotationSpeed, -360.0f, 360.0f);
		if (!m_AutoRotate)
			ImGui::SliderFloat("Angle", &m_Rotation, 0.0f, 360.0f);
	}

}
