#pragma once
#include <ostream>
#include <cstdint>

namespace Dingo
{

	/// A gamepad button; the face buttons have Xbox and PlayStation names for the same positions.
	/// Values match GLFW's gamepad mapping (glfw3.h).
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

	/// A gamepad axis; Input::GetGamepadAxis reads sticks in -1..1 and triggers in 0..1.
	enum class GamepadAxis : uint8_t
	{
		LeftX = 0,
		LeftY = 1,
		RightX = 2,
		RightY = 3,
		LeftTrigger = 4,
		RightTrigger = 5
	};

	/// Hardware family, detected from the USB vendor id (name-based fallback).
	/// Third-party pads that imitate neither report Unknown.
	enum class GamepadType : uint8_t
	{
		Unknown = 0,
		Xbox,
		PlayStation,
		Nintendo,
		Steam
	};

	/// The button's name ("A", "LeftBumper", ...); the PlayStation aliases print as A, B, X and Y.
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

	/// The family's name ("Xbox", "PlayStation", ...), or "Unknown".
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

	/// Prints the family's name, as ToString does.
	inline std::ostream& operator<<(std::ostream& os, GamepadType type)
	{
		return os << ToString(type);
	}

	/// Number of GamepadButton values (they run 0 to 14).
	inline constexpr uint32_t k_GamepadButtonCount = 15;
	/// Number of GamepadAxis values.
	inline constexpr uint32_t k_GamepadAxisCount = 6;
	/// Number of gamepad slots; Input's gamepad index is 0 to k_MaxGamepads - 1.
	inline constexpr uint32_t k_MaxGamepads = 16;

	[[deprecated("use k_GamepadButtonCount; removed in 2.0")]] inline constexpr uint32_t GamepadButtonCount = k_GamepadButtonCount;
	[[deprecated("use k_GamepadAxisCount; removed in 2.0")]] inline constexpr uint32_t GamepadAxisCount = k_GamepadAxisCount;
	[[deprecated("use k_MaxGamepads; removed in 2.0")]] inline constexpr uint32_t MaxGamepads = k_MaxGamepads;

	/// Prints the button's number, not its name.
	inline std::ostream& operator<<(std::ostream& os, GamepadButton button)
	{
		os << static_cast<int32_t>(button);
		return os;
	}

	/// Prints the axis's number, not its name.
	inline std::ostream& operator<<(std::ostream& os, GamepadAxis axis)
	{
		os << static_cast<int32_t>(axis);
		return os;
	}

}
