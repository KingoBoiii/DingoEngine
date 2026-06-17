#pragma once

// Engine-internal: this header lives under src/ and is NEVER shipped or included
// by client code. It is the only place the EnTT registry is named, which keeps
// EnTT a private implementation detail of the engine.

#include "DingoEngine/Core/UUID.h"

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
		};

	}

}
