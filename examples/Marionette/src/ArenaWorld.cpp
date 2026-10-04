#include "ArenaWorld.h"
#include "GameTuning.h"

#include <cmath>
#include <numbers>

namespace
{
	using namespace Dingo;

	constexpr float k_UnitSphereRadius = 0.5f;
	constexpr float k_SideAngle = 2.0f * std::numbers::pi_v<float> / ARENA_SIDES;
	constexpr float k_HalfSideAngle = 0.5f * k_SideAngle;

	float Apothem()
	{
		return ARENA_RADIUS * std::cos(k_HalfSideAngle);
	}

	glm::quat Yaw(float radians)
	{
		return glm::angleAxis(radians, glm::vec3(0.0f, 1.0f, 0.0f));
	}
}

namespace Dingo
{

	ArenaWorld::ArenaWorld(Scene& scene)
		: m_Scene(scene)
	{
		Renderer3D& renderer3D = Application::Get().GetRenderer3D();
		m_BoxMesh = renderer3D.GetBoxMesh();
		m_FlameMesh = Mesh::CreateSphere(k_UnitSphereRadius, FLAME_MESH_RINGS, FLAME_MESH_SEGMENTS);

		m_FloorMaterial = renderer3D.CreateLitMaterial(MaterialParams().SetDebugName("ArenaFloor").SetRoughness(FLOOR_ROUGHNESS));
		m_WallMaterial = renderer3D.CreateLitMaterial(MaterialParams().SetDebugName("ArenaWall").SetRoughness(WALL_ROUGHNESS));
		m_BrazierMaterial = renderer3D.CreateLitMaterial(MaterialParams().SetDebugName("ArenaBrazier").SetRoughness(BRAZIER_ROUGHNESS));
		m_FlameMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("ArenaFlame")
			.SetEmissiveColor(FLAME_COLOR)
			.SetEmissiveStrength(FLAME_EMISSIVE));

		Entity ambient = scene.CreateEntity("Ambient");
		ambient.AddComponent<Transform3DComponent>();
		ambient.AddComponent<AmbientLightComponent>(AmbientLightComponent(AMBIENT_COLOR, AMBIENT_INTENSITY));

		Entity moon = scene.CreateEntity("Moon");
		DirectionalLightComponent& moonLight = moon.AddComponent<DirectionalLightComponent>();
		moonLight.Direction = MOON_DIRECTION;
		moonLight.Color = MOON_COLOR;
		moonLight.Intensity = MOON_INTENSITY;
		moonLight.Ambient = 0.0f;

		BuildFloor();
		BuildWalls();
		BuildBraziers();
		DE_INFO("Marionette: arena built, {} m across, {} braziers", 2.0f * ARENA_RADIUS, BRAZIER_COUNT);
	}

	ArenaWorld::~ArenaWorld()
	{
		DestroyAndDelete(m_FloorMaterial);
		DestroyAndDelete(m_WallMaterial);
		DestroyAndDelete(m_BrazierMaterial);
		DestroyAndDelete(m_FlameMaterial);
		delete m_FlameMesh;
	}

	std::vector<glm::vec3> ArenaWorld::GetRimPoints() const
	{
		std::vector<glm::vec3> points;
		for (int i = 0; i < ARENA_SIDES; ++i)
		{
			const float angle = k_SideAngle * (static_cast<float>(i) + 0.5f);
			const float rim = (Apothem() + ARENA_WALL_THICKNESS) / std::cos(k_HalfSideAngle);
			for (const float y : { 0.0f, ARENA_WALL_HEIGHT })
				points.emplace_back(std::cos(angle) * rim, y, std::sin(angle) * rim);
		}
		return points;
	}

	Entity ArenaWorld::SpawnSolid(const char* name, const glm::vec3& center, const glm::vec3& size, float yawRadians, const glm::vec4& color, Material* material)
	{
		Entity entity = m_Scene.CreateEntity(name);
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = center;
		transform.Rotation = Yaw(yawRadians);
		transform.Scale = size;

		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_BoxMesh, color)).Material = material;
		entity.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
		entity.AddComponent<BoxCollider3DComponent>();
		return entity;
	}

	void ArenaWorld::BuildFloor()
	{
		// Six strips whose short ends are opposite sides of the polygon cover all of it.
		const float apothem = Apothem();
		const float strip = 2.0f * apothem * std::tan(k_HalfSideAngle);
		for (int i = 0; i < ARENA_SIDES / 2; ++i)
		{
			SpawnSolid("Floor", glm::vec3(0.0f, -ARENA_FLOOR_THICKNESS * 0.5f, 0.0f),
				glm::vec3(2.0f * apothem, ARENA_FLOOR_THICKNESS, strip), -k_SideAngle * static_cast<float>(i), COLOR_FLOOR, m_FloorMaterial);
		}
	}

	void ArenaWorld::BuildWalls()
	{
		const float apothem = Apothem();
		const float length = 2.0f * (apothem + ARENA_WALL_THICKNESS) * std::tan(k_HalfSideAngle);
		const float distance = apothem + ARENA_WALL_THICKNESS * 0.5f;
		for (int i = 0; i < ARENA_SIDES; ++i)
		{
			const float normal = k_SideAngle * static_cast<float>(i);
			SpawnSolid("Wall", glm::vec3(std::cos(normal) * distance, ARENA_WALL_HEIGHT * 0.5f, std::sin(normal) * distance),
				glm::vec3(length, ARENA_WALL_HEIGHT, ARENA_WALL_THICKNESS), 0.5f * std::numbers::pi_v<float> - normal, COLOR_WALL, m_WallMaterial);
		}
	}

	void ArenaWorld::BuildBraziers()
	{
		for (int i = 0; i < BRAZIER_COUNT; ++i)
		{
			const float angle = glm::radians(BRAZIER_ANGLE_OFFSET_DEG) + 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / BRAZIER_COUNT;
			const glm::vec3 floor(std::cos(angle) * BRAZIER_RING_RADIUS, 0.0f, std::sin(angle) * BRAZIER_RING_RADIUS);

			SpawnSolid("BrazierBase", floor + glm::vec3(0.0f, BRAZIER_BASE_HEIGHT * 0.5f, 0.0f),
				glm::vec3(BRAZIER_BASE_WIDTH, BRAZIER_BASE_HEIGHT, BRAZIER_BASE_WIDTH), 0.0f, COLOR_BRAZIER, m_BrazierMaterial);
			SpawnSolid("BrazierBowl", floor + glm::vec3(0.0f, BRAZIER_BASE_HEIGHT + BRAZIER_BOWL_HEIGHT * 0.5f, 0.0f),
				glm::vec3(BRAZIER_BOWL_WIDTH, BRAZIER_BOWL_HEIGHT, BRAZIER_BOWL_WIDTH), 0.0f, COLOR_BRAZIER, m_BrazierMaterial);

			const glm::vec3 flame = floor + glm::vec3(0.0f, BRAZIER_BASE_HEIGHT + BRAZIER_BOWL_HEIGHT + BRAZIER_FLAME_RISE, 0.0f);

			Entity core = m_Scene.CreateEntity("BrazierFlame");
			auto& coreTransform = core.AddComponent<Transform3DComponent>();
			coreTransform.Position = flame;
			coreTransform.Scale = glm::vec3(BRAZIER_FLAME_DIAMETER);
			core.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_FlameMesh, COLOR_FLAME)).Material = m_FlameMaterial;

			Entity light = m_Scene.CreateEntity("BrazierLight");
			light.AddComponent<Transform3DComponent>().Position = flame + glm::vec3(0.0f, BRAZIER_LIGHT_RISE, 0.0f);
			light.AddComponent<PointLightComponent>(PointLightComponent(FLAME_COLOR, BRAZIER_LIGHT_INTENSITY, BRAZIER_LIGHT_RANGE));
		}
	}

}
