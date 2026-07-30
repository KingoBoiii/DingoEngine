#pragma once

// Engine-internal: owns a scene's physics worlds and keeps entity transforms and
// simulation bodies in step. Lives under src/ so EnTT stays a private
// implementation detail.

#include "DingoEngine/Physics/2D/Physics2D.h"
#include "DingoEngine/Physics/3D/Physics3D.h"
#include "DingoEngine/Physics/3D/CharacterController3D.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

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
			// Creates a 2D world only if the registry has 2D rigid bodies, and a 3D world
			// only if it has 3D rigid bodies or character controllers, so a scene pays for
			// just the dimension it uses. Bakes a body for every qualifying entity.
			void Start(entt::registry& registry, const glm::vec2& gravity2D, const glm::vec3& gravity3D);

			// Tears both worlds down and resets every runtime handle in the registry to
			// its "none" sentinel, so a later Start is clean.
			void Stop(entt::registry& registry);

			// Steps each live world and writes the simulated transforms back: 2D onto
			// TransformComponent, 3D (and character controllers) onto Transform3DComponent.
			void Step(entt::registry& registry, float deltaTime);

			// Instantiates whatever body/controller the entity's components call for.
			// Each route no-ops if its world isn't live or the component is absent.
			void CreateBodiesForEntity(entt::registry& registry, entt::entity handle);

			// Releases the entity's body/shapes and frees its controller slot, so nothing
			// keeps colliding after the entity is gone.
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
			void CreateBody2D(entt::registry& registry, entt::entity handle);
			void CreateBody3D(entt::registry& registry, entt::entity handle);
			void CreateController(entt::registry& registry, entt::entity handle);

		private:
			// The backends (Box2D / Jolt) live behind the Physics2D / Physics3D
			// interfaces; these exist only between Start and Stop.
			std::unique_ptr<Physics2D> m_Physics2D;
			std::unique_ptr<Physics3D> m_Physics3D;

			int m_SubStepCount = 4;
			int m_CollisionSteps = 1;

			// One per CharacterController3DComponent. The component's RuntimeController
			// field indexes into this vector; slots are never reused (a destroyed
			// controller leaves a null hole) so indices stay stable for the world's
			// lifetime. Cleared in Stop with the 3D world.
			std::vector<std::unique_ptr<CharacterController3D>> m_Controllers;
		};

	}

}
