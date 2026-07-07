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

	// Hardware family, detected from the USB vendor id (name-based fallback).
	// Third-party pads that imitate neither report Unknown.
	enum class GamepadType : uint8_t
	{
		Unknown = 0,
		Xbox,
		PlayStation,
		Nintendo,
		Steam
	};

	inline const char* ToString(GamepadButton button)
	{
		switch (button)
		{
			case GamepadButton::A:           return "A";
			case GamepadButton::B:           return "B";
			case GamepadButton::X:           return "X";
			case GamepadButton::Y:           return "Y";
			case GamepadButton::LeftBumper:  return "LeftBumper";
			case GamepadButton::RightBumper: return "RightBumper";
			case GamepadButton::Back:        return "Back";
			case GamepadButton::Start:       return "Start";
			case GamepadButton::Guide:       return "Guide";
			case GamepadButton::LeftThumb:   return "LeftThumb";
			case GamepadButton::RightThumb:  return "RightThumb";
			case GamepadButton::DPadUp:      return "DPadUp";
			case GamepadButton::DPadRight:   return "DPadRight";
			case GamepadButton::DPadDown:    return "DPadDown";
			case GamepadButton::DPadLeft:    return "DPadLeft";
			default:                         return "Unknown";
		}
	}

	inline const char* ToString(GamepadType type)
	{
		switch (type)
		{
			case GamepadType::Xbox:        return "Xbox";
			case GamepadType::PlayStation: return "PlayStation";
			case GamepadType::Nintendo:    return "Nintendo";
			case GamepadType::Steam:       return "Steam";
			default:                       return "Unknown";
		}
	}

	inline std::ostream& operator<<(std::ostream& os, GamepadType type)
	{
		return os << ToString(type);
	}

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
