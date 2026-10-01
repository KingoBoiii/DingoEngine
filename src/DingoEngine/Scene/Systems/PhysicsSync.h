#pragma once

// Engine-internal: owns a scene's physics worlds and keeps entity transforms and
// simulation bodies in step. Lives under src/ so EnTT stays a private
// implementation detail.

#include "DingoEngine/Physics/2D/Physics2D.h"
#include "DingoEngine/Physics/3D/Physics3D.h"
#include "DingoEngine/Physics/3D/CharacterController3D.h"
#include "DingoEngine/Scene/Systems/HierarchySystem.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace Dingo
{

	namespace Internal
	{

		class PhysicsSync
		{
		public:
			static constexpr int k_MaxCollisionSteps = 4;
			static constexpr float k_MaxStepTime = k_MaxCollisionSteps / 60.0f;

			// Creates a 2D world only if the registry has 2D rigid bodies, and a 3D world
			// only if it has 3D rigid bodies or character controllers, so a scene pays for
			// just the dimension it uses. Bakes a body for every qualifying entity.
			void Start(entt::registry& registry, const glm::vec2& gravity2D, const glm::vec3& gravity3D);

			// Tears both worlds down and drops every entity's runtime body/controller
			// component with them, so a later Start is clean.
			void Stop(entt::registry& registry);

			// Steps each live world and writes the simulated transforms back: 2D onto
			// TransformComponent, 3D (and character controllers) onto Transform3DComponent. A child
			// stores the local transform that reproduces its simulated world pose, except a
			// kinematic child, which is driven before the step to where its parent will be after it.
			void Step(entt::registry& registry, float deltaTime);

			// Instantiates whatever body/controller the entity's components call for.
			// Each route no-ops if its world isn't live or the component is absent.
			void CreateBodiesForEntity(entt::registry& registry, entt::entity handle);

			// Releases the body/shapes and controller the entity owns — even if its settings
			// component has since been removed — so nothing keeps colliding after it is gone.
			void DestroyBodiesForEntity(entt::registry& registry, entt::entity handle);

			bool IsRunning() const;

			Physics2D* Get2D() const { return m_Physics2D.get(); }
			Physics3D* Get3D() const { return m_Physics3D.get(); }

			void SetGravity(const glm::vec2& gravity);
			void SetGravity(const glm::vec3& gravity);

			CharacterController3D* GetController(const entt::registry& registry, entt::entity handle) const;

			// Opaque runtime handles for an entity, or the "none" sentinel when it has none.
			PhysicsBodyId2D RuntimeBody2D(const entt::registry& registry, entt::entity handle) const;
			PhysicsBodyId3D RuntimeBody3D(const entt::registry& registry, entt::entity handle) const;

		private:
			// Start's bake passes `memo`; a single late body works its pose out on demand.
			void CreateBody2D(entt::registry& registry, entt::entity handle, HierarchySystem::WorldMemo* memo = nullptr);
			void CreateBody3D(entt::registry& registry, entt::entity handle, HierarchySystem::WorldMemo* memo = nullptr);
			void CreateController(entt::registry& registry, entt::entity handle, HierarchySystem::WorldMemo* memo = nullptr);
			void WriteBackChildren(entt::registry& registry);
			void WriteBackChildren2D(entt::registry& registry);
			void DriveKinematicChildren(entt::registry& registry, float deltaTime);
			void DriveKinematicChildren2D(entt::registry& registry, float deltaTime);
			void PredictedWorldPose2D(const entt::registry& registry, entt::entity handle, float deltaTime, glm::vec3& position, float& rotation);
			bool PredictedPose2D(const entt::registry& registry, entt::entity handle, float deltaTime, glm::vec3& position, float& rotation);
			// The world transform `handle` will have after this step: the end-of-step pose of its
			// nearest ancestor (itself included) with a moving body or a controller, times the
			// locals below it.
			glm::mat4 PredictedWorldTransform(const entt::registry& registry, entt::entity handle, float deltaTime);
			bool PredictedPose(const entt::registry& registry, entt::entity handle, float deltaTime, glm::mat4& world);

			// Opens a kinematic-follow pass, forgetting every earlier prediction.
			void BeginPrediction(const entt::registry& registry);

		private:
			struct ChildWriteBack
			{
				entt::entity Handle;
				std::uint32_t Depth;
				glm::vec3 Position;
				glm::quat Rotation;
			};

			struct ChildWriteBack2D
			{
				entt::entity Handle;
				std::uint32_t Depth;
				glm::vec2 Position;
				float Angle; // radians
			};

			struct KinematicChild
			{
				entt::entity Handle;
				std::uint32_t Depth;
			};

			// An entity's end-of-step world in the current kinematic-follow pass: World in 3D,
			// Position/Rotation in 2D.
			struct PredictedEntry
			{
				glm::mat4 World;
				glm::vec3 Position;
				float Rotation;
				std::uint32_t Pass = 0;
			};

			PredictedEntry& Predicted(entt::entity handle);

			// The backends (Box2D / Jolt) live behind the Physics2D / Physics3D
			// interfaces; these exist only between Start and Stop.
			std::unique_ptr<Physics2D> m_Physics2D;
			std::unique_ptr<Physics3D> m_Physics3D;

			int m_SubStepCount = 4;

			// One per CharacterController3DComponent, indexed by CharacterController3DRuntime.
			// Slots are never reused (a destroyed controller leaves a null hole) so indices
			// stay stable for the world's lifetime. Cleared in Stop with the 3D world.
			std::vector<std::unique_ptr<CharacterController3D>> m_Controllers;

			std::vector<ChildWriteBack> m_ChildWriteBacks;
			std::vector<ChildWriteBack2D> m_ChildWriteBacks2D;
			std::vector<KinematicChild> m_KinematicChildren;
			std::vector<entt::entity> m_PredictionChain;
			std::vector<PredictedEntry> m_Predicted; // indexed by entity
			std::uint32_t m_PredictionPass = 0;
			HierarchySystem::WorldMemo m_Memo;
		};

	}

}
