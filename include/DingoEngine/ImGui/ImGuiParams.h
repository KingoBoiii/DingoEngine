#pragma once

namespace Dingo
{

	struct ImGuiParams
	{
		bool EnableDocking = true; // Enable docking by default
		bool EnableViewports = false; // Enable viewports by default
		bool EnableKeyboardNavigation = true; // Enable keyboard navigation by default
		bool EnableGamepadNavigation = false; // Disable gamepad navigation by default
		// bool UseDarkTheme = true; // Use dark theme by default
		// bool UseHighDPI = true; // Use high DPI scaling by default
	};

}
