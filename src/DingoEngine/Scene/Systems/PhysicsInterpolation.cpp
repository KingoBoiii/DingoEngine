#include "depch.h"
#include "DingoEngine/Scene/Systems/PhysicsInterpolation.h"

#include "DingoEngine/Scene/Components.h"
#include "DingoEngine/Scene/Systems/HierarchySystem.h"
#include "DingoEngine/Scene/Systems/RuntimeComponents.h"

#include <glm/gtc/quaternion.hpp>

#include <cmath>

namespace Dingo
{

	namespace Internal
	{

		namespace
		{

			// Kinematic children are moved by the game, static bodies not at all: only what physics
			// writes back is drawn between ticks.
			bool Tracked3D(const entt::registry& registry, entt::entity handle)
			{
				if (!registry.all_of<Transform3DComponent>(handle))
					return false;
				if (registry.all_of<CharacterController3DRuntime>(handle))
					return true;

				const RigidBody3DRuntime* runtime = registry.try_get<RigidBody3DRuntime>(handle);
				if (!runtime || runtime->Type == BodyType3D::Static)
					return false;
				if (runtime->Type == BodyType3D::Kinematic && HierarchySystem::GetParent(registry, handle) != entt::null)
					return false;

				const RigidBody3DComponent* settings = registry.try_get<RigidBody3DComponent>(handle);
				return !settings || settings->Interpolate;
			}

			bool Tracked2D(const entt::registry& registry, entt::entity handle)
			{
				const RigidBody2DRuntime* runtime = registry.try_get<RigidBody2DRuntime>(handle);
				if (!runtime || runtime->Type == BodyType2D::Static || !registry.all_of<TransformComponent>(handle))
					return false;
				if (runtime->Type == BodyType2D::Kinematic && HierarchySystem::GetParent(registry, handle) != entt::null)
					return false;

				const RigidBody2DComponent* settings = registry.try_get<RigidBody2DComponent>(handle);
				return !settings || settings->Interpolate;
			}

			// A moved position is a teleport and snaps. A rotation alone is the game turning a body
			// or controller every frame (a player facing its input): that keeps position smoothing
			// and draws the rotation as set.
			enum class Change { None, Rotation, Teleport };

			Change Combine(bool positionMoved, bool rotationMoved)
			{
				if (positionMoved)
					return Change::Teleport;
				return rotationMoved ? Change::Rotation : Change::None;
			}

			// Exact comparisons: between ticks nothing but the game changes a component, a body or a
			// controller, and the write-back copies the simulated pose bit for bit.
			Change Moved3D(const entt::registry& registry, const PhysicsSync& physics, entt::entity handle, const InterpolatedPose3D& pose)
			{
				const Transform3DComponent& transform = registry.get<Transform3DComponent>(handle);
				bool positionMoved = transform.Position != pose.Position;
				bool rotationMoved = transform.Rotation != pose.Rotation;
				if (HierarchySystem::GetParent(registry, handle) == entt::null)
				{
					if (const CharacterController3D* controller = physics.GetController(registry, handle))
					{
						positionMoved = positionMoved || controller->GetPosition() != pose.Position;
						rotationMoved = rotationMoved || controller->GetRotation() != pose.Rotation;
					}
					else if (const Physics3D* world = physics.Get3D())
					{
						const PhysicsBodyId3D body = physics.RuntimeBody3D(registry, handle);
						if (body != k_InvalidBody3D)
						{
							positionMoved = positionMoved || world->GetPosition(body) != pose.Position;
							rotationMoved = rotationMoved || world->GetRotation(body) != pose.Rotation;
						}
					}
				}
				return Combine(positionMoved, rotationMoved);
			}

			Change Moved2D(const entt::registry& registry, const PhysicsSync& physics, entt::entity handle, const InterpolatedPose2D& pose)
			{
				const TransformComponent& transform = registry.get<TransformComponent>(handle);
				bool positionMoved = glm::vec2(transform.Position) != pose.Position;
				bool rotationMoved = transform.Rotation != pose.Rotation;
				if (HierarchySystem::GetParent(registry, handle) == entt::null)
				{
					if (const Physics2D* world = physics.Get2D())
					{
						const PhysicsBodyId2D body = registry.get<RigidBody2DRuntime>(handle).Body;
						positionMoved = positionMoved || world->GetPosition(body) != pose.Position;
						rotationMoved = rotationMoved || glm::degrees(world->GetAngle(body)) != pose.Rotation;
					}
				}
				return Combine(positionMoved, rotationMoved);
			}

			// The short way round: a game may wrap the component's angle differently from Box2D.
			float LerpDegrees(float from, float to, float alpha)
			{
				return from + std::remainder(to - from, 360.0f) * alpha;
			}

		}

		void PhysicsInterpolation::BeforeTick(entt::registry& registry, const PhysicsSync& physics)
		{
			m_Scratch.clear();
			for (auto [handle, pose] : registry.view<InterpolatedPose3D>().each())
			{
				const Change change = Tracked3D(registry, handle) ? Moved3D(registry, physics, handle, pose) : Change::Teleport;
				if (change == Change::Teleport)
					m_Scratch.push_back(handle);
				else if (change == Change::Rotation)
					pose.PreviousRotation = pose.Rotation = registry.get<Transform3DComponent>(handle).Rotation;
			}
			registry.remove<InterpolatedPose3D>(m_Scratch.begin(), m_Scratch.end());

			m_Scratch.clear();
			for (auto [handle, pose] : registry.view<InterpolatedPose2D>().each())
			{
				const Change change = Tracked2D(registry, handle) ? Moved2D(registry, physics, handle, pose) : Change::Teleport;
				if (change == Change::Teleport)
					m_Scratch.push_back(handle);
				else if (change == Change::Rotation)
					pose.PreviousRotation = pose.Rotation = registry.get<TransformComponent>(handle).Rotation;
			}
			registry.remove<InterpolatedPose2D>(m_Scratch.begin(), m_Scratch.end());
		}

		void PhysicsInterpolation::AfterTick(entt::registry& registry)
		{
			m_Scratch.clear();
			for (auto [handle, runtime] : registry.view<const RigidBody3DRuntime>().each())
				m_Scratch.push_back(handle);
			for (auto [handle, runtime] : registry.view<const CharacterController3DRuntime>(entt::exclude<RigidBody3DRuntime>).each())
				m_Scratch.push_back(handle);

			for (entt::entity handle : m_Scratch)
			{
				if (!Tracked3D(registry, handle))
				{
					registry.remove<InterpolatedPose3D>(handle);
					continue;
				}

				const Transform3DComponent& transform = registry.get<Transform3DComponent>(handle);
				if (InterpolatedPose3D* pose = registry.try_get<InterpolatedPose3D>(handle))
				{
					pose->PreviousPosition = pose->Position;
					pose->PreviousRotation = pose->Rotation;
					pose->Position = transform.Position;
					pose->Rotation = transform.Rotation;
				}
				else
				{
					registry.emplace<InterpolatedPose3D>(handle, transform.Position, transform.Position, transform.Rotation, transform.Rotation);
				}
			}

			for (auto [handle, runtime] : registry.view<const RigidBody2DRuntime>().each())
			{
				if (!Tracked2D(registry, handle))
				{
					registry.remove<InterpolatedPose2D>(handle);
					continue;
				}

				const TransformComponent& transform = registry.get<TransformComponent>(handle);
				const glm::vec2 position(transform.Position);
				if (InterpolatedPose2D* pose = registry.try_get<InterpolatedPose2D>(handle))
				{
					pose->PreviousPosition = pose->Position;
					pose->PreviousRotation = pose->Rotation;
					pose->Position = position;
					pose->Rotation = transform.Rotation;
				}
				else
				{
					registry.emplace<InterpolatedPose2D>(handle, position, position, transform.Rotation, transform.Rotation);
				}
			}
		}

		void PhysicsInterpolation::Clear(entt::registry& registry)
		{
			registry.clear<InterpolatedPose3D>();
			registry.clear<InterpolatedPose2D>();
		}

		void PhysicsInterpolation::Begin(entt::registry& registry, const PhysicsSync& physics, float alpha)
		{
			if (m_Depth++ > 0)
				return;

			m_Saved3D.clear();
			m_Saved2D.clear();

			for (auto [handle, pose] : registry.view<const InterpolatedPose3D>().each())
			{
				if (!Tracked3D(registry, handle))
					continue;
				const Change change = Moved3D(registry, physics, handle, pose);
				const bool still = pose.PreviousPosition == pose.Position && pose.PreviousRotation == pose.Rotation;
				if (change == Change::Teleport || still)
					continue;

				Transform3DComponent& transform = registry.get<Transform3DComponent>(handle);
				m_Saved3D.push_back({ handle, transform.Position, transform.Rotation });
				transform.Position = glm::mix(pose.PreviousPosition, pose.Position, alpha);
				if (change == Change::None)
					transform.Rotation = glm::slerp(pose.PreviousRotation, pose.Rotation, alpha);
			}

			for (auto [handle, pose] : registry.view<const InterpolatedPose2D>().each())
			{
				if (!Tracked2D(registry, handle))
					continue;
				const Change change = Moved2D(registry, physics, handle, pose);
				const bool still = pose.PreviousPosition == pose.Position && pose.PreviousRotation == pose.Rotation;
				if (change == Change::Teleport || still)
					continue;

				TransformComponent& transform = registry.get<TransformComponent>(handle);
				m_Saved2D.push_back({ handle, transform.Position, transform.Rotation });
				const glm::vec2 position = glm::mix(pose.PreviousPosition, pose.Position, alpha);
				transform.Position.x = position.x;
				transform.Position.y = position.y;
				if (change == Change::None)
					transform.Rotation = LerpDegrees(pose.PreviousRotation, pose.Rotation, alpha);
			}
		}

		void PhysicsInterpolation::End(entt::registry& registry)
		{
			if (m_Depth == 0 || --m_Depth > 0)
				return;

			for (const Saved3D& saved : m_Saved3D)
			{
				if (Transform3DComponent* transform = registry.try_get<Transform3DComponent>(saved.Handle))
				{
					transform->Position = saved.Position;
					transform->Rotation = saved.Rotation;
				}
			}

			for (const Saved2D& saved : m_Saved2D)
			{
				if (TransformComponent* transform = registry.try_get<TransformComponent>(saved.Handle))
				{
					transform->Position = saved.Position;
					transform->Rotation = saved.Rotation;
				}
			}

			m_Saved3D.clear();
			m_Saved2D.clear();
		}

	}

}
