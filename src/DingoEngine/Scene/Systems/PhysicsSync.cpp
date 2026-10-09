#include "depch.h"
#include "DingoEngine/Scene/Systems/PhysicsSync.h"

#include "DingoEngine/Scene/Components.h"
#include "DingoEngine/Scene/Systems/HierarchySystem.h"
#include "DingoEngine/Scene/Systems/RuntimeComponents.h"

#include <algorithm>
#include <cmath>

namespace Dingo
{

	namespace Internal
	{

		namespace
		{
			std::uint64_t PackPair(std::uint32_t first, std::uint32_t second)
			{
				return (static_cast<std::uint64_t>(first) << 32) | second;
			}

			// Withdraws every pair in `applied` that `wanted` no longer has, then (re)applies all of
			// `wanted`, which becomes the new `applied`. apply(first, second, ignore) must be a no-op
			// for a pair that is already in that state.
			template<typename Apply>
			void SyncPairs(std::vector<std::uint64_t>& applied, std::vector<std::uint64_t>& wanted, Apply apply)
			{
				if (applied.empty() && wanted.empty())
					return;

				std::sort(wanted.begin(), wanted.end());
				wanted.erase(std::unique(wanted.begin(), wanted.end()), wanted.end());

				for (std::uint64_t pair : applied)
				{
					if (!std::binary_search(wanted.begin(), wanted.end(), pair))
						apply(static_cast<std::uint32_t>(pair >> 32), static_cast<std::uint32_t>(pair), false);
				}

				for (std::uint64_t pair : wanted)
					apply(static_cast<std::uint32_t>(pair >> 32), static_cast<std::uint32_t>(pair), true);

				applied.swap(wanted);
			}
		}

		void PhysicsSync::Start(entt::registry& registry, const glm::vec2& gravity2D, const glm::vec3& gravity3D)
		{
			m_Memo.Begin(registry);

			auto rb2dView = registry.view<RigidBody2DComponent>();
			if ((!m_Physics2D || !m_Physics2D->IsValid()) && rb2dView.begin() != rb2dView.end())
			{
				m_Physics2D.reset(Physics2D::Create());
				m_Physics2D->Initialize(gravity2D);

				for (entt::entity handle : rb2dView)
					CreateBody2D(registry, handle, &m_Memo);
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
					CreateBody3D(registry, handle, &m_Memo);

				for (entt::entity handle : cc3dView)
					CreateController(registry, handle, &m_Memo);
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
				m_IgnoredPairs.clear();
				m_IgnoredControllerBodies.clear();
			}
		}

		void PhysicsSync::Step(entt::registry& registry, float deltaTime)
		{
			if (m_Physics2D && m_Physics2D->IsValid())
			{
				DriveKinematicChildren2D(registry, deltaTime);
				m_Physics2D->Step(deltaTime, m_SubStepCount);

				m_ChildWriteBacks2D.clear();

				auto view = registry.view<RigidBody2DRuntime, RigidBody2DComponent, TransformComponent>();
				for (entt::entity handle : view)
				{
					// Static bodies never move — skip the read-back so we don't churn over
					// them or revert a runtime edit to a static entity's Transform.
					const BodyType2D type = view.get<RigidBody2DComponent>(handle).Type;
					if (type == BodyType2D::Static)
						continue;

					const PhysicsBodyId2D body = view.get<RigidBody2DRuntime>(handle).Body;
					glm::vec2 position = m_Physics2D->GetPosition(body);
					float angle = m_Physics2D->GetAngle(body);

					if (HierarchySystem::GetParent(registry, handle) != entt::null)
					{
						if (type != BodyType2D::Kinematic)
							m_ChildWriteBacks2D.push_back({ handle, 0, position, angle });
						continue;
					}

					TransformComponent& transform = view.get<TransformComponent>(handle);
					transform.Position.x = position.x;
					transform.Position.y = position.y;
					transform.Rotation = glm::degrees(angle);
				}

				WriteBackChildren2D(registry);
			}

			if (!m_Physics3D || !m_Physics3D->IsValid())
				return;

			// One collision step per 1/60 s, as Jolt recommends: a single step over a 30 fps frame
			// lets a falling body cross a mesh collider's zero-thickness triangles. The 0.1
			// tolerance keeps 60 Hz frame jitter at one step. Scene::OnUpdate caps the delta at
			// k_MaxStepTime, so a stall never stretches a step past 1/60 s.
			const int collisionSteps = std::clamp(static_cast<int>(std::ceil(deltaTime * 60.0f - 0.1f)), 1, k_MaxCollisionSteps);

			SyncAncestorFilters(registry);
			DriveKinematicChildren(registry, deltaTime);
			m_Physics3D->Step(deltaTime, collisionSteps);

			m_ChildWriteBacks.clear();

			auto view = registry.view<RigidBody3DRuntime, RigidBody3DComponent, Transform3DComponent>();
			for (entt::entity handle : view)
			{
				// Static bodies never move — skip the read-back so we don't churn over
				// them or revert a runtime edit to a static entity's Transform3D.
				const BodyType3D type = view.get<RigidBody3DComponent>(handle).Type;
				if (type == BodyType3D::Static)
					continue;

				const PhysicsBodyId3D body = view.get<RigidBody3DRuntime>(handle).Body;
				if (HierarchySystem::GetParent(registry, handle) != entt::null)
				{
					if (type != BodyType3D::Kinematic)
						m_ChildWriteBacks.push_back({ handle, 0, m_Physics3D->GetPosition(body), m_Physics3D->GetRotation(body) });
					continue;
				}

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

				if (HierarchySystem::GetParent(registry, handle) != entt::null)
				{
					m_ChildWriteBacks.push_back({ handle, 0, controller->GetPosition(), controller->GetRotation() });
					continue;
				}

				Transform3DComponent& transform = ccView.get<Transform3DComponent>(handle);
				transform.Position = controller->GetPosition();
				transform.Rotation = controller->GetRotation();
			}

			WriteBackChildren(registry);
		}

		void PhysicsSync::SyncAncestorFilters(entt::registry& registry)
		{
			m_WantedPairs.clear();
			m_WantedControllerBodies.clear();

			auto view = registry.view<RigidBody3DRuntime, RigidBody3DComponent, HierarchyComponent>();
			for (entt::entity handle : view)
			{
				const entt::entity parent = view.get<HierarchyComponent>(handle).Parent;
				if (parent == entt::null || view.get<RigidBody3DComponent>(handle).Type != BodyType3D::Kinematic)
					continue;

				const PhysicsBodyId3D body = view.get<RigidBody3DRuntime>(handle).Body;
				for (entt::entity ancestor = parent; ancestor != entt::null; ancestor = HierarchySystem::GetParent(registry, ancestor))
				{
					if (const RigidBody3DRuntime* runtime = registry.try_get<RigidBody3DRuntime>(ancestor))
						m_WantedPairs.push_back(PackPair(body, runtime->Body));
					if (const CharacterController3DRuntime* slot = registry.try_get<CharacterController3DRuntime>(ancestor))
						m_WantedControllerBodies.push_back(PackPair(slot->Index, body));
				}
			}

			SyncPairs(m_IgnoredPairs, m_WantedPairs, [this](std::uint32_t child, std::uint32_t ancestor, bool ignore)
			{
				m_Physics3D->IgnoreCollision(child, ancestor, ignore);
			});

			SyncPairs(m_IgnoredControllerBodies, m_WantedControllerBodies, [this](std::uint32_t slot, std::uint32_t body, bool ignore)
			{
				if (slot < m_Controllers.size() && m_Controllers[slot])
					m_Controllers[slot]->IgnoreBody(body, ignore);
			});
		}

		void PhysicsSync::DriveKinematicChildren2D(entt::registry& registry, float deltaTime)
		{
			m_KinematicChildren.clear();
			auto view = registry.view<RigidBody2DRuntime, RigidBody2DComponent, HierarchyComponent>();
			for (entt::entity handle : view)
			{
				if (view.get<RigidBody2DComponent>(handle).Type == BodyType2D::Kinematic && view.get<HierarchyComponent>(handle).Parent != entt::null)
					m_KinematicChildren.push_back({ handle, HierarchySystem::Depth(registry, handle) });
			}

			std::stable_sort(m_KinematicChildren.begin(), m_KinematicChildren.end(), [](const KinematicChild& a, const KinematicChild& b)
			{
				return a.Depth < b.Depth;
			});

			BeginPrediction(registry);
			for (const KinematicChild& child : m_KinematicChildren)
			{
				glm::vec3 position;
				float rotation;
				PredictedWorldPose2D(registry, HierarchySystem::GetParent(registry, child.Handle), deltaTime, position, rotation);
				if (const TransformComponent* local = registry.try_get<TransformComponent>(child.Handle))
					HierarchySystem::Compose2D(position, rotation, *local);

				m_Physics2D->MoveKinematic(registry.get<RigidBody2DRuntime>(child.Handle).Body, glm::vec2(position), glm::radians(rotation), deltaTime);
			}
		}

		void PhysicsSync::BeginPrediction(const entt::registry& registry)
		{
			m_Memo.Begin(registry);
			if (++m_PredictionPass == 0)
			{
				for (PredictedEntry& entry : m_Predicted)
					entry.Pass = 0;
				m_PredictionPass = 1;
			}
		}

		PhysicsSync::PredictedEntry& PhysicsSync::Predicted(entt::entity handle)
		{
			const std::size_t index = static_cast<std::size_t>(entt::to_entity(handle));
			if (index >= m_Predicted.size())
				m_Predicted.resize(index + 1);
			return m_Predicted[index];
		}

		bool PhysicsSync::PredictedPose2D(const entt::registry& registry, entt::entity handle, float deltaTime, glm::vec3& position, float& rotation)
		{
			const RigidBody2DRuntime* body = registry.try_get<RigidBody2DRuntime>(handle);
			const RigidBody2DComponent* rigidBody = registry.try_get<RigidBody2DComponent>(handle);
			if (!body || !rigidBody || rigidBody->Type == BodyType2D::Static)
				return false;

			float unusedRotation;
			m_Memo.Pose2D(handle, position, unusedRotation);
			const glm::vec2 xy = m_Physics2D->GetPosition(body->Body) + m_Physics2D->GetLinearVelocity(body->Body) * deltaTime;
			position = { xy.x, xy.y, position.z };
			rotation = glm::degrees(m_Physics2D->GetAngle(body->Body) + m_Physics2D->GetAngularVelocity(body->Body) * deltaTime);
			return true;
		}

		void PhysicsSync::PredictedWorldPose2D(const entt::registry& registry, entt::entity handle, float deltaTime, glm::vec3& position, float& rotation)
		{
			position = glm::vec3(0.0f);
			rotation = 0.0f;
			m_PredictionChain.clear();
			for (entt::entity e = handle; e != entt::null; e = HierarchySystem::GetParent(registry, e))
			{
				const PredictedEntry& known = Predicted(e);
				if (known.Pass == m_PredictionPass)
				{
					position = known.Position;
					rotation = known.Rotation;
					break;
				}
				if (PredictedPose2D(registry, e, deltaTime, position, rotation))
				{
					PredictedEntry& entry = Predicted(e);
					entry.Position = position;
					entry.Rotation = rotation;
					entry.Pass = m_PredictionPass;
					break;
				}
				m_PredictionChain.push_back(e);
			}

			for (auto it = m_PredictionChain.rbegin(); it != m_PredictionChain.rend(); ++it)
			{
				if (const TransformComponent* local = registry.try_get<TransformComponent>(*it))
					HierarchySystem::Compose2D(position, rotation, *local);

				PredictedEntry& entry = Predicted(*it);
				entry.Position = position;
				entry.Rotation = rotation;
				entry.Pass = m_PredictionPass;
			}
		}

		void PhysicsSync::WriteBackChildren2D(entt::registry& registry)
		{
			if (m_ChildWriteBacks2D.empty())
				return;

			m_Memo.Begin(registry);
			for (ChildWriteBack2D& writeBack : m_ChildWriteBacks2D)
				writeBack.Depth = m_Memo.Depth(writeBack.Handle);

			std::stable_sort(m_ChildWriteBacks2D.begin(), m_ChildWriteBacks2D.end(), [](const ChildWriteBack2D& a, const ChildWriteBack2D& b)
			{
				return a.Depth < b.Depth;
			});

			for (const ChildWriteBack2D& writeBack : m_ChildWriteBacks2D)
			{
				if (TransformComponent* transform = registry.try_get<TransformComponent>(writeBack.Handle))
				{
					m_Memo.SetWorldXY2D(writeBack.Handle, *transform, writeBack.Position);
					m_Memo.SetWorldRotation2D(writeBack.Handle, *transform, glm::degrees(writeBack.Angle));
				}
			}
		}

		void PhysicsSync::DriveKinematicChildren(entt::registry& registry, float deltaTime)
		{
			m_KinematicChildren.clear();
			auto view = registry.view<RigidBody3DRuntime, RigidBody3DComponent, HierarchyComponent>();
			for (entt::entity handle : view)
			{
				if (view.get<RigidBody3DComponent>(handle).Type == BodyType3D::Kinematic && view.get<HierarchyComponent>(handle).Parent != entt::null)
					m_KinematicChildren.push_back({ handle, HierarchySystem::Depth(registry, handle) });
			}

			// Shallowest first: a kinematic parent's MoveKinematic sets the velocity its children are
			// predicted from.
			std::stable_sort(m_KinematicChildren.begin(), m_KinematicChildren.end(), [](const KinematicChild& a, const KinematicChild& b)
			{
				return a.Depth < b.Depth;
			});

			BeginPrediction(registry);
			for (const KinematicChild& child : m_KinematicChildren)
			{
				const glm::mat4 world = PredictedWorldTransform(registry, HierarchySystem::GetParent(registry, child.Handle), deltaTime)
					* HierarchySystem::LinkTransform(registry, child.Handle);

				glm::vec3 position, scale;
				glm::quat rotation;
				HierarchySystem::Decompose(world, position, rotation, scale);
				m_Physics3D->MoveKinematic(registry.get<RigidBody3DRuntime>(child.Handle).Body, position, rotation, deltaTime);
			}
		}

		bool PhysicsSync::PredictedPose(const entt::registry& registry, entt::entity handle, float deltaTime, glm::mat4& world)
		{
			glm::vec3 position;
			glm::quat rotation;

			const RigidBody3DRuntime* body = registry.try_get<RigidBody3DRuntime>(handle);
			const RigidBody3DComponent* rigidBody = registry.try_get<RigidBody3DComponent>(handle);
			const CharacterController3DRuntime* controllerSlot = registry.try_get<CharacterController3DRuntime>(handle);
			const CharacterController3D* controller = controllerSlot ? m_Controllers[controllerSlot->Index].get() : nullptr;

			if (body && rigidBody && rigidBody->Type != BodyType3D::Static)
			{
				// The same integration Jolt applies over the step: exact for a MoveKinematic'd body,
				// and blind to this step's contacts and gravity for a dynamic one.
				position = m_Physics3D->GetPosition(body->Body) + m_Physics3D->GetLinearVelocity(body->Body) * deltaTime;
				rotation = m_Physics3D->GetRotation(body->Body);
				const glm::vec3 angularStep = m_Physics3D->GetAngularVelocity(body->Body) * deltaTime;
				const float angle = glm::length(angularStep);
				if (angle > 1e-6f)
					rotation = glm::normalize(glm::angleAxis(angle, angularStep / angle) * rotation);
			}
			else if (controller)
			{
				position = controller->GetPosition() + controller->GetLinearVelocity() * deltaTime;
				rotation = controller->GetRotation();
			}
			else
			{
				return false;
			}

			world = glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), m_Memo.Scale(handle));
			return true;
		}

		glm::mat4 PhysicsSync::PredictedWorldTransform(const entt::registry& registry, entt::entity handle, float deltaTime)
		{
			// Climbs to an ancestor already predicted this pass, or the nearest one whose pose physics
			// decides this step, then composes the links below it root-first, the same order
			// HierarchySystem::WorldTransform uses; every entity on the way is kept for its other
			// descendants.
			glm::mat4 world(1.0f);
			m_PredictionChain.clear();
			for (entt::entity e = handle; e != entt::null; e = HierarchySystem::GetParent(registry, e))
			{
				const PredictedEntry& known = Predicted(e);
				if (known.Pass == m_PredictionPass)
				{
					world = known.World;
					break;
				}
				if (PredictedPose(registry, e, deltaTime, world))
				{
					PredictedEntry& entry = Predicted(e);
					entry.World = world;
					entry.Pass = m_PredictionPass;
					break;
				}
				m_PredictionChain.push_back(e);
			}

			for (auto it = m_PredictionChain.rbegin(); it != m_PredictionChain.rend(); ++it)
			{
				world = world * HierarchySystem::LinkTransform(registry, *it);
				PredictedEntry& entry = Predicted(*it);
				entry.World = world;
				entry.Pass = m_PredictionPass;
			}

			return world;
		}

		void PhysicsSync::WriteBackChildren(entt::registry& registry)
		{
			if (m_ChildWriteBacks.empty())
				return;

			// Shallowest first: a simulated child's local is solved against its parent's world, which
			// must already hold this step's result when the parent is simulated too.
			m_Memo.Begin(registry);
			for (ChildWriteBack& writeBack : m_ChildWriteBacks)
				writeBack.Depth = m_Memo.Depth(writeBack.Handle);

			std::stable_sort(m_ChildWriteBacks.begin(), m_ChildWriteBacks.end(), [](const ChildWriteBack& a, const ChildWriteBack& b)
			{
				return a.Depth < b.Depth;
			});

			for (const ChildWriteBack& writeBack : m_ChildWriteBacks)
			{
				Transform3DComponent& transform = registry.get<Transform3DComponent>(writeBack.Handle);
				m_Memo.SetWorldPosition(writeBack.Handle, transform, writeBack.Position);
				m_Memo.SetWorldRotation(writeBack.Handle, transform, writeBack.Rotation);
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

		void PhysicsSync::CreateBody2D(entt::registry& registry, entt::entity handle, HierarchySystem::WorldMemo* memo)
		{
			if (!m_Physics2D || !m_Physics2D->IsValid())
				return;

			if (!registry.all_of<RigidBody2DComponent, TransformComponent>(handle))
				return;

			if (registry.all_of<RigidBody2DRuntime>(handle))
				return; // a body already exists for this entity — don't leak a second one

			const auto& rigidBody = registry.get<RigidBody2DComponent>(handle);
			const auto& transform = registry.get<TransformComponent>(handle);

			glm::vec3 position;
			float rotation;
			if (memo)
				memo->Pose2D(handle, transform, position, rotation);
			else
				HierarchySystem::WorldPose2D(registry, handle, transform, position, rotation);

			RigidBodyParams2D bodyParams;
			bodyParams.Type = rigidBody.Type;
			bodyParams.Position = { position.x, position.y };
			bodyParams.Rotation = glm::radians(rotation); // Transform stores degrees
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

		void PhysicsSync::CreateBody3D(entt::registry& registry, entt::entity handle, HierarchySystem::WorldMemo* memo)
		{
			if (!m_Physics3D || !m_Physics3D->IsValid())
				return;

			if (!registry.all_of<RigidBody3DComponent, Transform3DComponent>(handle))
				return;

			if (registry.all_of<RigidBody3DRuntime>(handle))
				return; // a body already exists for this entity — don't leak a second one

			const auto& rigidBody = registry.get<RigidBody3DComponent>(handle);

			glm::vec3 scale;
			RigidBodyParams3D params;
			const Transform3DComponent& transform = registry.get<Transform3DComponent>(handle);
			if (memo)
				memo->Pose(handle, transform, params.Position, params.Rotation, scale);
			else
				HierarchySystem::WorldPose(registry, handle, transform, params.Position, params.Rotation, scale);
			params.Type = rigidBody.Type;
			params.ContinuousCollision = rigidBody.ContinuousCollision;

			// The collider shape is baked into the body at creation. Collider sizes are
			// fractions of the entity's full extent (its world scale), so a unit-scaled
			// entity with the default collider exactly fills its box. A mirrored axis keeps a
			// primitive's size positive; only the mesh collider takes the sign.
			const glm::vec3 size = glm::abs(scale);
			if (registry.all_of<SphereCollider3DComponent>(handle))
			{
				auto& collider = registry.get<SphereCollider3DComponent>(handle);
				params.Shape = ColliderShape3D::Sphere;
				params.Radius = size.x * collider.Radius;
				params.Friction = collider.Friction;
				params.Restitution = collider.Restitution;
			}
			else if (registry.all_of<CapsuleCollider3DComponent>(handle))
			{
				auto& collider = registry.get<CapsuleCollider3DComponent>(handle);
				params.Shape = ColliderShape3D::Capsule;
				params.Radius = size.x * collider.Radius;
				params.HalfHeight = size.y * collider.HalfHeight;
				params.Friction = collider.Friction;
				params.Restitution = collider.Restitution;
			}
			else if (registry.all_of<BoxCollider3DComponent>(handle))
			{
				auto& collider = registry.get<BoxCollider3DComponent>(handle);
				params.Shape = ColliderShape3D::Box;
				params.HalfExtents = size * collider.HalfExtents;
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
				params.MeshScale = scale;
				params.Friction = collider.Friction;
				params.Restitution = collider.Restitution;
			}
			else
			{
				// No collider component: fall back to a box matching the transform.
				params.Shape = ColliderShape3D::Box;
				params.HalfExtents = size * 0.5f;
			}

			const PhysicsBodyId3D body = m_Physics3D->CreateBody(params);
			if (body != k_InvalidBody3D)
				registry.emplace<RigidBody3DRuntime>(handle).Body = body;
		}

		void PhysicsSync::CreateController(entt::registry& registry, entt::entity handle, HierarchySystem::WorldMemo* memo)
		{
			if (!m_Physics3D || !m_Physics3D->IsValid())
				return;

			if (!registry.all_of<CharacterController3DComponent, Transform3DComponent>(handle))
				return;

			if (registry.all_of<CharacterController3DRuntime>(handle))
				return; // already created — don't leak a second controller

			const auto& cc = registry.get<CharacterController3DComponent>(handle);

			glm::vec3 scale;
			CharacterControllerParams3D params;
			params.Radius = cc.Radius;
			params.Height = cc.Height;
			params.StepHeight = cc.StepHeight;
			params.MaxSlopeAngle = cc.MaxSlopeAngle;
			params.CollideWithCharacters = cc.CollideWithCharacters;
			const Transform3DComponent& transform = registry.get<Transform3DComponent>(handle);
			if (memo)
				memo->Pose(handle, transform, params.Position, params.Rotation, scale);
			else
				HierarchySystem::WorldPose(registry, handle, transform, params.Position, params.Rotation, scale);

			std::unique_ptr<CharacterController3D> controller = m_Physics3D->CreateCharacterController(params);
			if (!controller)
				return;

			m_Controllers.push_back(std::move(controller));
			registry.emplace<CharacterController3DRuntime>(handle).Index = static_cast<std::uint32_t>(m_Controllers.size() - 1);
		}

	}

}
