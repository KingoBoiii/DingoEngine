#pragma once

namespace Dingo
{

	// Configuration for the engine's Dear ImGui layer: the debug window and the immediate-mode
	// tools a game draws from Layer::OnImGuiRender (DingoEngine/UI/UI.h).
	struct ImGuiParams
	{
		bool EnableDocking = true; // Enable docking by default
		bool EnableViewports = false; // Enable viewports by default
		bool EnableKeyboardNavigation = true; // Enable keyboard navigation by default
		bool EnableGamepadNavigation = false; // Disable gamepad navigation by default
	};

}
