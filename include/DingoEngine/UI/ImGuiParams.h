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
	};

}
