#pragma once

#include <cstdint>

// Engine-internal: the XInput calls gamepad rumble needs, which GLFW's input-only gamepad API
// doesn't cover. On a platform without XInput they report no controllers and do nothing.

namespace Dingo::Internal::XInput
{

	inline constexpr uint32_t k_MaxUsers = 4;

	// Bit i is set while XInput user index i has a controller.
	uint32_t GetConnectedMask();

	// Motor speeds from 0 to 1: lowFrequency drives the heavy left motor, highFrequency the light right one.
	bool SetVibration(uint32_t userIndex, float lowFrequency, float highFrequency);

}
