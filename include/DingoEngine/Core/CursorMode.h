#pragma once
#include <cstdint>

namespace Dingo
{

	/// How the mouse cursor behaves over the game window; set it with Input::SetCursorMode.
	enum class CursorMode : uint8_t
	{
		/// Visible, free to move anywhere (default).
		Normal,
		/// Invisible while over the window, still free to move off it.
		Hidden,
		/// Invisible, confined to the window, unbounded relative motion (mouse-look).
		/// See Input::SetCursorMode for focus-loss behavior.
		Locked
	};

}
