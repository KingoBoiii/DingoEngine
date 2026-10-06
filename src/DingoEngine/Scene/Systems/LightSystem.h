#pragma once

// Engine-internal: feeds a scene's light components to Renderer3D.
// Lives under src/ so EnTT stays a private implementation detail.

#include "DingoEngine/Scene/Systems/HierarchySystem.h"

#include <entt/entt.hpp>

namespace Dingo
{

	class Renderer3D;

	namespace Internal
	{

		namespace LightSystem
		{

			// Submits every light component and the scene's summed ambient. A registry without
			// a single light component gets a default DirectionalLightComponent instead.
			void SubmitLights(const entt::registry& registry, Renderer3D& renderer, HierarchySystem::WorldMemo& memo);

		}

	}

}
