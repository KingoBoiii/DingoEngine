#pragma once

namespace Dingo
{

	/// Configuration for the engine's Dear ImGui layer: the debug window and the immediate-mode
	/// tools a game draws from Layer::OnImGuiRender (DingoEngine/UI/UI.h).
	struct ImGuiParams
	{
		bool EnableDocking = true; ///< Enable docking by default
		/// Let ImGui windows leave the main window as their own OS windows; off by default.
		bool EnableViewports = false;
		bool EnableKeyboardNavigation = true; ///< Enable keyboard navigation by default
		bool EnableGamepadNavigation = false; ///< Disable gamepad navigation by default
		/// While the pointer is over an ImGui window (or ImGui holds a press), mouse presses and
		/// the scroll are hidden from Input and the layers' mouse events; while ImGui edits a
		/// widget (a text field, a dragged slider) or shows a modal, so are key presses. A press
		/// that started in the game stays the game's until it is released. Set false when the
		/// game itself is drawn inside an ImGui window (an editor-style viewport).
		bool BlockGameInput = true;
	};

}
