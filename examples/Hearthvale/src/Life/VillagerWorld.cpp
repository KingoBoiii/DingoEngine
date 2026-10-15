#include "Life/VillagerWorld.h"
#include "GameTuning.h"
#include "Look/ArtRegistry.h"

#include <glm/gtc/quaternion.hpp>

namespace Dingo
{

	namespace
	{
		constexpr float k_HeadLift = 0.1f;
		constexpr std::uint32_t k_VillagerLayer = 2u;
	}

	VillagerWorld::VillagerWorld(Scene& scene)
		: m_Scene(scene)
	{
		for (int i = 0; i < VILLAGER_COUNT; ++i)
		{
			const VillagerId id = static_cast<VillagerId>(i);
			const ArtEntry* body = Art::Find(Art::ForVillager(id));
			const ArtEntry* head = Art::Find(Art::ForVillagerHead());
			if (!body || !head)
				continue;

			const float headCenter = body->Size.y + k_HeadLift;
			const float half = 0.5f * (headCenter + 0.5f * head->Size.y);
			m_HalfHeight[static_cast<size_t>(i)] = half;

			Entity root = scene.CreateEntity(GetVillagerDef(id).Name);
			root.AddComponent<Transform3DComponent>();
			RigidBody3DComponent bodyType(BodyType3D::Kinematic);
			bodyType.IsSensor = true;
			bodyType.QueryLayers = k_VillagerLayer;
			root.AddComponent<RigidBody3DComponent>(bodyType);
			BoxCollider3DComponent box;
			box.HalfExtents = glm::vec3(0.3f, half, 0.3f);
			root.AddComponent<BoxCollider3DComponent>(box);

			const struct { std::string_view Id; const char* Name; glm::vec3 Position; } parts[] = {
				{ Art::ForVillager(id),       "Body", glm::vec3(0.0f, 0.5f * body->Size.y - half, 0.0f) },
				{ Art::ForVillagerHead(),     "Head", glm::vec3(0.0f, headCenter - half, 0.0f) },
			};
			for (const auto& part : parts)
			{
				Entity visual = Art::Spawn(scene, part.Id, part.Name, part.Position);
				if (visual)
					visual.SetParent(root, false);
			}

			root.SetActive(false);
			m_Roots[static_cast<size_t>(i)] = root;
		}
	}

	void VillagerWorld::Sync(const VillagerPlaces& places, float dt)
	{
		Physics3D* physics = m_Scene.GetPhysics3D();
		for (int i = 0; i < VILLAGER_COUNT; ++i)
		{
			Entity root = m_Roots[static_cast<size_t>(i)];
			if (!root.IsValid())
				continue;

			const VillagerPlace& place = places[static_cast<size_t>(i)];
			auto& transform = root.GetComponent<Transform3DComponent>();
			transform.Position = glm::vec3(place.Position.x, GROUND_TOP + m_HalfHeight[static_cast<size_t>(i)], place.Position.y);
			transform.Rotation = glm::angleAxis(place.Yaw, glm::vec3(0.0f, 1.0f, 0.0f));

			const bool visible = !place.Indoors;
			if (root.IsActive() != visible)
				root.SetActive(visible);
			if (!visible || !physics || dt <= 0.0f)
				continue;

			const PhysicsBodyId3D body = m_Scene.GetRuntimeBody3D(root);
			if (body != k_InvalidBody3D)
				physics->MoveKinematic(body, transform.Position, transform.Rotation, dt);
		}
	}

	std::optional<VillagerId> VillagerWorld::Pick(const Ray& ray, float reach) const
	{
		const Physics3D* physics = m_Scene.GetPhysics3D();
		if (!physics)
			return std::nullopt;

		QueryFilter3D filter;
		filter.SetLayers(k_VillagerLayer).SetIncludeSensors(true);
		RayCastHit3D hit;
		if (!physics->RayCast(ray, reach, hit, filter))
			return std::nullopt;
		return VillagerOf(m_Scene.GetEntityFromBody3D(hit.Body));
	}

	std::optional<VillagerId> VillagerWorld::VillagerOf(Entity entity) const
	{
		if (!entity.IsValid())
			return std::nullopt;
		for (int i = 0; i < VILLAGER_COUNT; ++i)
		{
			if (m_Roots[static_cast<size_t>(i)] == entity)
				return static_cast<VillagerId>(i);
		}
		return std::nullopt;
	}

}
