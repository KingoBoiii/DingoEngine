#pragma once

namespace Dingo
{

	// Configuration for the engine's immediate-mode UI layer (see DingoEngine/UI/UI.h).
	// Backend-agnostic on the public surface; the active backend (Dear ImGui) maps these
	// onto its own flags internally.
	struct UIParams
	{
		bool EnableDocking = true; // Enable docking by default
		bool EnableViewports = false; // Enable viewports by default
		bool EnableKeyboardNavigation = true; // Enable keyboard navigation by default
		bool EnableGamepadNavigation = false; // Disable gamepad navigation by default
		// bool UseDarkTheme = true; // Use dark theme by default
		// bool UseHighDPI = true; // Use high DPI scaling by default
	};

}
