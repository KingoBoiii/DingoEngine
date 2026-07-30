#pragma once

// Engine-internal: drives the AudioEngine from the scene's audio components.
// Lives under src/ so EnTT stays a private implementation detail.

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace Dingo
{

	namespace Internal
	{

		namespace AudioSync
		{

			// World-space position for a source: Transform3DComponent if present, else the
			// 2D TransformComponent's Position at z = 0. Every entity has a TransformComponent,
			// so this always returns something.
			glm::vec3 PositionOf(const entt::registry& registry, entt::entity handle);

			// Pushes every spatialized source's position and the primary listener's
			// position/orientation to the engine. Call once per frame, after transforms
			// are final.
			void SyncListenerAndSources(entt::registry& registry);

			// (Re)starts the entity's source from its component params. Stops whatever it
			// was already playing first. No-op without an AudioSourceComponent or Clip.
			void PlaySource(entt::registry& registry, entt::entity handle);

			// Stops the entity's live sound and clears its handle. No-op if nothing plays.
			void StopSource(entt::registry& registry, entt::entity handle);

			// Stops every live sound in the registry. Looping sounds are never
			// self-reaping, so dropping entities without this leaves them playing with no
			// handle left to stop.
			void StopAllSources(entt::registry& registry);

		}

	}

}
