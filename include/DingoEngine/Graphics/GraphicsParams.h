#pragma once
#include "DingoEngine/Common.h"

#include <cstdint>
#include <optional>

struct GLFWwindow;

namespace Dingo
{

	struct GraphicsParams
	{
		Dingo::GraphicsAPI GraphicsAPI = Dingo::GraphicsAPI::Vulkan;
		uint16_t FramesInFlight = 3;

		// Loads the Vulkan validation layer when it is installed. Empty = Debug builds only: it cost
		// Release about 0.6 ms of render CPU a frame. `--vulkan-validation[=off]` sets it too.
		std::optional<bool> VulkanValidation;

		// Set by the engine (from the application window) before the graphics context is created,
		// so device selection can verify which GPU/queue can present to the display the window
		// actually lives on. May be null (e.g. headless), in which case selection is surface-blind.
		GLFWwindow* NativeWindowHandle = nullptr;
	};

}
