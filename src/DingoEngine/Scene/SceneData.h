#pragma once

// Engine-internal: this header lives under src/ and is NEVER shipped or included
// by client code. It is the only place the EnTT registry is named, which keeps
// EnTT a private implementation detail of the engine.

#include "DingoEngine/Core/UUID.h"
#include "DingoEngine/Scene/Systems/PhysicsSync.h"
#include "DingoEngine/Scene/Systems/ScriptSystem.h"

#include <entt/entt.hpp>

#include <unordered_map>
#include <utility>
#include <vector>

namespace Dingo
{

	namespace Internal
	{

		struct SceneData
		{
			entt::registry Registry;
			std::unordered_map<UUID, entt::entity> EntityMap;

			ScriptSystem Scripts;
			PhysicsSync Physics;

			// Deferred-destruction support: while scripts are updating we queue
			// destroys and apply them after the update pass, so a script can safely
			// destroy its own (or another) entity mid-update.
			bool Updating = false;
			std::vector<entt::entity> PendingDestroy;

			// Reused every frame by the sprite z-sort in RenderEntities: clear() keeps the
			// capacity, so a steady-state frame allocates nothing to sort.
			std::vector<std::pair<float, entt::entity>> SpriteSortBuffer;
		};

	}

}
