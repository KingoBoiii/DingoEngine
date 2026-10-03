#pragma once

// Engine-internal: a plain-data window onto every live scene's animators and their latest events,
// for the F7 debug tab. Keeps EnTT and SceneData out of the UI code.

#include "DingoEngine/Graphics/AnimationClip.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Dingo
{

	class Animator;
	class Scene;

	namespace Internal
	{

		struct SceneData;

		namespace AnimationDebug
		{

			// Each scene keeps this many of its newest events, across all its animators.
			inline constexpr size_t k_RecentEvents = 20;

			struct AnimatorRow
			{
				std::string Scene;
				std::string Entity;
				// Null when the animator isn't bound to the entity's current model. Valid until the scene
				// next updates or changes.
				const Animator* Instance = nullptr;
				std::string Model;
				bool Enabled = true;
				float Speed = 1.0f;
			};

			struct EventRow
			{
				std::string Entity; // resolved when collected; "(destroyed)" if the entity is gone
				std::string Clip;
				std::string_view Name;
				AnimationEventType Type = AnimationEventType::Instant;
				float Time = 0.0f;
				uint32_t Layer = 0;
				uint64_t Sequence = 0;
			};

			void RegisterScene(Scene* scene, SceneData* data);
			void UnregisterScene(Scene* scene);

			// Every live scene's animators that have a runtime animator, scenes in creation order.
			void CollectAnimators(std::vector<AnimatorRow>& out);

			// The newest `max` events across every live scene, oldest first.
			void CollectRecentEvents(std::vector<EventRow>& out, size_t max);

		}

	}

}
