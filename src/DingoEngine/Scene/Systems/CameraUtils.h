#pragma once

// Engine-internal: camera/light selection and view-projection math over a scene
// registry. Lives under src/ so EnTT stays a private implementation detail.

#include "DingoEngine/Core/Ray.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace Dingo
{

	namespace Internal
	{

		namespace CameraUtils
		{

			// First CameraComponent marked Primary, else the first one found. Returns
			// false (out unchanged) when the registry has no CameraComponent.
			bool FindPrimaryCamera(const entt::registry& registry, entt::entity& out);

			// Render cameras per projection type in one pass (Primary preferred, else
			// first of that type). Each out handle is set only when its flag comes back true.
			void FindRenderCameras(const entt::registry& registry, entt::entity& outPerspective, bool& outHasPerspective, entt::entity& outOrthographic, bool& outHasOrthographic);

			bool FindFirstDirectionalLight(const entt::registry& registry, entt::entity& out);

			// Projection from the entity's CameraComponent, view from its transform.
			// Identity when the entity carries no CameraComponent.
			glm::mat4 ViewProjection(const entt::registry& registry, entt::entity camera, float aspect);

			// Unprojects a screen pixel through the primary perspective camera. Returns a
			// default Ray when there is no perspective camera.
			Ray ScreenPointToRay(const entt::registry& registry, const glm::vec2& screenPos, const glm::vec2& viewportSize);

		}

	}

}
