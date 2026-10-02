#include "AnimationTest.h"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <limits>

namespace
{
	using namespace Dingo;

	using Clock = std::chrono::steady_clock;

	constexpr const char* k_FoxPath = "assets/models/Fox/Fox.gltf";
	constexpr float k_FoxLength = 1.6f;
	constexpr float k_CrowdSpacing = 2.2f;
	constexpr float k_WarmupSeconds = 2.0f;
	constexpr uint32_t k_MeasuredFrames = 120;

	double Milliseconds(Clock::time_point from, Clock::time_point to)
	{
		return std::chrono::duration<double, std::milli>(to - from).count();
	}

	const SubMesh* FindSkinnedSubMesh(const Model& model)
	{
		for (const SubMesh& submesh : model.GetSubMeshes())
		{
			if (submesh.MeshData->HasSkin())
				return &submesh;
		}
		return nullptr;
	}

	glm::quat Turn(float degrees, const glm::vec3& axis)
	{
		return glm::angleAxis(glm::radians(degrees), axis);
	}
}

namespace Dingo
{

	void AnimationTest::Check(bool condition, const std::string& name)
	{
		m_Checks.push_back({ name, condition });
		if (condition)
			DE_INFO("[PASS] {}", name);
		else
			DE_ERROR("[FAIL] {}", name);
	}

	void AnimationTest::Initialize()
	{
		m_Checks.clear();
		m_DrawChecksDone = false;
		m_Time = 0.0f;
		m_TimedFrames = 0;
		m_FrameMs = m_RenderMs = m_EndSceneMs = 0.0;
		m_TimingResult.clear();

		if (!m_ArgsRead)
		{
			m_ArgsRead = true;
			const ApplicationCommandLineArgs& args = Application::Get().GetCommandLineArgs();
			if (auto mode = args.Get("anim"))
			{
				m_Mode = *mode == "bindstatic" ? Mode::BindStatic : *mode == "pose" ? Mode::Pose
					: *mode == "crowd" ? Mode::Crowd : Mode::Bind;
				if (m_Mode == Mode::Bind && *mode != "bind")
					DE_WARN("Animation Test: unknown --anim={}; showing bind. Use bind, bindstatic, pose or crowd.", *mode);
			}
			if (auto count = args.Get("anim-count"); count && !count->empty())
				m_CrowdCount = static_cast<uint32_t>((std::max)(1l, std::strtol(std::string(*count).c_str(), nullptr, 10)));
			m_CrowdStatic = args.Get("anim-static").has_value();
		}

		m_Camera = PerspectiveCamera(45.0f, m_AspectRatio, 0.05f, 200.0f);

		m_Fox = Model::LoadFromFile(k_FoxPath);
		RunLoadChecks();
		if (!m_Fox || !FindSkinnedSubMesh(*m_Fox))
			return;

		glm::vec3 minBounds((std::numeric_limits<float>::max)());
		glm::vec3 maxBounds((std::numeric_limits<float>::lowest)());
		for (const SubMesh& submesh : m_Fox->GetSubMeshes())
		{
			for (const MeshVertex& vertex : submesh.MeshData->GetVertices())
			{
				minBounds = (glm::min)(minBounds, vertex.Position);
				maxBounds = (glm::max)(maxBounds, vertex.Position);
			}
		}
		const glm::vec3 extent = maxBounds - minBounds;
		m_FoxScale = k_FoxLength / (std::max)({ extent.x, extent.y, extent.z });
		const glm::vec3 center = 0.5f * (minBounds + maxBounds);
		m_FoxOffset = -glm::vec3(center.x, minBounds.y, center.z) * m_FoxScale;
		m_FoxCenter = glm::vec3(0.0f, 0.5f * extent.y * m_FoxScale, 0.0f);
		m_FoxRadius = 0.5f * glm::length(extent) * m_FoxScale;

		Renderer3D& renderer = Application::Get().GetRenderer3D();
		m_FoxMaterial = renderer.CreateLitMaterial(MaterialParams().SetDebugName("Fox"));
		if (Texture* diffuse = FindSkinnedSubMesh(*m_Fox)->DiffuseTexture)
			m_FoxMaterial->SetTexture(0, diffuse);

		BuildScene();
	}

	void AnimationTest::RunLoadChecks()
	{
		Check(m_Fox && m_Fox->IsSkinned(), "Fox.gltf loads with a skeleton");
		if (!m_Fox || !m_Fox->IsSkinned())
			return;

		const Skeleton& skeleton = *m_Fox->GetSkeleton();
		Check(skeleton.GetJointCount() == 24 && skeleton.GetSkinJointCount() == 24, "24 joints, all reachable by the skin");
		Check(m_Fox->GetAnimationCount() == 3 && m_Fox->FindAnimation("Survey") && m_Fox->FindAnimation("Walk") && m_Fox->FindAnimation("Run"),
			"3 clips: Survey, Walk, Run");

		float worstPalette = 0.0f;
		for (const glm::mat4& matrix : skeleton.GetRestPalette())
		{
			for (int column = 0; column < 4; ++column)
				for (int row = 0; row < 4; ++row)
					worstPalette = (std::max)(worstPalette, std::abs(matrix[column][row] - (column == row ? 1.0f : 0.0f)));
		}
		Check(skeleton.GetRestPalette().size() == 24 && worstPalette < 1e-3f, std::format("the rest palette is identity (worst {:.1e})", worstPalette));

		const SubMesh* skinned = FindSkinnedSubMesh(*m_Fox);
		Check(skinned && skinned->MeshData->GetSkinJointCount() == 24 && skinned->MeshData->GetSkinJointCount() <= Renderer3D::k_MaxSkinJoints,
			"the skin reads 24 palette entries, within k_MaxSkinJoints");
		if (!skinned)
			return;

		float worstWeights = 0.0f;
		for (const SkinnedMeshVertex& vertex : skinned->MeshData->GetSkinVertices())
			worstWeights = (std::max)(worstWeights, std::abs(vertex.Weights.x + vertex.Weights.y + vertex.Weights.z + vertex.Weights.w - 1.0f));
		Check(worstWeights < 1e-5f, "every skin vertex's weights sum to 1");
	}

	void AnimationTest::BuildScene()
	{
		m_Scene = new Scene("Animation Test");
		Renderer3D& renderer = Application::Get().GetRenderer3D();

		Entity floor = m_Scene->CreateEntity("Floor");
		floor.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, -0.05f, 0.0f }, { 40.0f, 0.1f, 40.0f }));
		floor.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetBoxMesh(), { 0.42f, 0.46f, 0.40f, 1.0f }));

		const SubMesh* skinned = FindSkinnedSubMesh(*m_Fox);
		const uint32_t count = m_Mode == Mode::Crowd ? m_CrowdCount : (m_Mode == Mode::Pose ? 0u : 1u);
		const uint32_t columns = (std::max)(1u, static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(count)))));
		const uint32_t rows = (count + columns - 1) / columns;
		for (uint32_t i = 0; i < count; ++i)
		{
			const float x = (static_cast<float>(i % columns) - 0.5f * static_cast<float>(columns - 1)) * k_CrowdSpacing;
			const float z = (static_cast<float>(i / columns) - 0.5f * static_cast<float>(rows - 1)) * k_CrowdSpacing;

			Entity fox = m_Scene->CreateEntity(std::format("Fox {}", i));
			fox.AddComponent<Transform3DComponent>(Transform3DComponent(m_FoxOffset + glm::vec3(x, 0.0f, z), glm::vec3(m_FoxScale)));

			const bool drawStatic = m_Mode == Mode::BindStatic || (m_Mode == Mode::Crowd && m_CrowdStatic);
			if (drawStatic)
				fox.AddComponent<MeshRendererComponent>(MeshRendererComponent(skinned->MeshData)).Material = m_FoxMaterial;
			else
				fox.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(m_Fox)).Material = m_FoxMaterial;
		}

		if (m_Mode == Mode::Pose)
		{
			const Skeleton& skeleton = *m_Fox->GetSkeleton();
			std::vector<JointPose> poses;
			for (const Joint& joint : skeleton.GetJoints())
				poses.push_back(joint.RestPose);

			auto bend = [&](const char* name, float degrees, const glm::vec3& axis)
			{
				const int32_t joint = skeleton.FindJoint(name);
				if (joint != Skeleton::k_InvalidJoint)
					poses[joint].Rotation = poses[joint].Rotation * Turn(degrees, axis);
			};
			bend("b_Neck_04", 25.0f, { 0.0f, 1.0f, 0.0f });
			bend("b_Head_05", 30.0f, { 0.0f, 1.0f, 0.0f });
			bend("b_Tail01_012", 30.0f, { 0.0f, 1.0f, 0.0f });
			bend("b_Tail02_013", 30.0f, { 0.0f, 1.0f, 0.0f });
			bend("b_Tail03_014", 30.0f, { 0.0f, 1.0f, 0.0f });
			bend("b_RightUpperArm_06", -40.0f, { 0.0f, 0.0f, 1.0f });
			bend("b_LeftLeg01_015", 35.0f, { 0.0f, 0.0f, 1.0f });

			std::vector<glm::mat4> globals(skeleton.GetJointCount());
			skeleton.ComputeGlobalTransforms(poses, globals);
			m_PosePalette.resize(skeleton.GetSkinJointCount());
			skeleton.ComputeSkinningPalette(globals, m_PosePalette);

			m_PoseTransform = glm::translate(glm::mat4(1.0f), m_FoxOffset) * glm::scale(glm::mat4(1.0f), glm::vec3(m_FoxScale));
		}

		m_Scene->OnStart();

		if (m_Mode == Mode::Crowd)
		{
			const float half = 0.5f * static_cast<float>(columns) * k_CrowdSpacing;
			m_Camera.SetPosition({ -0.9f * half - 2.0f, 1.1f * half + 1.5f, 1.4f * half + 2.5f });
			m_Camera.SetTarget({ 0.0f, 0.0f, 0.0f });
		}
		else
		{
			const float distance = 1.25f * m_FoxRadius / std::sin(glm::radians(0.5f * m_Camera.GetFOV()));
			m_Camera.SetPosition(m_FoxCenter + distance * glm::normalize(glm::vec3(-0.75f, 0.35f, 1.0f)));
			m_Camera.SetTarget(m_FoxCenter);
		}
	}

	void AnimationTest::DestroyScene()
	{
		delete m_Scene;
		m_Scene = nullptr;
	}

	void AnimationTest::SubmitPosedFox(Renderer3D& renderer)
	{
		const SubMesh* skinned = FindSkinnedSubMesh(*m_Fox);
		renderer.SubmitSkinnedMesh(skinned->MeshData, m_PoseTransform, m_PosePalette, glm::vec4(1.0f), m_FoxMaterial);
	}

	void AnimationTest::RunDrawChecks(const Renderer3D::Statistics& stats)
	{
		const uint32_t budget = Application::Get().GetRenderer3D().GetSkinnedInstanceBudget();
		switch (m_Mode)
		{
			case Mode::Bind:
				Check(stats.SkinnedDraws == 1 && stats.SkinnedInstances == 1 && stats.SkinnedJoints == 24 && stats.DroppedSkinnedDraws == 0, "the bind-pose Fox is one skinned draw of 24 joints");
				break;
			case Mode::BindStatic:
				Check(stats.SkinnedDraws == 0, "the static Fox takes the batched path");
				break;
			case Mode::Pose:
				Check(stats.SkinnedDraws == 1 && stats.SkinnedJoints == 24 && stats.DroppedSkinnedDraws == 0, "the posed Fox is one skinned draw of 24 joints");
				break;
			case Mode::Crowd:
				if (m_CrowdStatic)
				{
					Check(stats.SkinnedDraws == 0, std::format("{} static foxes take the batched path", m_CrowdCount));
					break;
				}
				const uint32_t expected = (std::min)(m_CrowdCount, budget);
				Check(stats.SkinnedDraws == expected && stats.DroppedSkinnedDraws == m_CrowdCount - expected,
					std::format("{} foxes: {} skinned draws and {} dropped past MaxSkinnedInstances ({})", m_CrowdCount, expected, m_CrowdCount - expected, budget));
				break;
		}
	}

	void AnimationTest::TrackTiming(float deltaTime, double renderMs, double endSceneMs)
	{
		m_Time += deltaTime;
		if (m_Mode != Mode::Crowd || m_Time < k_WarmupSeconds || m_TimedFrames >= k_MeasuredFrames)
			return;

		m_FrameMs += deltaTime * 1000.0;
		m_RenderMs += renderMs;
		m_EndSceneMs += endSceneMs;
		if (++m_TimedFrames < k_MeasuredFrames)
			return;

		const double frames = static_cast<double>(m_TimedFrames);
		m_TimingResult = std::format("{} foxes, {}: frame {:.2f} ms, RenderEntities3D {:.3f} ms, EndScene recording {:.3f} ms (mean of {} frames)",
			m_CrowdCount, m_CrowdStatic ? "static" : "skinned", m_FrameMs / frames, m_RenderMs / frames, m_EndSceneMs / frames, m_TimedFrames);
		DE_INFO("[Anim] {}", m_TimingResult);
	}

	void AnimationTest::Update(float deltaTime)
	{
		Renderer3D& renderer = Application::Get().GetRenderer3D();
		renderer.BeginScene(m_Camera);
		renderer.Clear(m_ClearColor);
		if (!m_Scene)
		{
			renderer.EndScene();
			return;
		}

		m_Scene->OnUpdate((std::min)(deltaTime, 1.0f / 30.0f));
		m_Scene->SubmitLights(renderer);

		const Clock::time_point renderStart = Clock::now();
		m_Scene->RenderEntities3D(renderer);
		if (m_Mode == Mode::Pose)
			SubmitPosedFox(renderer);
		const Clock::time_point endSceneStart = Clock::now();
		renderer.EndScene();
		const Clock::time_point endSceneEnd = Clock::now();

		m_LastStats = renderer.GetStatistics();
		if (!m_DrawChecksDone)
		{
			m_DrawChecksDone = true;
			RunDrawChecks(m_LastStats);
		}
		TrackTiming(deltaTime, Milliseconds(renderStart, endSceneStart), Milliseconds(endSceneStart, endSceneEnd));
	}

	void AnimationTest::Cleanup()
	{
		DestroyScene();
		DestroyAndDelete(m_FoxMaterial);
		DestroyAndDelete(m_Fox);
		m_PosePalette.clear();
	}

	void AnimationTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
		m_Camera.SetAspectRatio(m_AspectRatio);
	}

	void AnimationTest::ImGuiRender()
	{
		GraphicsTest::ImGuiRender();
		ImGui::Separator();

		int mode = static_cast<int>(m_Mode);
		ImGui::RadioButton("Bind", &mode, static_cast<int>(Mode::Bind));
		ImGui::SameLine();
		ImGui::RadioButton("Bind (static)", &mode, static_cast<int>(Mode::BindStatic));
		ImGui::SameLine();
		ImGui::RadioButton("Pose", &mode, static_cast<int>(Mode::Pose));
		ImGui::SameLine();
		ImGui::RadioButton("Crowd", &mode, static_cast<int>(Mode::Crowd));
		if (mode != static_cast<int>(m_Mode))
		{
			m_Mode = static_cast<Mode>(mode);
			Cleanup();
			Initialize();
			return;
		}

		ImGui::Text("Skinned draws %u, instances %u, dropped %u, joints %u, draw calls %u",
			m_LastStats.SkinnedDraws, m_LastStats.SkinnedInstances, m_LastStats.DroppedSkinnedDraws, m_LastStats.SkinnedJoints, m_LastStats.DrawCalls);
		if (m_Mode == Mode::Crowd)
			ImGui::TextWrapped("Timing: %s", m_TimingResult.empty() ? "measuring..." : m_TimingResult.c_str());

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : ImVec4(0.95f, 0.3f, 0.3f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

}
