#include "MeshColliderTest.h"

#include <glm/gtc/constants.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <format>

namespace
{
	constexpr int   k_TerrainCells = 32;
	constexpr float k_TerrainHalfSize = 8.0f;
	// A pillar tall enough that its base never clears the terrain: nothing can roll
	// under it and be crushed through the ground when it comes back down.
	constexpr float k_LiftHeight = 4.0f;
	constexpr float k_LiftBottom = -0.7f - 0.5f * k_LiftHeight;
	constexpr float k_LiftTravel = 3.0f;
	constexpr float k_LiftPeriod = 6.0f;
	constexpr float k_SpawnInterval = 0.25f;
	constexpr float k_SettleSeconds = 4.0f;

	float TerrainHeight(float x, float z)
	{
		return 0.06f * (x * x + z * z) + 0.35f * std::sin(x * 1.3f) * std::cos(z * 1.1f);
	}

	Dingo::Mesh* CreateTerrainMesh()
	{
		const int rowLength = k_TerrainCells + 1;
		const float step = 2.0f * k_TerrainHalfSize / k_TerrainCells;

		std::vector<Dingo::MeshVertex> vertices;
		vertices.reserve(rowLength * rowLength);
		for (int j = 0; j < rowLength; j++)
		{
			for (int i = 0; i < rowLength; i++)
			{
				const float x = -k_TerrainHalfSize + i * step;
				const float z = -k_TerrainHalfSize + j * step;
				const float e = 0.01f;
				const float dx = (TerrainHeight(x + e, z) - TerrainHeight(x - e, z)) / (2.0f * e);
				const float dz = (TerrainHeight(x, z + e) - TerrainHeight(x, z - e)) / (2.0f * e);
				vertices.push_back({ { x, TerrainHeight(x, z), z }, glm::normalize(glm::vec3(-dx, 1.0f, -dz)),
					{ i / float(k_TerrainCells), j / float(k_TerrainCells) } });
			}
		}

		// Counter-clockwise seen from above: collision triangles are one-sided.
		std::vector<uint32_t> indices;
		indices.reserve(k_TerrainCells * k_TerrainCells * 6);
		for (int j = 0; j < k_TerrainCells; j++)
		{
			for (int i = 0; i < k_TerrainCells; i++)
			{
				const uint32_t a = j * rowLength + i;
				const uint32_t b = a + 1;
				const uint32_t d = a + rowLength;
				const uint32_t c = d + 1;
				indices.insert(indices.end(), { a, d, c, a, c, b });
			}
		}

		return Dingo::Mesh::Create(vertices, indices);
	}
}

namespace Dingo
{

	void MeshColliderTest::Check(bool condition, const std::string& name)
	{
		m_Checks.push_back({ name, condition });
		if (condition)
			DE_INFO("[PASS] {}", name);
		else
			DE_ERROR("[FAIL] {}", name);
	}

	void MeshColliderTest::Initialize()
	{
		m_Checks.clear();
		m_Bodies.clear();
		m_Random.seed(7);
		m_SpawnTarget = 30;
		m_Spawned = 0;
		m_Escaped = 0;
		m_SpawnTimer = 0.0f;
		m_SettleTimer = 0.0f;
		m_SettledChecksDone = false;
		m_PebbleChecked = false;
		m_LiftTime = 0.0f;

		m_TerrainMesh = CreateTerrainMesh();
		m_PebbleMesh = Mesh::CreateSphere(0.5f, 6, 8);
		m_LiftMesh = Mesh::CreateBox();

		m_Scene = new Scene("Mesh Collider Test");

		m_Terrain = m_Scene->CreateEntity("Terrain");
		m_Terrain.AddComponent<Transform3DComponent>();
		m_Terrain.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_TerrainMesh, { 0.36f, 0.52f, 0.30f, 1.0f }));
		m_Terrain.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
		m_Terrain.AddComponent<MeshCollider3DComponent>();

		m_Lift = m_Scene->CreateEntity("Lift");
		m_Lift.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, k_LiftBottom, 0.0f }, { 3.0f, k_LiftHeight, 3.0f }));
		m_Lift.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_LiftMesh, { 0.85f, 0.70f, 0.30f, 1.0f }));
		m_Lift.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Kinematic));
		m_Lift.AddComponent<MeshCollider3DComponent>(MeshCollider3DComponent(m_LiftMesh));

		m_Scene->OnStart();

		Physics3D* physics = m_Scene->GetPhysics3D();
		const PhysicsBodyId3D terrainBody = m_Scene->GetRuntimeBody3D(m_Terrain);
		Check(physics && physics->IsBodyValid(terrainBody), "static triangle-mesh terrain built from its MeshRenderer mesh");
		Check(physics && physics->IsBodyValid(m_Scene->GetRuntimeBody3D(m_Lift)), "kinematic triangle-mesh lift built");

		bool raysOnSurface = physics != nullptr;
		for (const glm::vec2 point : { glm::vec2(2.0f, -3.0f), glm::vec2(-4.5f, 1.5f), glm::vec2(5.5f, 5.0f) })
		{
			RayCastHit3D hit;
			raysOnSurface = raysOnSurface
				&& physics->RayCast(Ray({ point.x, 20.0f, point.y }, { 0.0f, -1.0f, 0.0f }), 40.0f, hit)
				&& hit.Body == terrainBody
				&& std::abs(hit.Point.y - TerrainHeight(point.x, point.y)) < 1e-3f
				&& hit.Normal.y > 0.0f;
		}
		Check(raysOnSurface, "ray casts land on the terrain's vertices with upward normals");

		Entity aliasA = m_Scene->CreateEntity("Alias A");
		aliasA.AddComponent<Transform3DComponent>(Transform3DComponent({ -40.0f, -40.0f, 0.0f }));
		aliasA.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Kinematic));
		Entity aliasB = m_Scene->CreateEntity("Alias B");
		aliasB.AddComponent<Transform3DComponent>(Transform3DComponent({ 40.0f, -40.0f, 0.0f }));
		aliasB.AddComponent<RigidBody3DComponent>();
		m_Scene->CreateRigidBody(aliasA);
		m_Scene->CreateRigidBody(aliasB);

		aliasB.GetComponent<RigidBody3DComponent>() = aliasA.GetComponent<RigidBody3DComponent>();
		const PhysicsBodyId3D aliasBodyA = m_Scene->GetRuntimeBody3D(aliasA);
		const PhysicsBodyId3D aliasBodyB = m_Scene->GetRuntimeBody3D(aliasB);
		const bool ownBodies = physics && aliasBodyA != aliasBodyB && physics->IsBodyValid(aliasBodyA) && physics->IsBodyValid(aliasBodyB);
		m_Scene->DestroyEntity(aliasA);
		Check(ownBodies && physics->IsBodyValid(aliasBodyB), "assigning a live RigidBody3DComponent onto another entity leaves each its own body");
		m_Scene->DestroyEntity(aliasB);

		m_Camera = PerspectiveCamera(45.0f, m_AspectRatio, 0.1f, 200.0f);
	}

	void MeshColliderTest::SpawnBody()
	{
		std::uniform_real_distribution<float> spread(-3.0f, 3.0f);
		std::uniform_real_distribution<float> unit(0.0f, 1.0f);
		Renderer3D& renderer = Application::Get().GetRenderer3D();

		Entity body = m_Scene->CreateEntity("Body");
		auto& transform = body.AddComponent<Transform3DComponent>();
		transform.Position = { spread(m_Random), 7.0f + 2.0f * unit(m_Random), spread(m_Random) };
		transform.SetRotationEuler({ 360.0f * unit(m_Random), 360.0f * unit(m_Random), 0.0f });
		RigidBody3DComponent rigidBody(BodyType3D::Dynamic);
		rigidBody.ContinuousCollision = true;
		body.AddComponent<RigidBody3DComponent>(rigidBody);

		const bool pebble = m_Spawned % 3 == 0;
		if (pebble)
		{
			transform.Scale = { 0.5f + 0.4f * unit(m_Random), 0.35f + 0.25f * unit(m_Random), 0.5f + 0.4f * unit(m_Random) };
			body.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_PebbleMesh, { 0.62f, 0.58f, 0.52f, 1.0f }));
			body.AddComponent<MeshCollider3DComponent>(MeshCollider3DComponent(nullptr, true));
		}
		else if (m_Spawned % 3 == 1)
		{
			transform.Scale = glm::vec3(0.6f);
			body.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetSphereMesh(), { 0.85f, 0.35f, 0.25f, 1.0f }));
			body.AddComponent<SphereCollider3DComponent>();
		}
		else
		{
			transform.Scale = glm::vec3(0.6f);
			body.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetBoxMesh(), { 0.35f, 0.55f, 0.80f, 1.0f }));
			body.AddComponent<BoxCollider3DComponent>();
		}

		m_Scene->CreateRigidBody(body);
		m_Bodies.push_back(body);
		m_Spawned++;

		if (pebble && !m_PebbleChecked)
		{
			m_PebbleChecked = true;
			Check(m_Scene->GetPhysics3D()->IsBodyValid(m_Scene->GetRuntimeBody3D(body)),
				"dynamic convex-hull pebble built from its MeshRenderer mesh at a non-uniform scale");
		}
	}

	void MeshColliderTest::UpdateLift(float deltaTime)
	{
		if (m_LiftEnabled)
			m_LiftTime += deltaTime;

		const float phase = 2.0f * glm::pi<float>() * m_LiftTime / k_LiftPeriod;
		const float height = k_LiftBottom + 0.5f * k_LiftTravel * (1.0f - std::cos(phase));

		m_Scene->GetPhysics3D()->MoveKinematic(m_Scene->GetRuntimeBody3D(m_Lift),
			{ 0.0f, height, 0.0f }, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), deltaTime);
	}

	void MeshColliderTest::RunSettledChecks()
	{
		uint32_t inside = 0;
		uint32_t sunk = 0;
		for (Entity body : m_Bodies)
		{
			const glm::vec3 position = body.GetComponent<Transform3DComponent>().Position;
			if (std::abs(position.x) > k_TerrainHalfSize - 0.5f || std::abs(position.z) > k_TerrainHalfSize - 0.5f)
				continue;

			inside++;
			if (position.y < TerrainHeight(position.x, position.z) - 0.05f)
				sunk++;
		}
		Check(inside > 0 && sunk == 0, std::format("no body sank through the terrain ({} of {} below the surface)", sunk, inside));
	}

	void MeshColliderTest::Update(float deltaTime)
	{
		const float step = (std::min)(deltaTime, 1.0f / 30.0f);

		if (m_Spawned < m_SpawnTarget)
		{
			m_SpawnTimer += step;
			if (m_SpawnTimer >= k_SpawnInterval)
			{
				m_SpawnTimer = 0.0f;
				SpawnBody();
			}
		}
		else if (!m_SettledChecksDone)
		{
			m_SettleTimer += step;
			if (m_SettleTimer >= k_SettleSeconds)
			{
				m_SettledChecksDone = true;
				RunSettledChecks();
			}
		}

		UpdateLift(step);
		m_Scene->OnUpdate(step);

		for (auto it = m_Bodies.begin(); it != m_Bodies.end();)
		{
			if (it->GetComponent<Transform3DComponent>().Position.y < -10.0f)
			{
				m_Scene->DestroyEntity(*it);
				it = m_Bodies.erase(it);
				m_Escaped++;
			}
			else
			{
				++it;
			}
		}

		if (m_AutoOrbit)
			m_OrbitAngle = std::fmod(m_OrbitAngle + step * 10.0f, 360.0f);
		const float orbit = glm::radians(m_OrbitAngle);
		m_Camera.SetPosition({ std::sin(orbit) * 16.0f, 10.0f, std::cos(orbit) * 16.0f });
		m_Camera.SetTarget({ 0.0f, 1.0f, 0.0f });

		Renderer3D& renderer = Application::Get().GetRenderer3D();
		renderer.BeginScene(m_Camera);
		renderer.Clear(m_ClearColor);
		m_Scene->SubmitLights(renderer);
		m_Scene->RenderEntities3D(renderer);
		renderer.EndScene();
	}

	void MeshColliderTest::Cleanup()
	{
		delete m_Scene;
		m_Scene = nullptr;
		m_Bodies.clear();
		m_Terrain = {};
		m_Lift = {};

		delete m_TerrainMesh;
		delete m_PebbleMesh;
		delete m_LiftMesh;
		m_TerrainMesh = nullptr;
		m_PebbleMesh = nullptr;
		m_LiftMesh = nullptr;
	}

	void MeshColliderTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
		m_Camera.SetAspectRatio(m_AspectRatio);
	}

	void MeshColliderTest::ImGuiRender()
	{
		GraphicsTest::ImGuiRender();
		ImGui::Separator();

		ImGui::Text("Bodies: %u  Spawned: %u / %u  Escaped: %u",
			static_cast<uint32_t>(m_Bodies.size()), m_Spawned, m_SpawnTarget, m_Escaped);
		if (ImGui::Button("Spawn 10 more"))
		{
			m_SpawnTarget += 10;
			m_SettledChecksDone = false;
			m_SettleTimer = 0.0f;
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset"))
		{
			Cleanup();
			Initialize();
			return;
		}

		ImGui::Checkbox("Lift moving", &m_LiftEnabled);
		ImGui::Checkbox("Auto Orbit", &m_AutoOrbit);
		if (!m_AutoOrbit)
			ImGui::SliderFloat("Orbit", &m_OrbitAngle, 0.0f, 360.0f);

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : ImVec4(0.95f, 0.3f, 0.3f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
		if (!m_SettledChecksDone)
			ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "[....] bodies still spawning / settling");
	}

}
