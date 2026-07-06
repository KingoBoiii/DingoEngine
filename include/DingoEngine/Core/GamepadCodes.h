#pragma once
#include <ostream>
#include <cstdint>

namespace Dingo
{

	// Values match GLFW's gamepad mapping (glfw3.h).
	enum class GamepadButton : uint8_t
	{
		A = 0,
		B = 1,
		X = 2,
		Y = 3,
		Cross = A,
		Circle = B,
		Square = X,
		Triangle = Y,
		LeftBumper = 4,
		RightBumper = 5,
		Back = 6,
		Start = 7,
		Guide = 8,
		LeftThumb = 9,
		RightThumb = 10,
		DPadUp = 11,
		DPadRight = 12,
		DPadDown = 13,
		DPadLeft = 14
	};

	enum class GamepadAxis : uint8_t
	{
		LeftX = 0,
		LeftY = 1,
		RightX = 2,
		RightY = 3,
		LeftTrigger = 4,
		RightTrigger = 5
	};

	inline constexpr uint32_t GamepadButtonCount = 15;
	inline constexpr uint32_t GamepadAxisCount = 6;
	inline constexpr uint32_t MaxGamepads = 16;

	inline std::ostream& operator<<(std::ostream& os, GamepadButton button)
	{
		os << static_cast<int32_t>(button);
		return os;
	}

	inline std::ostream& operator<<(std::ostream& os, GamepadAxis axis)
	{
		os << static_cast<int32_t>(axis);
		return os;
	}

}
