#include "Life/VillagerWorld.h"
#include "GameTuning.h"
#include "Look/ArtRegistry.h"
#include "Look/FarmerAssets.h"

#include <glm/gtc/quaternion.hpp>

namespace Dingo
{

	namespace
	{
		constexpr std::uint32_t k_VillagerLayer = 2u;
	}

	VillagerWorld::VillagerWorld(Scene& scene, const FarmerAssets* assets)
		: m_Scene(scene)
	{
		if (assets)
		{
			m_Idle = assets->GetIdle();
			m_Walk = assets->GetWalk();
		}

		for (int i = 0; i < VILLAGER_COUNT; ++i)
		{
			const VillagerId id = static_cast<VillagerId>(i);
			const ArtEntry* body = Art::Find(Art::ForVillager(id));
			if (!body)
				continue;

			const VillagerLook look = Art::ForVillagerCharacter(id);
			const CharacterAsset* character = assets ? assets->FindCharacter(look.ModelPath) : nullptr;
			const bool skinned = character && character->Loaded && character->Loaded->GetSkeleton() && character->Height > 0.0f && m_Idle && m_Walk;

			const float height = skinned ? look.Height : body->Size.y;
			const float half = 0.5f * height;
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

			Entity visual;
			if (skinned)
			{
				visual = scene.CreateEntity("Character");
				auto& transform = visual.AddComponent<Transform3DComponent>();
				transform.Position = glm::vec3(0.0f, -half, 0.0f);
				transform.Scale = glm::vec3(look.Height / character->Height);
				SkinnedMeshRendererComponent renderer(character->Loaded);
				renderer.Material = character->Lit;
				renderer.Color = look.Tint;
				visual.AddComponent<SkinnedMeshRendererComponent>(renderer);
				AnimatorComponent animator;
				animator.PlayOnStart = false;
				visual.AddComponent<AnimatorComponent>(animator);
			}
			else
			{
				visual = Art::Spawn(scene, Art::ForVillager(id), "Body", glm::vec3(0.0f, 0.5f * body->Size.y - half, 0.0f));
			}
			if (visual)
				visual.SetParent(root, false);
			m_Visuals[static_cast<size_t>(i)] = skinned ? visual : Entity();

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
			if (!visible)
				continue;

			Entity visual = m_Visuals[static_cast<size_t>(i)];
			if (visual.IsValid())
			{
				if (Animator* animator = m_Scene.GetAnimator(visual))
				{
					const AnimationClip* wanted = place.Moving ? m_Walk : m_Idle;
					if (animator->GetCurrentClip() != wanted)
						animator->Play(wanted, PLAYER_ANIM_FADE);
					visual.GetComponent<AnimatorComponent>().Speed = place.Moving ? PLAYER_WALK_ANIM_SPEED : 1.0f;
				}
			}

			if (!physics)
				continue;

			const PhysicsBodyId3D body = m_Scene.GetRuntimeBody3D(root);
			if (body == k_InvalidBody3D)
				continue;
			// A zero step (always under --freeze) cannot move a kinematic body, so it is placed directly or picking misses it.
			if (dt > 0.0f)
				physics->MoveKinematic(body, transform.Position, transform.Rotation, dt);
			else
				physics->SetPositionAndRotation(body, transform.Position, transform.Rotation);
		}
	}

	std::optional<glm::vec3> VillagerWorld::GetBodyPosition(VillagerId villager) const
	{
		const Physics3D* physics = m_Scene.GetPhysics3D();
		const Entity root = m_Roots[static_cast<size_t>(villager)];
		if (!physics || !root.IsValid())
			return std::nullopt;
		const PhysicsBodyId3D body = m_Scene.GetRuntimeBody3D(root);
		if (body == k_InvalidBody3D)
			return std::nullopt;
		return physics->GetPosition(body);
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
