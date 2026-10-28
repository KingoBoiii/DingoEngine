#include "RagdollTest.h"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <set>
#include <vector>

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

	// For each expected body below the root, its joint and the nearest expected body joint above it.
	std::vector<std::pair<int32_t, int32_t>> BodyLinks(const Skeleton& skeleton)
	{
		std::set<int32_t> simulated;
		for (const char* joint : k_ExpectedBodies)
		{
			const int32_t index = skeleton.FindJoint(joint);
			if (index >= 0)
				simulated.insert(index);
		}

		std::vector<std::pair<int32_t, int32_t>> links;
		for (const int32_t joint : simulated)
		{
			int32_t parent = skeleton.GetJoint(static_cast<uint32_t>(joint)).Parent;
			while (parent >= 0 && !simulated.contains(parent))
				parent = skeleton.GetJoint(static_cast<uint32_t>(parent)).Parent;
			if (parent >= 0)
				links.emplace_back(joint, parent);
		}
		return links;
	}

	std::vector<float> LinkLengths(const Animator& animator, const std::vector<std::pair<int32_t, int32_t>>& links)
	{
		std::vector<float> lengths;
		for (const auto& [joint, parent] : links)
			lengths.push_back(glm::length(glm::vec3(animator.GetGlobalTransforms()[joint][3]) - glm::vec3(animator.GetGlobalTransforms()[parent][3])));
		return lengths;
	}

	// Eight 0.18 m steps of 0.35 m, falling toward +z from a 1.44 m landing, 2 m wide, at the origin.
	Mesh* MakeStairs(float& topHeight, float& length)
	{
		constexpr int k_Steps = 8;
		constexpr float k_Rise = 0.18f;
		constexpr float k_Tread = 0.35f;
		constexpr float k_Landing = 1.0f;
		constexpr float k_HalfWidth = 1.0f;
		topHeight = k_Steps * k_Rise;
		length = k_Landing + (k_Steps - 1) * k_Tread;

		std::vector<MeshVertex> vertices;
		std::vector<uint32_t> indices;
		// Counter-clockwise seen from the side it faces: collision triangles are one-sided.
		auto quad = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 normal)
		{
			const uint32_t base = static_cast<uint32_t>(vertices.size());
			for (const glm::vec3& corner : { a, b, c, d })
				vertices.push_back({ corner, normal, { 0.0f, 0.0f } });
			indices.insert(indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
		};

		float z = 0.0f;
		for (int i = 0; i < k_Steps; i++)
		{
			const float y = (k_Steps - i) * k_Rise;
			const float tread = i == 0 ? k_Landing : k_Tread;
			quad({ -k_HalfWidth, y, z }, { -k_HalfWidth, y, z + tread }, { k_HalfWidth, y, z + tread }, { k_HalfWidth, y, z }, { 0.0f, 1.0f, 0.0f });
			z += tread;
			quad({ -k_HalfWidth, y - k_Rise, z }, { k_HalfWidth, y - k_Rise, z }, { k_HalfWidth, y, z }, { -k_HalfWidth, y, z }, { 0.0f, 0.0f, 1.0f });
		}
		return Mesh::Create(vertices, indices);
	}

	Entity AddFloor(Scene& scene)
	{
		Entity floor = scene.CreateEntity("Floor");
		floor.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, -0.5f, 0.0f }, { 40.0f, 1.0f, 40.0f }));
		floor.AddComponent<MeshRendererComponent>(MeshRendererComponent(Application::Get().GetRenderer3D().GetBoxMesh(), { 0.4f, 0.4f, 0.4f, 1.0f }));
		floor.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
		return floor;
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

	Entity RagdollTest::AddFox(const std::string& name, const glm::vec3& position, bool animated, Model* model)
	{
		return AddFox(*m_Scene, name, position, animated, model);
	}

	Entity RagdollTest::AddFox(Scene& scene, const std::string& name, const glm::vec3& position, bool animated, Model* model)
	{
		Entity fox = scene.CreateEntity(name);
		fox.AddComponent<Transform3DComponent>(Transform3DComponent(position + glm::vec3(0.0f, m_FoxLift, 0.0f), glm::vec3(m_FoxScale)));
		fox.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(model ? model : m_Fox));
		if (animated)
			fox.AddComponent<AnimatorComponent>(AnimatorComponent("Survey"));
		return fox;
	}

	void RagdollTest::BuildScene()
	{
		m_Scene = new Scene("Ragdoll Test");
		m_Scene->SetClearColor({ 0.05f, 0.05f, 0.07f, 1.0f });

		AddFloor(*m_Scene);

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

	void RagdollTest::RunRecipeChecks()
	{
		const std::filesystem::path source = Application::Get().GetAssetManager().GetRootDirectory() / std::filesystem::path(k_FoxPath).parent_path();
		const std::filesystem::path folder = TestTempDirectory("DingoRagdollTest");
		std::error_code error;
		std::filesystem::remove_all(folder, error);
		std::filesystem::create_directories(folder, error);
		bool copied = true;
		for (const char* file : { "Fox.gltf", "Fox.bin", "Texture.png" })
			copied = copied && std::filesystem::copy_file(source / file, folder / file, std::filesystem::copy_options::overwrite_existing, error);
		auto writeRecipes = [&](const char* text)
		{
			std::ofstream out(folder / "Fox.ragdoll", std::ios::binary | std::ios::trunc);
			out << text;
		};
		writeRecipes("exclude b_Tail02_013\n[heavy]\nb_Hip_01 mass=0.5\nb_Neck_04 shape=sphere\n");
		m_RecipeFox = copied ? Model::LoadFromFile(folder / "Fox.gltf") : nullptr;
		Check(m_RecipeFox && m_RecipeFox->GetRagdollRecipes().size() == 2 && m_RecipeFox->FindRagdollRecipe("heavy"),
			"recipe: the Fox's .ragdoll sidecar loads its default and [heavy] recipes");
		if (!m_RecipeFox || !m_RecipeFox->GetSkeleton())
			return;

		const Skeleton& skeleton = *m_RecipeFox->GetSkeleton();
		Entity fox = AddFox("Recipe Fox", { 0.0f, 0.3f, -3.0f }, false, m_RecipeFox);
		RagdollComponent settings;
		settings.DriveToAnimation = false;
		fox.AddComponent<RagdollComponent>(settings);
		m_Scene->OnUpdate(k_Dt);
		Check(DistinctBodies(*m_Scene, fox, skeleton) == std::size(k_ExpectedBodies) - 1
			&& m_Scene->GetRagdollBody(fox, "b_Tail02_013") == m_Scene->GetRagdollBody(fox, "b_Tail01_012"),
			"recipe: the default recipe's exclude leaves the tail tip on the body above");

		Physics3D& physics = *m_Scene->GetPhysics3D();
		// The velocity an impulse of `kilograms` (in kg m/s) gives the hip: 1 m/s when that is its mass.
		auto hipKick = [&](float kilograms)
		{
			const PhysicsBodyId3D hip = m_Scene->GetRagdollBody(fox, "b_Hip_01");
			if (hip == k_InvalidBody3D)
				return 0.0f;
			const float before = physics.GetLinearVelocity(hip).z;
			physics.ApplyImpulse(hip, { 0.0f, 0.0f, kilograms });
			return physics.GetLinearVelocity(hip).z - before;
		};

		fox.GetComponent<RagdollComponent>().Recipe = "heavy";
		m_Scene->OnUpdate(k_Dt);
		Check(DistinctBodies(*m_Scene, fox, skeleton) == std::size(k_ExpectedBodies), "recipe: naming another recipe rebuilds with it");
		Check(std::abs(hipKick(0.5f * settings.TotalMass) - 1.0f) < 0.01f, "recipe: mass=0.5 gives the hip half the TotalMass");

		writeRecipes("[heavy]\nb_Hip_01 mass=0.25\n");
		Check(m_RecipeFox->ReloadRagdollRecipes(), "recipe: ReloadRagdollRecipes reads the edited file");
		m_Scene->OnUpdate(k_Dt);
		Check(std::abs(hipKick(0.25f * settings.TotalMass) - 1.0f) < 0.01f, "recipe: an edited recipe rebuilds the ragdoll with its new mass");

		m_Scene->DestroyEntity(fox);
		m_Scene->OnUpdate(k_Dt);
	}

	void RagdollTest::RunPushChecks()
	{
		Entity firm = AddFox("Pushed Fox", { -2.5f, 0.3f, 2.0f }, false);
		firm.AddComponent<RagdollComponent>();
		Entity weak = AddFox("Strength 0 Fox", { 2.5f, 0.3f, 2.0f }, false);
		RagdollComponent settings;
		settings.Strength = 0.0f;
		weak.AddComponent<RagdollComponent>(settings);
		m_Scene->OnUpdate(k_Dt);

		Physics3D& physics = *m_Scene->GetPhysics3D();
		const PhysicsBodyId3D chest = m_Scene->GetRagdollBody(firm, "b_Spine02_03");
		const PhysicsBodyId3D weakHip = m_Scene->GetRagdollBody(weak, "b_Hip_01");
		if (chest == k_InvalidBody3D || weakHip == k_InvalidBody3D)
		{
			Check(false, "push: both foxes build their ragdolls");
			return;
		}
		const glm::vec3 weakStart = physics.GetPosition(weakHip);
		Run(*m_Scene, 2.0f);

		const glm::vec3 chestStart = physics.GetPosition(chest);
		const glm::vec3 shove(0.0f, 0.0f, 60.0f);
		physics.ApplyImpulse(chest, shove);
		physics.ApplyImpulse(m_Scene->GetRagdollBody(weak, "b_Spine02_03"), shove);
		float bent = 0.0f;
		for (int i = 0; i < 18; i++)
		{
			m_Scene->OnUpdate(k_Dt);
			bent = (std::max)(bent, glm::length(physics.GetPosition(chest) - chestStart));
		}
		Run(*m_Scene, 2.0f);
		const float back = glm::length(physics.GetPosition(chest) - chestStart);
		DE_INFO("Ragdoll push: the driven chest moved {:.1f} mm and ended {:.2f} mm from its pose", bent * 1000.0f, back * 1000.0f);

		Check(bent > 0.008f, "push: a 60 N*s impulse on a driven fox's chest bends its pose");
		Check(back < 0.005f, "push: at Strength 1 the chest returns within 5 mm of the animated pose in 2 s");
		Check(physics.GetPosition(weakHip).y < weakStart.y - 0.05f, "push: at Strength 0 the fox stays limp and collapses");

		const PhysicsBodyId3D firmHip = m_Scene->GetRagdollBody(firm, "b_Hip_01");
		Transform3DComponent& walk = firm.GetComponent<Transform3DComponent>();
		const glm::vec3 restOffset = physics.GetPosition(firmHip) - walk.Position;
		float lag = 0.0f;
		for (int i = 0; i < 60; i++)
		{
			walk.Position.x -= 1.5f * k_Dt;
			m_Scene->OnUpdate(k_Dt);
			if (i >= 30)
				lag = (std::max)(lag, glm::length(physics.GetPosition(firmHip) - walk.Position - restOffset));
		}
		DE_INFO("Ragdoll walk: at 1.5 m/s the driven hip trails its pose by {:.1f} mm", lag * 1000.0f);
		Check(lag < 0.04f, "push: a driven fox carried at 1.5 m/s keeps its hip within 4 cm (a frame plus 15 mm) of the animated pose");
	}

	void RagdollTest::RunRecoverChecks()
	{
		Entity fox = AddFox("Recovering Fox", { 4.0f, 0.3f, -2.0f }, false);
		fox.AddComponent<RagdollComponent>();
		m_Scene->OnUpdate(k_Dt);

		Physics3D& physics = *m_Scene->GetPhysics3D();
		const PhysicsBodyId3D hip = m_Scene->GetRagdollBody(fox, "b_Hip_01");
		if (hip == k_InvalidBody3D)
		{
			Check(false, "recover: the fox builds its ragdoll");
			return;
		}
		Run(*m_Scene, 1.0f);
		const float standingHip = physics.GetPosition(hip).y;
		RagdollComponent& settings = fox.GetComponent<RagdollComponent>();

		settings.Strength = 0.0f;
		Run(*m_Scene, 3.0f);
		const float limpHip = physics.GetPosition(hip).y;
		const float limpEntity = fox.GetComponent<Transform3DComponent>().Position.y;

		settings.Strength = 1.0f;
		Run(*m_Scene, 4.0f);
		const float risenHip = physics.GetPosition(hip).y;
		DE_INFO("Ragdoll recover: hip {:.3f} m standing, {:.3f} m limp (entity y {:.3f}), {:.3f} m after Strength returns to 1",
			standingHip, limpHip, limpEntity, risenHip);

		Check(limpHip < standingHip - 0.3f, "recover: at Strength 0 the fox drops well below its standing hip height");
		Check(std::abs(risenHip - standingHip) < 0.1f, "recover: a limp fox stands back up within 10 cm of its hip height when Strength returns to 1");
	}

	void RagdollTest::RunStairChecks()
	{
		const Skeleton& skeleton = *m_Fox->GetSkeleton();
		float top = 0.0f;
		float length = 0.0f;
		m_Stairs = MakeStairs(top, length);
		const glm::vec3 base(-6.0f, 0.0f, -5.0f);
		Entity stairs = m_Scene->CreateEntity("Stairs");
		stairs.AddComponent<Transform3DComponent>(Transform3DComponent(base));
		stairs.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Stairs, { 0.55f, 0.45f, 0.35f, 1.0f }));
		stairs.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
		stairs.AddComponent<MeshCollider3DComponent>(MeshCollider3DComponent(m_Stairs));
		m_Scene->CreateRigidBody(stairs);

		Entity faller = AddFox("Stair Fox", base + glm::vec3(0.0f, top + 0.05f, 0.95f), true);
		RagdollComponent limp;
		limp.DriveToAnimation = false;
		faller.AddComponent<RagdollComponent>(limp);
		m_Scene->OnUpdate(k_Dt);

		Physics3D& physics = *m_Scene->GetPhysics3D();
		const Animator* animator = m_Scene->GetAnimator(faller);
		const PhysicsBodyId3D hip = m_Scene->GetRagdollBody(faller, "b_Hip_01");
		if (!animator || hip == k_InvalidBody3D || !physics.IsBodyValid(m_Scene->GetRuntimeBody3D(stairs)))
		{
			Check(false, "stair-fall: the staircase and the fox's ragdoll build");
			return;
		}

		const auto links = BodyLinks(skeleton);
		const std::vector<float> rest = LinkLengths(*animator, links);
		const glm::vec3 start = physics.GetPosition(hip);
		physics.ApplyImpulse(hip, { 0.0f, 0.0f, 150.0f });
		Run(*m_Scene, 5.0f);

		bool finite = true;
		for (const char* joint : k_ExpectedBodies)
			finite = finite && Finite(physics.GetPosition(m_Scene->GetRagdollBody(faller, joint)));
		const glm::vec3 end = physics.GetPosition(hip);
		Check(finite && end.y < start.y - 0.5f && end.y > -0.1f && end.z > start.z + 0.5f,
			"stair-fall: a limp fox tumbles off the landing and down the steps, finite and above the floor");

		const std::vector<float> after = LinkLengths(*animator, links);
		float worst = 0.0f;
		for (size_t i = 0; i < links.size(); i++)
			worst = (std::max)(worst, rest[i] > 0.0f ? after[i] / rest[i] : 0.0f);
		DE_INFO("Ragdoll stair-fall: hip {:.2f} m -> {:.2f} m, longest bone at {:.3f}x its length", start.y, end.y, worst);
		Check(!links.empty() && worst < 1.1f, "stair-fall: every simulated bone stays within 10% of its length from its parent");
	}

	void RagdollTest::RunCrowdChecks()
	{
		constexpr int k_Foxes = 8;
		constexpr double k_BudgetMs = 8.0;
		Scene crowd("Ragdoll Crowd");
		AddFloor(crowd);
		std::vector<Entity> foxes;
		for (int i = 0; i < k_Foxes; i++)
		{
			const bool driven = i % 2 == 0;
			Entity fox = AddFox(crowd, "Crowd Fox", { (i % 4) * 2.0f - 3.0f, 0.3f, (i / 4) * 2.5f }, driven);
			RagdollComponent settings;
			settings.DriveToAnimation = driven;
			fox.AddComponent<RagdollComponent>(settings);
			foxes.push_back(fox);
		}
		crowd.OnStart();
		crowd.OnUpdate(k_Dt);

		size_t bodies = 0;
		for (Entity fox : foxes)
			bodies += DistinctBodies(crowd, fox, *m_Fox->GetSkeleton());
		const size_t joints = bodies - foxes.size();

		constexpr int k_Updates = 120;
		double total = 0.0;
		double worst = 0.0;
		for (int i = 0; i < k_Updates; i++)
		{
			const auto begin = std::chrono::steady_clock::now();
			crowd.OnUpdate(k_Dt);
			const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
			total += ms;
			worst = (std::max)(worst, ms);
		}
		const double mean = total / k_Updates;
		DE_INFO("Ragdoll crowd: {} ragdolls, {} bodies, {} joints; Scene::OnUpdate mean {:.2f} ms, worst {:.2f} ms (budget {:.1f} ms mean, checked outside Debug)",
			k_Foxes, bodies, joints, mean, worst, k_BudgetMs);
		Check(bodies == k_Foxes * std::size(k_ExpectedBodies), "crowd: eight ragdolls build 128 bodies");
#if !defined(DE_DEBUG) && !defined(__SANITIZE_ADDRESS__)
		Check(mean < k_BudgetMs, "crowd: eight ragdolls update in under 8 ms per 1/60 s step on average (CPU)");
#endif
		crowd.OnStop();
	}

	void RagdollTest::RunFixedTickChecks()
	{
		SceneParams params;
		params.FixedTick = true;
		Scene ticked("Ragdoll FixedTick", params);
		constexpr float k_Step = 1.0f / 32.0f;
		ticked.SetFixedTimeStep(k_Step);
		AddFloor(ticked);
		Entity fox = AddFox(ticked, "Falling Fox", { 0.0f, 3.0f, 0.0f }, true);
		RagdollComponent limp;
		limp.DriveToAnimation = false;
		fox.AddComponent<RagdollComponent>(limp);
		ticked.OnStart();

		const Skeleton& skeleton = *m_Fox->GetSkeleton();
		const int32_t hip = skeleton.FindJoint("b_Hip_01");
		auto drawnHip = [&]()
		{
			const Animator* animator = ticked.GetAnimator(fox);
			if (!animator || hip < 0)
				return glm::vec3(0.0f);
			return glm::vec3((ticked.GetDrawnWorldTransform(fox) * skeleton.GetRootTransform() * animator->GetGlobalTransforms()[hip])[3]);
		};

		ticked.OnUpdate(k_Step);
		ticked.OnUpdate(k_Step);
		ticked.OnUpdate(k_Step);
		const glm::vec3 previous = fox.GetComponent<Transform3DComponent>().Position;
		ticked.OnUpdate(k_Step);
		const glm::vec3 latest = fox.GetComponent<Transform3DComponent>().Position;
		const glm::vec3 drawnA = drawnHip();
		ticked.OnUpdate(k_Step * 0.5f);
		const float alpha = ticked.GetFixedTimeAlpha();
		const glm::vec3 between = fox.GetComponent<Transform3DComponent>().Position;
		const glm::vec3 drawnEntity = glm::vec3(ticked.GetDrawnWorldTransform(fox)[3]);
		const glm::vec3 drawnB = drawnHip();
		ticked.OnUpdate(k_Step * 0.5f);
		const glm::vec3 drawnC = drawnHip();

		Check(std::abs(alpha - 0.5f) < 1e-5f && between == latest && glm::length(latest - previous) > 1e-3f,
			"fixed tick: between ticks a falling fox's entity reads its latest tick");
		Check(glm::length(drawnEntity - glm::mix(previous, latest, alpha)) < 1e-4f,
			"fixed tick: the entity following its root body is drawn between its last two ticks");
		const float travelled = glm::length(drawnC - drawnA);
		Check(travelled > 1e-3f && glm::length(drawnB - 0.5f * (drawnA + drawnC)) < 0.1f * travelled,
			"fixed tick: the posed bones are drawn between their last two ticks");
		ticked.OnStop();
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
		RunRecipeChecks();
		RunPushChecks();
		RunRecoverChecks();
		RunStairChecks();
		RunCrowdChecks();
		RunFixedTickChecks();

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
		delete m_RecipeFox;
		m_RecipeFox = nullptr;
		delete m_Stairs;
		m_Stairs = nullptr;
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
