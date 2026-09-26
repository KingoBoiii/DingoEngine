#pragma once
#include <cstdint>

namespace Dingo
{

	// Normal  - visible, free to move anywhere (default).
	// Hidden  - invisible while over the window, still free to move off it.
	// Locked  - invisible, confined to the window, unbounded relative motion
	//           (mouse-look). See Input::SetCursorMode for focus-loss behavior.
	enum class CursorMode : uint8_t
	{
		Normal,
		Hidden,
		Locked
	};

}
