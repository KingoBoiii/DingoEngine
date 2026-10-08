#pragma once

// Engine-internal: this header lives under src/ and is NEVER shipped or included
// by client code. It is the only place the EnTT registry is named, which keeps
// EnTT a private implementation detail of the engine.

#include "DingoEngine/Core/UUID.h"
#include "DingoEngine/Graphics/Animator.h"
#include "DingoEngine/Scene/Systems/AnimationSystem.h"
#include "DingoEngine/Scene/Systems/HierarchySystem.h"
#include "DingoEngine/Scene/Systems/LightSystem.h"
#include "DingoEngine/Scene/Systems/PhysicsSync.h"
#include "DingoEngine/Scene/Systems/ScriptSystem.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <cstdint>
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

			// Reused every frame by the 2D z-sort in RenderEntities: clear() keeps the
			// capacity, so a steady-state frame allocates nothing to sort.
			enum class Draw2DKind : std::uint8_t { Sprite, Circle, Text };
			struct Draw2D
			{
				glm::vec3 Position; // world; z is the sort key, then Kind, then Depth
				float Rotation;
				std::uint32_t Depth;
				entt::entity Entity;
				Draw2DKind Kind;
			};
			std::vector<Draw2D> Draw2DSortBuffer;

			// Scratch for the per-entity readers (rendering, lights, audio); reset by each pass.
			HierarchySystem::WorldMemo Memo;

			AnimationSystem::EventScratch AnimationEvents;

			LightSystem::ShadowProbeState ShadowProbes;
		};

	}

}
