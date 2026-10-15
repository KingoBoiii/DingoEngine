#pragma once
#include "DingoEngine/UI/DingoUI.h"
#include "DingoUIDrawList.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Dingo::Internal
{

	// 0 at 0 and 1 at 1 for every curve; t outside 0..1 is clamped first.
	float EvaluateEase(UIEase ease, float t);

	// Moves `progress` (0..1) toward 1 (forward) or 0 by dt / duration; a duration of zero or less
	// jumps to the end.
	float StepProgress(float progress, float dt, float duration, bool forward);

	// Values eased toward a target, by id. An id undeclared for k_MaxAge frames is forgotten.
	class UITweenMap
	{
	public:
		static constexpr uint64_t k_MaxAge = 2;

		// The first call for an id returns `target`. A new target restarts from the current value
		// (returned unchanged this frame); otherwise the value advances by `dt` once per frame.
		glm::vec4 Update(uint64_t id, const glm::vec4& target, float dt, float duration, UIEase ease, uint64_t frame);
		size_t Evict(uint64_t frame);
		size_t Size() const { return m_Tweens.size(); }

	private:
		struct Tween
		{
			glm::vec4 From{ 0.0f };
			glm::vec4 To{ 0.0f };
			glm::vec4 Value{ 0.0f };
			float Elapsed = 0.0f;
			uint64_t LastSeenFrame = 0;
			uint64_t LastStepFrame = 0;
		};

		std::unordered_map<uint64_t, Tween> m_Tweens;
	};

	// How a transitioning root is drawn at one moment.
	struct UIRootVisual
	{
		float Alpha = 1.0f;
		glm::vec2 Offset{ 0.0f }; // design pixels, before the root's scale
		float Scale = 1.0f;

		bool IsIdentity() const { return Alpha == 1.0f && Offset == glm::vec2(0.0f) && Scale == 1.0f; }
	};

	// The roots that have a transition: how far each is in, and, once the game stops declaring one,
	// the draw commands of its last frame, which play out with no tree behind them.
	class UIRootTransitions
	{
	public:
		struct Entry
		{
			UITransition Transition;
			int Layer = 0;
			float Progress = 0.0f; // 0 hidden, 1 shown
			bool Exiting = false;
			uint64_t LastSeenFrame = 0;
			UIDrawList Kept;       // the last frame the root was drawn, untransformed
			glm::vec2 Pivot{ 0.0f }; // centre of the root's rect, for scaling
			float RootScale = 1.0f;
		};

		// A declared root. Advances it once per frame (a new root starts hidden and is not advanced
		// on its first frame); null, and the root forgotten, when its transition does nothing.
		Entry* Declare(uint64_t id, const UITransition& transition, int layer, uint64_t frame, float dt);
		// Plays out the roots not declared in `frame`, dropping the finished ones and those with
		// nothing kept, and lists the rest by layer.
		void AdvanceExiting(uint64_t frame, float dt, std::vector<Entry*>& exiting);
		// Drops the exiting roots whose kept commands draw `texture`, and empties a declared one's,
		// so it is dropped if it exits before it is drawn again. The other kept fonts must be alive.
		void ForgetTexture(const Texture* texture);
		size_t Size() const { return m_Entries.size(); }
		const Entry* Find(uint64_t id) const;

		static UIRootVisual VisualOf(const Entry& entry);

	private:
		std::unordered_map<uint64_t, Entry> m_Entries;
	};

}
