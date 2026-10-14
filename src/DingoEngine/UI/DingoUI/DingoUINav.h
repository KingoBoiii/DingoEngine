#pragma once
#include "DingoEngine/UI/DingoUI.h"
#include "DingoUIDrawList.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace Dingo::Internal
{

	enum class UINavDirection : uint8_t
	{
		Left,
		Right,
		Up,
		Down
	};

	inline constexpr size_t k_UINavDirectionCount = 4;

	// One frame of navigation input as the player gave it: a direction is held, the rest went
	// down this frame. The context turns a held direction into moves with repeat.
	struct UINavInput
	{
		std::array<bool, k_UINavDirectionCount> Direction{};
		bool Accept = false;
		bool Back = false;
		bool Next = false;
		bool Previous = false;
		uint32_t Gamepad = 0; // the pad whose buttons were read, and whose buttons are taken from the game

		bool Any() const { return Direction[0] || Direction[1] || Direction[2] || Direction[3] || Accept || Back || Next || Previous; }
	};

	// A focusable widget of last frame, as the spatial search sees it.
	struct UINavCandidate
	{
		uint64_t Id = 0;
		UIRect Rect;
	};

	// The candidate to move to from `from` in `direction`, or -1: only those whose centre lies past
	// the origin's, scored by distance along the direction plus a penalty for the sideways gap and
	// offset. Ties go to the earlier declaration.
	int32_t FindNavTarget(const std::vector<UINavCandidate>& candidates, int32_t from, UINavDirection direction);
	// The candidate whose centre is nearest `point`, or -1.
	int32_t FindNearestNav(const std::vector<UINavCandidate>& candidates, const glm::vec2& point);
	// The next (or previous) index of `count` in declaration order, wrapping; from < 0 starts at an end.
	int32_t StepNavOrder(int32_t count, int32_t from, bool forward);

	// Turns a held direction into discrete moves: one at once, then repeats after a delay.
	class UINavRepeater
	{
	public:
		bool Step(bool held, float deltaTime, float delay, float interval);

	private:
		bool m_Held = false;
		float m_Timer = 0.0f;
	};

	// Reads the configured keys (when `keyboard`) and `gamepad`'s D-pad, left stick and buttons from Input.
	void ReadNavInput(UINavInput& out, bool keyboard, const UINavParams& params, uint32_t gamepad);
	// The first connected pad, or 0.
	uint32_t FindNavGamepad();
	// Hides from the game the keys and pad buttons bound to the actions in `fired`.
	void ConsumeNavInput(const UINavInput& fired, bool keyboard, const UINavParams& params);

}
