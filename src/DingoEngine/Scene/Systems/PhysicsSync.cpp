#include "depch.h"
#include "DingoEngine/Scene/Systems/PhysicsSync.h"

#include "DingoEngine/Scene/Components.h"
#include "DingoEngine/Scene/Systems/RuntimeComponents.h"

#include <algorithm>
#include <cmath>

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
				registry.clear<RigidBody2DRuntime>();
			}

			if (m_Physics3D && m_Physics3D->IsValid())
			{
				// Character controllers hold the world, so tear them down BEFORE the Physics3D.
				m_Controllers.clear();
				registry.clear<CharacterController3DRuntime>();

				m_Physics3D->Shutdown(); // destroys all 3D bodies
				m_Physics3D.reset();
				registry.clear<RigidBody3DRuntime>();
			}
		}

		void PhysicsSync::Step(entt::registry& registry, float deltaTime)
		{
			if (m_Physics2D && m_Physics2D->IsValid())
			{
				m_Physics2D->Step(deltaTime, m_SubStepCount);

				auto view = registry.view<RigidBody2DRuntime, RigidBody2DComponent, TransformComponent>();
				for (entt::entity handle : view)
				{
					// Static bodies never move — skip the read-back so we don't churn over
					// them or revert a runtime edit to a static entity's Transform.
					if (view.get<RigidBody2DComponent>(handle).Type == BodyType2D::Static)
						continue;

					const PhysicsBodyId2D body = view.get<RigidBody2DRuntime>(handle).Body;
					glm::vec2 position = m_Physics2D->GetPosition(body);
					float angle = m_Physics2D->GetAngle(body);

					TransformComponent& transform = view.get<TransformComponent>(handle);
					transform.Position.x = position.x;
					transform.Position.y = position.y;
					transform.Rotation = glm::degrees(angle);
				}
			}

			if (!m_Physics3D || !m_Physics3D->IsValid())
				return;

			// One collision step per 1/60 s, as Jolt recommends: a single step over a 30 fps frame
			// lets a falling body cross a mesh collider's zero-thickness triangles. The 0.1
			// tolerance keeps 60 Hz frame jitter at one step; the cap stops a stall from
			// snowballing into ever-longer frames.
			const int collisionSteps = std::clamp(static_cast<int>(std::ceil(deltaTime * 60.0f - 0.1f)), 1, k_MaxCollisionSteps);
			m_Physics3D->Step(deltaTime, collisionSteps);

			auto view = registry.view<RigidBody3DRuntime, RigidBody3DComponent, Transform3DComponent>();
			for (entt::entity handle : view)
			{
				// Static bodies never move — skip the read-back so we don't churn over
				// them or revert a runtime edit to a static entity's Transform3D.
				if (view.get<RigidBody3DComponent>(handle).Type == BodyType3D::Static)
					continue;

				const PhysicsBodyId3D body = view.get<RigidBody3DRuntime>(handle).Body;
				Transform3DComponent& transform = view.get<Transform3DComponent>(handle);
				transform.Position = m_Physics3D->GetPosition(body);
				transform.Rotation = m_Physics3D->GetRotation(body);
			}

			// Character controllers: update each (scripts set its velocity in their
			// OnUpdate), then write the swept position/rotation back onto the entity's
			// Transform3D. Their capsule "feet" position is the transform origin.
			auto ccView = registry.view<CharacterController3DRuntime, CharacterController3DComponent, Transform3DComponent>();
			for (entt::entity handle : ccView)
			{
				CharacterController3D* controller = m_Controllers[ccView.get<CharacterController3DRuntime>(handle).Index].get();
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
			if (const RigidBody2DRuntime* runtime = registry.try_get<RigidBody2DRuntime>(handle))
			{
				m_Physics2D->DestroyBody(runtime->Body);
				registry.remove<RigidBody2DRuntime>(handle);
			}

			if (const RigidBody3DRuntime* runtime = registry.try_get<RigidBody3DRuntime>(handle))
			{
				m_Physics3D->DestroyBody(runtime->Body);
				registry.remove<RigidBody3DRuntime>(handle);
			}

			// The slot stays, null, so other entities' indices remain valid.
			if (const CharacterController3DRuntime* runtime = registry.try_get<CharacterController3DRuntime>(handle))
			{
				m_Controllers[runtime->Index].reset();
				registry.remove<CharacterController3DRuntime>(handle);
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
			if (!registry.valid(handle))
				return nullptr;

			const CharacterController3DRuntime* runtime = registry.try_get<CharacterController3DRuntime>(handle);
			return runtime ? m_Controllers[runtime->Index].get() : nullptr;
		}

		PhysicsBodyId2D PhysicsSync::RuntimeBody2D(const entt::registry& registry, entt::entity handle) const
		{
			if (!registry.valid(handle))
				return 0;

			const RigidBody2DRuntime* runtime = registry.try_get<RigidBody2DRuntime>(handle);
			return runtime ? runtime->Body : 0;
		}

		PhysicsBodyId3D PhysicsSync::RuntimeBody3D(const entt::registry& registry, entt::entity handle) const
		{
			if (!registry.valid(handle))
				return k_InvalidBody3D;

			const RigidBody3DRuntime* runtime = registry.try_get<RigidBody3DRuntime>(handle);
			return runtime ? runtime->Body : k_InvalidBody3D;
		}

		void PhysicsSync::CreateBody2D(entt::registry& registry, entt::entity handle)
		{
			if (!m_Physics2D || !m_Physics2D->IsValid())
				return;

			if (!registry.all_of<RigidBody2DComponent, TransformComponent>(handle))
				return;

			if (registry.all_of<RigidBody2DRuntime>(handle))
				return; // a body already exists for this entity — don't leak a second one

			const auto& rigidBody = registry.get<RigidBody2DComponent>(handle);
			const auto& transform = registry.get<TransformComponent>(handle);

			RigidBodyParams2D bodyParams;
			bodyParams.Type = rigidBody.Type;
			bodyParams.Position = { transform.Position.x, transform.Position.y };
			bodyParams.Rotation = glm::radians(transform.Rotation); // Transform stores degrees
			bodyParams.FixedRotation = rigidBody.FixedRotation;

			const PhysicsBodyId2D body = m_Physics2D->CreateBody(bodyParams);
			if (body == 0)
				return;

			RigidBody2DRuntime& runtime = registry.emplace<RigidBody2DRuntime>(handle);
			runtime.Body = body;

			// Collider sizes are fractions of the entity's full extent (Transform.Size);
			// resolve them to world units here, so { 0.5, 0.5 } / radius 0.5 fits the quad.
			if (registry.all_of<BoxCollider2DComponent>(handle))
			{
				const auto& collider = registry.get<BoxCollider2DComponent>(handle);

				BoxShapeParams2D shapeParams;
				shapeParams.HalfExtents = { transform.Size.x * collider.Size.x, transform.Size.y * collider.Size.y };
				shapeParams.Center = { collider.Offset.x * transform.Size.x, collider.Offset.y * transform.Size.y };
				shapeParams.Density = collider.Density;
				shapeParams.Friction = collider.Friction;
				shapeParams.Restitution = collider.Restitution;

				runtime.BoxShape = m_Physics2D->AddBoxShape(body, shapeParams);
			}

			if (registry.all_of<CircleCollider2DComponent>(handle))
			{
				const auto& collider = registry.get<CircleCollider2DComponent>(handle);

				CircleShapeParams2D shapeParams;
				shapeParams.Radius = transform.Size.x * collider.Radius;
				shapeParams.Center = { collider.Offset.x * transform.Size.x, collider.Offset.y * transform.Size.y };
				shapeParams.Density = collider.Density;
				shapeParams.Friction = collider.Friction;
				shapeParams.Restitution = collider.Restitution;

				runtime.CircleShape = m_Physics2D->AddCircleShape(body, shapeParams);
			}
		}

		void PhysicsSync::CreateBody3D(entt::registry& registry, entt::entity handle)
		{
			if (!m_Physics3D || !m_Physics3D->IsValid())
				return;

			if (!registry.all_of<RigidBody3DComponent, Transform3DComponent>(handle))
				return;

			if (registry.all_of<RigidBody3DRuntime>(handle))
				return; // a body already exists for this entity — don't leak a second one

			const auto& rigidBody = registry.get<RigidBody3DComponent>(handle);
			const auto& transform = registry.get<Transform3DComponent>(handle);

			RigidBodyParams3D params;
			params.Type = rigidBody.Type;
			params.Position = transform.Position;
			params.Rotation = transform.Rotation;
			params.ContinuousCollision = rigidBody.ContinuousCollision;

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
			else if (registry.all_of<MeshCollider3DComponent>(handle))
			{
				auto& collider = registry.get<MeshCollider3DComponent>(handle);
				params.Shape = collider.Convex ? ColliderShape3D::ConvexHull : ColliderShape3D::Mesh;
				params.Mesh = collider.Mesh;
				if (!params.Mesh && registry.all_of<MeshRendererComponent>(handle))
					params.Mesh = registry.get<MeshRendererComponent>(handle).Mesh;
				if (!params.Mesh)
				{
					const std::string name = registry.all_of<TagComponent>(handle) ? registry.get<TagComponent>(handle).Tag : std::string();
					DE_CORE_ERROR("MeshCollider3DComponent on '{}' has no Mesh and no MeshRendererComponent::Mesh to fall back on; no body created", name);
					return;
				}
				params.MeshScale = transform.Scale;
				params.Friction = collider.Friction;
				params.Restitution = collider.Restitution;
			}
			else
			{
				// No collider component: fall back to a box matching the transform.
				params.Shape = ColliderShape3D::Box;
				params.HalfExtents = transform.Scale * 0.5f;
			}

			const PhysicsBodyId3D body = m_Physics3D->CreateBody(params);
			if (body != k_InvalidBody3D)
				registry.emplace<RigidBody3DRuntime>(handle).Body = body;
		}

		void PhysicsSync::CreateController(entt::registry& registry, entt::entity handle)
		{
			if (!m_Physics3D || !m_Physics3D->IsValid())
				return;

			if (!registry.all_of<CharacterController3DComponent, Transform3DComponent>(handle))
				return;

			if (registry.all_of<CharacterController3DRuntime>(handle))
				return; // already created — don't leak a second controller

			const auto& cc = registry.get<CharacterController3DComponent>(handle);
			const auto& transform = registry.get<Transform3DComponent>(handle);

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
			registry.emplace<CharacterController3DRuntime>(handle).Index = static_cast<std::uint32_t>(m_Controllers.size() - 1);
		}

	}

}
