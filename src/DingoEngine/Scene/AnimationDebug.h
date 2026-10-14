#pragma once

// Engine-internal: a plain-data window onto every live scene's animators and their latest events,
// for the F7 debug tab. Keeps EnTT and SceneData out of the UI code.

#include "DingoEngine/Graphics/AnimationClip.h"
#include "DingoEngine/Scene/Components.h"

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

		namespace AnimationDebug
		{

			// Each scene keeps this many of its newest events, across all its animators.
			inline constexpr size_t k_RecentEvents = 20;

			// Cheap to collect for a crowd: the names are read only for the rows on screen (DescribeAnimator).
			struct AnimatorRow
			{
				const Scene* Owner = nullptr;
				uint32_t Entity = 0;
				// Null when the animator isn't bound to the entity's current model. Valid while SceneDebug::LockScenes()
				// is held and the scene neither updates nor changes.
				const Animator* Instance = nullptr;
				bool Enabled = true;
				float Speed = 1.0f;
				// Not drawn last frame, and posed by its CullingMode (AnimatorComponent).
				bool Culled = false;
				AnimatorCullingMode CullingMode = AnimatorCullingMode::AlwaysAnimate;
			};

			struct AnimatorNames
			{
				std::string Scene;
				std::string Entity;
				std::string Model;
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

			// Every live scene's animators that have a runtime animator, scenes in the order they first updated.
			void CollectAnimators(std::vector<AnimatorRow>& out);
			// The row's scene, entity and model names; "(gone)" for a scene or entity gone since.
			AnimatorNames DescribeAnimator(const AnimatorRow& row);

			// The newest `max` events across every live scene, oldest first.
			void CollectRecentEvents(std::vector<EventRow>& out, size_t max);

		}

	}

}
