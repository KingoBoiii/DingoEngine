#pragma once

// Engine-internal: feeds a scene's light components to Renderer3D.
// Lives under src/ so EnTT stays a private implementation detail.

#include "DingoEngine/Scene/Systems/HierarchySystem.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_map>

namespace Dingo
{

	class Renderer3D;

	namespace Internal
	{

		namespace LightSystem
		{

			// Scene::GetLightVisibility's requests, keyed by (light entity << 32 | the caller's key),
			// issued to Renderer3D as shadow probes by the next SubmitLights, which also collects the
			// answers that have come back since.
			struct ShadowProbeState
			{
				struct Request
				{
					entt::entity Light = entt::null;
					glm::vec3 Point{ 0.0f };
				};
				struct Answer
				{
					float Value = 1.0f;
					uint64_t Frame = 0;
				};

				std::unordered_map<uint64_t, Request> Pending;
				std::unordered_map<uint64_t, Answer> Answers;
				uint64_t Salt = 0; // keeps two scenes' keys apart on a shared renderer
				uint64_t PruneFrame = 0;

				static uint64_t LocalKey(entt::entity light, uint32_t key)
				{
					return (static_cast<uint64_t>(entt::to_integral(light)) << 32) | key;
				}
			};

			// Submits every light component, the scene's summed ambient and its first enabled
			// FogComponent (clearColor for one with UseClearColor). A registry without a single light
			// component gets a default DirectionalLightComponent instead. With probes, also issues the
			// pending shadow probes against the lights just submitted.
			void SubmitLights(const entt::registry& registry, Renderer3D& renderer, HierarchySystem::WorldMemo& memo, const glm::vec3& clearColor, ShadowProbeState* probes = nullptr);

		}

	}

}
