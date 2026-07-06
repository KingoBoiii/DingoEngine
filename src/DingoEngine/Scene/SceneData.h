#pragma once

// Engine-internal: this header lives under src/ and is NEVER shipped or included
// by client code. It is the only place the EnTT registry is named, which keeps
// EnTT a private implementation detail of the engine.

#include "DingoEngine/Core/UUID.h"
#include "DingoEngine/Physics/2D/Physics2D.h"
#include "DingoEngine/Physics/3D/Physics3D.h"

#include <entt/entt.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

namespace Dingo
{

	class ScriptableEntity;

	namespace Internal
	{

		struct SceneData
		{
			entt::registry Registry;
			std::unordered_map<UUID, entt::entity> EntityMap;

			// Behaviours attached to entities, keyed by entity handle.
			std::unordered_map<entt::entity, std::unique_ptr<ScriptableEntity>> Scripts;

			// Deferred-destruction support: while scripts are updating we queue
			// destroys and apply them after the update pass, so a script can safely
			// destroy its own (or another) entity mid-update.
			bool Updating = false;
			std::vector<entt::entity> PendingDestroy;

			// Physics (2D). The backend (Box2D) lives behind the Physics2D
			// interface; this instance exists only between OnPhysicsStart/Stop.
			std::unique_ptr<Physics2D> Physics;
			int PhysicsSubStepCount = 4;

			// Physics (3D). The backend (Jolt) lives behind the Physics3D interface;
			// created on OnPhysicsStart only if the scene has 3D rigid bodies OR character
			// controllers, so a purely-2D scene never pays for a Jolt world (and vice versa).
			std::unique_ptr<Physics3D> Physics3D;
			int PhysicsCollisionSteps = 1;

			// Character controllers owned by the Scene, one per CharacterController3DComponent.
			// The component's RuntimeController field indexes into this vector; slots are
			// never reused (a destroyed controller leaves a null hole) so indices stay stable
			// for the world's lifetime. Cleared in OnPhysicsStop with the Physics3D world.
			std::vector<std::unique_ptr<CharacterController3D>> CharacterControllers;
		};

	}

}
