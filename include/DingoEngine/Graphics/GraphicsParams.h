#pragma once
#include "DingoEngine/Common.h"

#include <cstdint>

struct GLFWwindow;

namespace Dingo
{

	struct GraphicsParams
	{
		GraphicsAPI GraphicsAPI;
		uint16_t FramesInFlight;

		// Set by the engine (from the application window) before the graphics context is created,
		// so device selection can verify which GPU/queue can present to the display the window
		// actually lives on. May be null (e.g. headless), in which case selection is surface-blind.
		GLFWwindow* NativeWindowHandle = nullptr;
	};

}
