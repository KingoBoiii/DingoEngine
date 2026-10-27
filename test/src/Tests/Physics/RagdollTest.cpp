#include "RagdollTest.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <set>

namespace
{
	using namespace Dingo;

	constexpr const char* k_FoxPath = "models/Fox/Fox.gltf";
	constexpr float k_FoxLength = 1.6f;
	constexpr float k_Dt = 1.0f / 60.0f;

	const char* const k_ExpectedBodies[] = {
		"b_Hip_01", "b_Spine01_02", "b_Spine02_03", "b_Neck_04",
		"b_RightUpperArm_06", "b_RightForeArm_07", "b_LeftUpperArm_09", "b_LeftForeArm_010",
		"b_Tail01_012", "b_Tail02_013",
		"b_LeftLeg01_015", "b_LeftLeg02_016", "b_LeftFoot01_017",
		"b_RightLeg01_019", "b_RightLeg02_020", "b_RightFoot01_021",
	};

	void Run(Scene& scene, float seconds)
	{
		const int steps = static_cast<int>(seconds / k_Dt);
		for (int i = 0; i < steps; i++)
			scene.OnUpdate(k_Dt);
	}

	bool Finite(const glm::vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

	size_t DistinctBodies(const Scene& scene, Entity entity, const Skeleton& skeleton)
	{
		std::set<PhysicsBodyId3D> bodies;
		for (size_t i = 0; i < skeleton.GetJointCount(); i++)
		{
			const PhysicsBodyId3D body = scene.GetRagdollBody(entity, skeleton.GetJoint(static_cast<int32_t>(i)).Name);
			if (body != k_InvalidBody3D)
				bodies.insert(body);
		}
		return bodies.size();
	}
}

namespace Dingo
{

	void RagdollTest::RunMassChecks()
	{
		std::unique_ptr<Physics3D> world(Physics3D::Create());
		world->Initialize(Physics3DParams());
		RigidBodyParams3D params;
		params.Type = BodyType3D::Dynamic;
		params.Shape = ColliderShape3D::Box;
		params.HalfExtents = glm::vec3(0.5f);
		params.Mass = 10.0f;
		const PhysicsBodyId3D light = world->CreateBody(params);
		params.Mass = 0.0f;
		params.Position = { 5.0f, 0.0f, 0.0f };
		const PhysicsBodyId3D dense = world->CreateBody(params);
		world->ApplyImpulse(light, { 0.0f, 0.0f, 10.0f });
		world->ApplyImpulse(dense, { 0.0f, 0.0f, 10.0f });
		Check(std::abs(world->GetLinearVelocity(light).z - 1.0f) < 1e-3f, "mass: RigidBodyParams3D::Mass sets a dynamic body's mass");
		Check(std::abs(world->GetLinearVelocity(dense).z - 0.01f) < 1e-4f, "mass: 0 keeps the shape's mass at 1000 kg/m^3");
		world->Shutdown();
	}

	Entity RagdollTest::AddFox(const std::string& name, const glm::vec3& position, bool animated)
	{
		Entity fox = m_Scene->CreateEntity(name);
		fox.AddComponent<Transform3DComponent>(Transform3DComponent(position + glm::vec3(0.0f, m_FoxLift, 0.0f), glm::vec3(m_FoxScale)));
		fox.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(m_Fox));
		if (animated)
			fox.AddComponent<AnimatorComponent>(AnimatorComponent("Survey"));
		return fox;
	}

	void RagdollTest::BuildScene()
	{
		m_Scene = new Scene("Ragdoll Test");
		m_Scene->SetClearColor({ 0.05f, 0.05f, 0.07f, 1.0f });

		Entity floor = m_Scene->CreateEntity("Floor");
		floor.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, -0.5f, 0.0f }, { 20.0f, 1.0f, 20.0f }));
		floor.AddComponent<MeshRendererComponent>(MeshRendererComponent(Application::Get().GetRenderer3D().GetBoxMesh(), { 0.4f, 0.4f, 0.4f, 1.0f }));
		floor.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));

		Entity light = m_Scene->CreateEntity("Sun");
		light.AddComponent<Transform3DComponent>().Rotation = glm::quatLookAt(glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f)), glm::vec3(0.0f, 1.0f, 0.0f));
		light.AddComponent<DirectionalLightComponent>();

		m_Limp = AddFox("Limp Fox", { -1.0f, 0.3f, 0.0f }, false);
		RagdollComponent limp;
		limp.DriveToAnimation = false;
		m_Limp.AddComponent<RagdollComponent>(limp);

		m_Driven = AddFox("Driven Fox", { 1.0f, 0.3f, 0.0f }, false);
		m_Driven.AddComponent<RagdollComponent>();
	}

	void RagdollTest::RunSceneChecks()
	{
		const Skeleton& skeleton = *m_Fox->GetSkeleton();
		Check(m_Scene->GetPhysics3D() != nullptr, "scene: ragdolls alone start the 3D world");

		size_t named = 0;
		for (const char* joint : k_ExpectedBodies)
			named += m_Scene->GetRagdollBody(m_Limp, joint) != k_InvalidBody3D ? 1 : 0;
		Check(named == std::size(k_ExpectedBodies) && DistinctBodies(*m_Scene, m_Limp, skeleton) == std::size(k_ExpectedBodies),
			"build: the Fox gets its 16 bodies (hip, spine, neck, legs, arms, tail)");
		Check(m_Scene->GetRagdollBody(m_Limp, "b_Head_05") == m_Scene->GetRagdollBody(m_Limp, "b_Neck_04")
			&& m_Scene->GetRagdollBody(m_Limp, "b_RightHand_08") == m_Scene->GetRagdollBody(m_Limp, "b_RightForeArm_07"),
			"GetRagdollBody: a joint without a body finds its nearest simulated ancestor's");
		Check(m_Scene->GetRagdollBody(m_Limp, "no such joint") == k_InvalidBody3D && m_Scene->GetRagdollBody(m_Limp, "b_Root_00") == k_InvalidBody3D,
			"GetRagdollBody: an unknown joint or one above the root body has none");

		const PhysicsBodyId3D limpHip = m_Scene->GetRagdollBody(m_Limp, "b_Hip_01");
		const PhysicsBodyId3D drivenHip = m_Scene->GetRagdollBody(m_Driven, "b_Hip_01");
		Check(m_Scene->GetEntityFromBody3D(limpHip) == m_Limp && m_Scene->GetEntityFromBody3D(m_Scene->GetRagdollBody(m_Limp, "b_LeftFoot01_017")) == m_Limp,
			"GetEntityFromBody3D: every ragdoll body belongs to its entity");

		Physics3D& physics = *m_Scene->GetPhysics3D();
		const glm::vec3 limpStart = physics.GetPosition(limpHip);
		const glm::vec3 drivenStart = physics.GetPosition(drivenHip);
		const glm::vec3 limpEntityStart = m_Limp.GetComponent<Transform3DComponent>().Position;
		Run(*m_Scene, 3.0f);

		bool finite = true;
		for (const char* joint : k_ExpectedBodies)
		{
			finite = finite && Finite(physics.GetPosition(m_Scene->GetRagdollBody(m_Limp, joint)));
			finite = finite && Finite(physics.GetPosition(m_Scene->GetRagdollBody(m_Driven, joint)));
		}
		Check(finite, "simulate: 3 s of two ragdolls on a floor stay finite");

		const glm::vec3 limpEnd = physics.GetPosition(limpHip);
		const glm::vec3 drivenEnd = physics.GetPosition(drivenHip);
		Check(limpEnd.y < limpStart.y - 0.05f && limpEnd.y > -0.1f, "limp: the hip falls and comes to rest on the floor");
		Check(glm::length(drivenEnd - drivenStart) < 0.05f, "driven: the hip holds the animated pose");
		Check(m_Limp.GetComponent<Transform3DComponent>().Position.y < limpEntityStart.y - 0.05f,
			"limp: the entity follows its root body down");

		Animator* survey = nullptr;
		{
			Entity animated = AddFox("Animated Fox", { 3.0f, 0.3f, 0.0f }, true);
			Entity blendZero = AddFox("Blend 0 Fox", { 5.0f, 0.3f, 0.0f }, true);
			RagdollComponent settings;
			settings.Blend = 0.0f;
			settings.DriveToAnimation = false;
			blendZero.AddComponent<RagdollComponent>(settings);
			Run(*m_Scene, 0.5f);

			survey = m_Scene->GetAnimator(animated);
			const Animator* zero = m_Scene->GetAnimator(blendZero);
			bool same = survey && zero && survey->GetSkinningPalette().size() == zero->GetSkinningPalette().size();
			for (size_t i = 0; same && i < survey->GetSkinningPalette().size(); i++)
				same = survey->GetSkinningPalette()[i] == zero->GetSkinningPalette()[i];
			Check(same && m_Scene->GetRagdollBody(blendZero, "b_Hip_01") != k_InvalidBody3D,
				"Blend 0: the ragdoll simulates but the palette is the animator's, bit for bit");

			blendZero.GetComponent<RagdollComponent>().Blend = 1.0f;
			Run(*m_Scene, 1.0f);
			const glm::vec3 posed = glm::vec3(survey->GetGlobalTransforms()[skeleton.FindJoint("b_Head_05")][3]);
			const glm::vec3 fallen = glm::vec3(zero->GetGlobalTransforms()[skeleton.FindJoint("b_Head_05")][3]);
			Check(glm::length(posed - fallen) > 1e-3f * glm::length(posed), "Blend 1: the limp bones replace the animated pose");

			const PhysicsBodyId3D hip = m_Scene->GetRagdollBody(blendZero, "b_Hip_01");
			blendZero.RemoveComponent<RagdollComponent>();
			Check(!physics.IsBodyValid(hip) && m_Scene->GetRagdollBody(blendZero, "b_Hip_01") == k_InvalidBody3D,
				"lifecycle: removing the RagdollComponent destroys its bodies");
			m_Scene->DestroyEntity(blendZero);
			m_Scene->DestroyEntity(animated);
		}

		m_Limp.SetActive(false);
		Check(!physics.IsBodyValid(limpHip) && m_Scene->GetRagdollBody(m_Limp, "b_Hip_01") == k_InvalidBody3D,
			"lifecycle: deactivating the entity destroys its ragdoll");
		m_Limp.SetActive(true);
		m_Scene->OnUpdate(k_Dt);
		Check(DistinctBodies(*m_Scene, m_Limp, skeleton) == std::size(k_ExpectedBodies), "lifecycle: reactivating rebuilds the ragdoll");

		{
			Entity extra = AddFox("Destroyed Fox", { -3.0f, 0.3f, 0.0f }, false);
			extra.AddComponent<RagdollComponent>();
			m_Scene->OnUpdate(k_Dt);
			const PhysicsBodyId3D hip = m_Scene->GetRagdollBody(extra, "b_Hip_01");
			m_Scene->DestroyEntity(extra);
			m_Scene->OnUpdate(k_Dt);
			Check(hip != k_InvalidBody3D && !physics.IsBodyValid(hip), "lifecycle: a ragdoll added at runtime builds, and goes with its entity");
		}

		m_Scene->OnStop();
		Check(m_Scene->GetRagdollBody(m_Driven, "b_Hip_01") == k_InvalidBody3D, "lifecycle: OnStop drops every ragdoll");
		m_Scene->OnStart();
		Check(DistinctBodies(*m_Scene, m_Driven, skeleton) == std::size(k_ExpectedBodies), "lifecycle: OnStart builds them again");
	}

	void RagdollTest::Initialize()
	{
		m_Checks.clear();
		RunMassChecks();

		m_Fox = Model::LoadFromFile(k_FoxPath);
		Check(m_Fox && m_Fox->GetSkeleton(), "the Fox loads with a skeleton");
		if (!m_Fox || !m_Fox->GetSkeleton())
			return;

		glm::vec3 low((std::numeric_limits<float>::max)());
		glm::vec3 high((std::numeric_limits<float>::lowest)());
		for (const SubMesh& submesh : m_Fox->GetSubMeshes())
		{
			for (const MeshVertex& vertex : submesh.Mesh->GetVertices())
			{
				low = (glm::min)(low, vertex.Position);
				high = (glm::max)(high, vertex.Position);
			}
		}
		const glm::vec3 extent = high - low;
		m_FoxScale = k_FoxLength / (std::max)({ extent.x, extent.y, extent.z });
		m_FoxLift = -low.y * m_FoxScale;

		BuildScene();
		m_Scene->OnStart();
		RunSceneChecks();

		m_Camera = PerspectiveCamera(45.0f, m_AspectRatio, 0.1f, 200.0f);
		m_Camera.SetPosition({ 0.0f, 1.5f, 5.0f });
		m_Camera.SetTarget({ 0.0f, 0.4f, 0.0f });
	}

	void RagdollTest::Update(float deltaTime)
	{
		if (!m_Scene)
			return;

		m_Scene->OnUpdate((std::min)(deltaTime, 1.0f / 30.0f));

		Renderer3D& renderer = Application::Get().GetRenderer3D();
		renderer.BeginScene(m_Camera);
		renderer.Clear(m_ClearColor);
		m_Scene->SubmitLights(renderer);
		m_Scene->RenderEntities3D(renderer);
		renderer.EndScene();
	}

	void RagdollTest::Cleanup()
	{
		delete m_Scene;
		m_Scene = nullptr;
		m_Limp = {};
		m_Driven = {};
		delete m_Fox;
		m_Fox = nullptr;
	}

	void RagdollTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
		m_Camera.SetAspectRatio(m_AspectRatio);
	}

	void RagdollTest::ImGuiRender()
	{
		GraphicsTest::ImGuiRender();
		ImGui::Separator();
		if (ImGui::Button("Reset"))
		{
			Cleanup();
			Initialize();
			return;
		}

		if (m_Driven.IsValid() && m_Driven.HasComponent<RagdollComponent>())
		{
			RagdollComponent& ragdoll = m_Driven.GetComponent<RagdollComponent>();
			ImGui::Separator();
			ImGui::TextUnformatted("Driven fox");
			ImGui::SliderFloat("Strength", &ragdoll.Strength, 0.0f, 2.0f);
			ImGui::SliderFloat("Blend", &ragdoll.Blend, 0.0f, 1.0f);
			ImGui::Checkbox("Drive to animation", &ragdoll.DriveToAnimation);
		}

		ImGui::Separator();
		for (const CheckResult& result : m_Checks)
			ImGui::TextColored(result.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s %s", result.Passed ? "PASS" : "FAIL", result.Name.c_str());
	}

}
