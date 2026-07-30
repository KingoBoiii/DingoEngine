#include "depch.h"
#include "DingoEngine/Scene/Systems/PhysicsSync.h"

#include "DingoEngine/Scene/Components.h"

namespace Dingo
{

	namespace Internal
	{

		void PhysicsSync::Start(entt::registry& registry, const glm::vec2& gravity2D, const glm::vec3& gravity3D)
		{
			auto rb2dView = registry.view<RigidBody2DComponent>();
			if ((!m_Physics2D || !m_Physics2D->IsValid()) && rb2dView.begin() != rb2dView.end())
			{
				m_Physics2D.reset(Physics2D::Create());
				m_Physics2D->Initialize(gravity2D);

				for (entt::entity handle : rb2dView)
					CreateBody2D(registry, handle);
			}

			auto rb3dView = registry.view<RigidBody3DComponent>();
			auto cc3dView = registry.view<CharacterController3DComponent>();
			const bool needs3D = rb3dView.begin() != rb3dView.end() || cc3dView.begin() != cc3dView.end();
			if ((!m_Physics3D || !m_Physics3D->IsValid()) && needs3D)
			{
				Physics3DParams params;
				params.Gravity = gravity3D;
				m_Physics3D.reset(Physics3D::Create());
				m_Physics3D->Initialize(params);

				for (entt::entity handle : rb3dView)
					CreateBody3D(registry, handle);

				for (entt::entity handle : cc3dView)
					CreateController(registry, handle);
			}
		}

		void PhysicsSync::Stop(entt::registry& registry)
		{
			if (m_Physics2D && m_Physics2D->IsValid())
			{
				m_Physics2D->Shutdown(); // also destroys all bodies + shapes
				m_Physics2D.reset();

				// Clear the now-dangling runtime handles so a later restart is clean.
				for (entt::entity handle : registry.view<RigidBody2DComponent>())
					registry.get<RigidBody2DComponent>(handle).RuntimeBody = 0;
				for (entt::entity handle : registry.view<BoxCollider2DComponent>())
					registry.get<BoxCollider2DComponent>(handle).RuntimeShape = 0;
				for (entt::entity handle : registry.view<CircleCollider2DComponent>())
					registry.get<CircleCollider2DComponent>(handle).RuntimeShape = 0;
			}

			if (m_Physics3D && m_Physics3D->IsValid())
			{
				// Character controllers hold the world, so tear them down BEFORE the Physics3D.
				m_Controllers.clear();
				for (entt::entity handle : registry.view<CharacterController3DComponent>())
					registry.get<CharacterController3DComponent>(handle).RuntimeController = CharacterController3DComponent::k_InvalidControllerIndex;

				m_Physics3D->Shutdown(); // destroys all 3D bodies
				m_Physics3D.reset();

				for (entt::entity handle : registry.view<RigidBody3DComponent>())
					registry.get<RigidBody3DComponent>(handle).RuntimeBody = k_InvalidBody3D;
			}
		}

		void PhysicsSync::Step(entt::registry& registry, float deltaTime)
		{
			if (m_Physics2D && m_Physics2D->IsValid())
			{
				m_Physics2D->Step(deltaTime, m_SubStepCount);

				auto view = registry.view<RigidBody2DComponent, TransformComponent>();
				for (entt::entity handle : view)
				{
					const RigidBody2DComponent& rigidBody = view.get<RigidBody2DComponent>(handle);
					if (rigidBody.RuntimeBody == 0)
						continue;

					glm::vec2 position = m_Physics2D->GetPosition(rigidBody.RuntimeBody);
					float angle = m_Physics2D->GetAngle(rigidBody.RuntimeBody);

					TransformComponent& transform = view.get<TransformComponent>(handle);
					transform.Position.x = position.x;
					transform.Position.y = position.y;
					transform.Rotation = glm::degrees(angle);
				}
			}

			if (!m_Physics3D || !m_Physics3D->IsValid())
				return;

			m_Physics3D->Step(deltaTime, m_CollisionSteps);

			auto view = registry.view<RigidBody3DComponent, Transform3DComponent>();
			for (entt::entity handle : view)
			{
				const RigidBody3DComponent& rigidBody = view.get<RigidBody3DComponent>(handle);
				if (rigidBody.RuntimeBody == k_InvalidBody3D)
					continue;

				// Static bodies never move — skip the read-back so we don't churn over
				// them or revert a runtime edit to a static entity's Transform3D.
				if (rigidBody.Type == BodyType3D::Static)
					continue;

				Transform3DComponent& transform = view.get<Transform3DComponent>(handle);
				transform.Position = m_Physics3D->GetPosition(rigidBody.RuntimeBody);
				transform.Rotation = m_Physics3D->GetRotation(rigidBody.RuntimeBody);
			}

			// Character controllers: update each (scripts set its velocity in their
			// OnUpdate), then write the swept position/rotation back onto the entity's
			// Transform3D. Their capsule "feet" position is the transform origin.
			auto ccView = registry.view<CharacterController3DComponent, Transform3DComponent>();
			for (entt::entity handle : ccView)
			{
				const CharacterController3DComponent& cc = ccView.get<CharacterController3DComponent>(handle);
				if (cc.RuntimeController == CharacterController3DComponent::k_InvalidControllerIndex
					|| cc.RuntimeController >= m_Controllers.size())
					continue;

				CharacterController3D* controller = m_Controllers[cc.RuntimeController].get();
				if (!controller)
					continue;

				controller->Update(deltaTime);

				Transform3DComponent& transform = ccView.get<Transform3DComponent>(handle);
				transform.Position = controller->GetPosition();
				transform.Rotation = controller->GetRotation();
			}
		}

		void PhysicsSync::CreateBodiesForEntity(entt::registry& registry, entt::entity handle)
		{
			CreateBody2D(registry, handle);
			CreateBody3D(registry, handle);
			CreateController(registry, handle);
		}

		void PhysicsSync::DestroyBodiesForEntity(entt::registry& registry, entt::entity handle)
		{
			if (m_Physics2D && m_Physics2D->IsValid() && registry.all_of<RigidBody2DComponent>(handle))
				m_Physics2D->DestroyBody(registry.get<RigidBody2DComponent>(handle).RuntimeBody);

			if (m_Physics3D && m_Physics3D->IsValid() && registry.all_of<RigidBody3DComponent>(handle))
				m_Physics3D->DestroyBody(registry.get<RigidBody3DComponent>(handle).RuntimeBody);

			// Free the entity's character controller (its slot stays but goes null so other
			// entities' stored indices remain valid).
			if (registry.all_of<CharacterController3DComponent>(handle))
			{
				std::uint32_t index = registry.get<CharacterController3DComponent>(handle).RuntimeController;
				if (index != CharacterController3DComponent::k_InvalidControllerIndex && index < m_Controllers.size())
					m_Controllers[index].reset();
			}
		}

		bool PhysicsSync::IsRunning() const
		{
			return (m_Physics2D && m_Physics2D->IsValid())
				|| (m_Physics3D && m_Physics3D->IsValid());
		}

		void PhysicsSync::SetGravity(const glm::vec2& gravity)
		{
			if (m_Physics2D && m_Physics2D->IsValid())
				m_Physics2D->SetGravity(gravity);
		}

		void PhysicsSync::SetGravity(const glm::vec3& gravity)
		{
			if (m_Physics3D && m_Physics3D->IsValid())
				m_Physics3D->SetGravity(gravity);
		}

		CharacterController3D* PhysicsSync::GetController(const entt::registry& registry, entt::entity handle) const
		{
			if (!registry.valid(handle) || !registry.all_of<CharacterController3DComponent>(handle))
				return nullptr;

			std::uint32_t index = registry.get<CharacterController3DComponent>(handle).RuntimeController;
			if (index == CharacterController3DComponent::k_InvalidControllerIndex || index >= m_Controllers.size())
				return nullptr;

			return m_Controllers[index].get();
		}

		PhysicsBodyId2D PhysicsSync::RuntimeBody2D(const entt::registry& registry, entt::entity handle) const
		{
			if (!registry.valid(handle) || !registry.all_of<RigidBody2DComponent>(handle))
				return 0;

			return registry.get<RigidBody2DComponent>(handle).RuntimeBody;
		}

		PhysicsBodyId3D PhysicsSync::RuntimeBody3D(const entt::registry& registry, entt::entity handle) const
		{
			if (!registry.valid(handle) || !registry.all_of<RigidBody3DComponent>(handle))
				return k_InvalidBody3D;

			return registry.get<RigidBody3DComponent>(handle).RuntimeBody;
		}

		void PhysicsSync::CreateBody2D(entt::registry& registry, entt::entity handle)
		{
			if (!m_Physics2D || !m_Physics2D->IsValid())
				return;

			if (!registry.all_of<RigidBody2DComponent, TransformComponent>(handle))
				return;

			auto& rigidBody = registry.get<RigidBody2DComponent>(handle);
			if (rigidBody.RuntimeBody != 0)
				return; // a body already exists for this entity — don't leak a second one

			auto& transform = registry.get<TransformComponent>(handle);

			RigidBodyParams2D bodyParams;
			bodyParams.Type = rigidBody.Type;
			bodyParams.Position = { transform.Position.x, transform.Position.y };
			bodyParams.Rotation = glm::radians(transform.Rotation); // Transform stores degrees
			bodyParams.FixedRotation = rigidBody.FixedRotation;

			rigidBody.RuntimeBody = m_Physics2D->CreateBody(bodyParams);

			// Collider sizes are fractions of the entity's full extent (Transform.Size);
			// resolve them to world units here, so { 0.5, 0.5 } / radius 0.5 fits the quad.
			if (registry.all_of<BoxCollider2DComponent>(handle))
			{
				auto& collider = registry.get<BoxCollider2DComponent>(handle);

				BoxShapeParams2D shapeParams;
				shapeParams.HalfExtents = { transform.Size.x * collider.Size.x, transform.Size.y * collider.Size.y };
				shapeParams.Center = { collider.Offset.x * transform.Size.x, collider.Offset.y * transform.Size.y };
				shapeParams.Density = collider.Density;
				shapeParams.Friction = collider.Friction;
				shapeParams.Restitution = collider.Restitution;

				collider.RuntimeShape = m_Physics2D->AddBoxShape(rigidBody.RuntimeBody, shapeParams);
			}

			if (registry.all_of<CircleCollider2DComponent>(handle))
			{
				auto& collider = registry.get<CircleCollider2DComponent>(handle);

				CircleShapeParams2D shapeParams;
				shapeParams.Radius = transform.Size.x * collider.Radius;
				shapeParams.Center = { collider.Offset.x * transform.Size.x, collider.Offset.y * transform.Size.y };
				shapeParams.Density = collider.Density;
				shapeParams.Friction = collider.Friction;
				shapeParams.Restitution = collider.Restitution;

				collider.RuntimeShape = m_Physics2D->AddCircleShape(rigidBody.RuntimeBody, shapeParams);
			}
		}

		void PhysicsSync::CreateBody3D(entt::registry& registry, entt::entity handle)
		{
			if (!m_Physics3D || !m_Physics3D->IsValid())
				return;

			if (!registry.all_of<RigidBody3DComponent, Transform3DComponent>(handle))
				return;

			auto& rigidBody = registry.get<RigidBody3DComponent>(handle);
			if (rigidBody.RuntimeBody != k_InvalidBody3D)
				return; // a body already exists for this entity — don't leak a second one

			auto& transform = registry.get<Transform3DComponent>(handle);

			RigidBodyParams3D params;
			params.Type = rigidBody.Type;
			params.Position = transform.Position;
			params.Rotation = transform.Rotation;

			// The collider shape is baked into the body at creation. Collider sizes are
			// fractions of the entity's full extent (Transform3D.Scale), so a unit-scaled
			// entity with the default collider exactly fills its box.
			if (registry.all_of<SphereCollider3DComponent>(handle))
			{
				auto& collider = registry.get<SphereCollider3DComponent>(handle);
				params.Shape = ColliderShape3D::Sphere;
				params.Radius = transform.Scale.x * collider.Radius;
				params.Friction = collider.Friction;
				params.Restitution = collider.Restitution;
			}
			else if (registry.all_of<CapsuleCollider3DComponent>(handle))
			{
				auto& collider = registry.get<CapsuleCollider3DComponent>(handle);
				params.Shape = ColliderShape3D::Capsule;
				params.Radius = transform.Scale.x * collider.Radius;
				params.HalfHeight = transform.Scale.y * collider.HalfHeight;
				params.Friction = collider.Friction;
				params.Restitution = collider.Restitution;
			}
			else if (registry.all_of<BoxCollider3DComponent>(handle))
			{
				auto& collider = registry.get<BoxCollider3DComponent>(handle);
				params.Shape = ColliderShape3D::Box;
				params.HalfExtents = transform.Scale * collider.HalfExtents;
				params.Friction = collider.Friction;
				params.Restitution = collider.Restitution;
			}
			else
			{
				// No collider component: fall back to a box matching the transform.
				params.Shape = ColliderShape3D::Box;
				params.HalfExtents = transform.Scale * 0.5f;
			}

			rigidBody.RuntimeBody = m_Physics3D->CreateBody(params);
		}

		void PhysicsSync::CreateController(entt::registry& registry, entt::entity handle)
		{
			if (!m_Physics3D || !m_Physics3D->IsValid())
				return;

			if (!registry.all_of<CharacterController3DComponent, Transform3DComponent>(handle))
				return;

			auto& cc = registry.get<CharacterController3DComponent>(handle);
			if (cc.RuntimeController != CharacterController3DComponent::k_InvalidControllerIndex)
				return; // already created — don't leak a second controller

			auto& transform = registry.get<Transform3DComponent>(handle);

			CharacterControllerParams3D params;
			params.Radius = cc.Radius;
			params.Height = cc.Height;
			params.StepHeight = cc.StepHeight;
			params.MaxSlopeAngle = cc.MaxSlopeAngle;
			params.Position = transform.Position;
			params.Rotation = transform.Rotation;

			std::unique_ptr<CharacterController3D> controller = m_Physics3D->CreateCharacterController(params);
			if (!controller)
				return;

			m_Controllers.push_back(std::move(controller));
			cc.RuntimeController = static_cast<std::uint32_t>(m_Controllers.size() - 1);
		}

	}

}
