#include "depch.h"
#include "DingoEngine/Core/Input.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>

namespace Dingo
{

	static_assert(GamepadButtonCount == GLFW_GAMEPAD_BUTTON_LAST + 1);
	static_assert(GamepadAxisCount == GLFW_GAMEPAD_AXIS_LAST + 1);
	static_assert(MaxGamepads == GLFW_JOYSTICK_LAST + 1);

	namespace
	{
		constexpr size_t MaxKeys = GLFW_KEY_LAST + 1;
		constexpr size_t MaxMouseButtons = GLFW_MOUSE_BUTTON_LAST + 1;

		struct GamepadState
		{
			std::array<bool, GamepadButtonCount> Buttons{};
			std::array<float, GamepadAxisCount> Axes{};
			bool Connected = false;
		};

		std::array<bool, MaxKeys> s_CurrentKeys{};
		std::array<bool, MaxKeys> s_PreviousKeys{};

		std::array<bool, MaxMouseButtons> s_CurrentMouseButtons{};
		std::array<bool, MaxMouseButtons> s_PreviousMouseButtons{};

		glm::vec2 s_MousePosition{ 0.0f };
		glm::vec2 s_PreviousMousePosition{ 0.0f };
		glm::vec2 s_MouseScrollDelta{ 0.0f };

		std::array<GamepadState, MaxGamepads> s_CurrentGamepads{};
		std::array<GamepadState, MaxGamepads> s_PreviousGamepads{};
		std::array<std::string, MaxGamepads> s_GamepadNames{};
		std::array<GamepadType, MaxGamepads> s_GamepadTypes{};
		float s_GamepadDeadzone = 0.15f;

		bool ValidKey(KeyCode keycode) { return static_cast<size_t>(keycode) < MaxKeys; }
		bool ValidMouseButton(MouseButton button) { return static_cast<size_t>(button) < MaxMouseButtons; }
		bool ValidGamepad(uint32_t gamepad) { return gamepad < MaxGamepads; }

		float ApplyDeadzone(float value, float deadzone)
		{
			const float magnitude = std::abs(value);
			if (magnitude < deadzone)
				return 0.0f;
			const float rescaled = (magnitude - deadzone) / (1.0f - deadzone);
			return std::copysign(std::min(rescaled, 1.0f), value);
		}

		glm::vec2 ApplyRadialDeadzone(glm::vec2 stick, float deadzone)
		{
			const float length = glm::length(stick);
			if (length < deadzone)
				return glm::vec2(0.0f);
			const float rescaled = std::min((length - deadzone) / (1.0f - deadzone), 1.0f);
			return (stick / length) * rescaled;
		}

		GamepadType ClassifyGamepad(int jid, const std::string& name)
		{
			// SDL-style GUID: hex string, bytes 4-5 = USB vendor id, little-endian.
			const char* guid = glfwGetJoystickGUID(jid);
			if (guid && std::strlen(guid) >= 12)
			{
				const std::string vendor(guid + 8, guid + 12);
				if (vendor == "5e04") return GamepadType::Xbox;        // Microsoft 0x045e
				if (vendor == "4c05") return GamepadType::PlayStation; // Sony      0x054c
				if (vendor == "7e05") return GamepadType::Nintendo;    // Nintendo  0x057e
				if (vendor == "de28") return GamepadType::Steam;       // Valve     0x28de
			}

			std::string lower = name;
			std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (lower.find("xbox") != std::string::npos || lower.find("x-box") != std::string::npos || lower.find("360") != std::string::npos)
				return GamepadType::Xbox;
			if (lower.find("playstation") != std::string::npos || lower.find("dualshock") != std::string::npos
				|| lower.find("dualsense") != std::string::npos || lower.find("sony") != std::string::npos
				|| lower.find("ps3") != std::string::npos || lower.find("ps4") != std::string::npos || lower.find("ps5") != std::string::npos)
				return GamepadType::PlayStation;
			if (lower.find("nintendo") != std::string::npos || lower.find("switch") != std::string::npos || lower.find("joy-con") != std::string::npos)
				return GamepadType::Nintendo;
			if (lower.find("steam") != std::string::npos)
				return GamepadType::Steam;

			return GamepadType::Unknown;
		}
	}

	void Input::Update()
	{
		s_PreviousKeys = s_CurrentKeys;
		s_PreviousMouseButtons = s_CurrentMouseButtons;
		s_PreviousMousePosition = s_MousePosition;
		s_MouseScrollDelta = glm::vec2(0.0f); // refilled by scroll callbacks during event polling

		s_PreviousGamepads = s_CurrentGamepads;
		for (uint32_t jid = 0; jid < MaxGamepads; jid++)
		{
			GamepadState& state = s_CurrentGamepads[jid];

			GLFWgamepadstate glfwState;
			if (!glfwJoystickIsGamepad(jid) || !glfwGetGamepadState(jid, &glfwState))
			{
				state = GamepadState{};
				continue;
			}

			if (!state.Connected)
			{
				const char* name = glfwGetGamepadName(jid);
				s_GamepadNames[jid] = name ? name : "Unknown Gamepad";
				s_GamepadTypes[jid] = ClassifyGamepad(jid, s_GamepadNames[jid]);
			}

			state.Connected = true;
			for (size_t i = 0; i < GamepadButtonCount; i++)
				state.Buttons[i] = glfwState.buttons[i] == GLFW_PRESS;
			for (size_t i = 0; i < GamepadAxisCount; i++)
				state.Axes[i] = glfwState.axes[i];
		}
	}

	bool Input::IsKeyPressed(KeyCode keycode)
	{
		return ValidKey(keycode) && s_CurrentKeys[static_cast<size_t>(keycode)] && !s_PreviousKeys[static_cast<size_t>(keycode)];
	}

	bool Input::IsKeyDown(KeyCode keycode)
	{
		return ValidKey(keycode) && s_CurrentKeys[static_cast<size_t>(keycode)];
	}

	bool Input::IsKeyReleased(KeyCode keycode)
	{
		return ValidKey(keycode) && !s_CurrentKeys[static_cast<size_t>(keycode)] && s_PreviousKeys[static_cast<size_t>(keycode)];
	}

	bool Input::IsKeyUp(KeyCode keycode)
	{
		return !IsKeyDown(keycode);
	}

	bool Input::IsMouseButtonPressed(MouseButton button)
	{
		return ValidMouseButton(button) && s_CurrentMouseButtons[static_cast<size_t>(button)] && !s_PreviousMouseButtons[static_cast<size_t>(button)];
	}

	bool Input::IsMouseButtonDown(MouseButton button)
	{
		return ValidMouseButton(button) && s_CurrentMouseButtons[static_cast<size_t>(button)];
	}

	bool Input::IsMouseButtonReleased(MouseButton button)
	{
		return ValidMouseButton(button) && !s_CurrentMouseButtons[static_cast<size_t>(button)] && s_PreviousMouseButtons[static_cast<size_t>(button)];
	}

	bool Input::IsMouseButtonUp(MouseButton button)
	{
		return !IsMouseButtonDown(button);
	}

	glm::vec2 Input::GetMousePosition()
	{
		return s_MousePosition;
	}

	glm::vec2 Input::GetMouseDelta()
	{
		return s_MousePosition - s_PreviousMousePosition;
	}

	glm::vec2 Input::GetMouseScrollDelta()
	{
		return s_MouseScrollDelta;
	}

	bool Input::IsGamepadConnected(uint32_t gamepad)
	{
		return ValidGamepad(gamepad) && s_CurrentGamepads[gamepad].Connected;
	}

	const std::string& Input::GetGamepadName(uint32_t gamepad)
	{
		static const std::string s_Empty;
		if (!IsGamepadConnected(gamepad))
			return s_Empty;
		return s_GamepadNames[gamepad];
	}

	GamepadType Input::GetGamepadType(uint32_t gamepad)
	{
		if (!IsGamepadConnected(gamepad))
			return GamepadType::Unknown;
		return s_GamepadTypes[gamepad];
	}

	bool Input::IsGamepadButtonPressed(GamepadButton button, uint32_t gamepad)
	{
		if (!ValidGamepad(gamepad))
			return false;
		const size_t index = static_cast<size_t>(button);
		return s_CurrentGamepads[gamepad].Buttons[index] && !s_PreviousGamepads[gamepad].Buttons[index];
	}

	bool Input::IsGamepadButtonDown(GamepadButton button, uint32_t gamepad)
	{
		if (!ValidGamepad(gamepad))
			return false;
		return s_CurrentGamepads[gamepad].Buttons[static_cast<size_t>(button)];
	}

	bool Input::IsGamepadButtonReleased(GamepadButton button, uint32_t gamepad)
	{
		if (!ValidGamepad(gamepad))
			return false;
		const size_t index = static_cast<size_t>(button);
		return !s_CurrentGamepads[gamepad].Buttons[index] && s_PreviousGamepads[gamepad].Buttons[index];
	}

	float Input::GetGamepadAxis(GamepadAxis axis, uint32_t gamepad)
	{
		float value = GetGamepadAxisRaw(axis, gamepad);
		if (axis == GamepadAxis::LeftTrigger || axis == GamepadAxis::RightTrigger)
			value = (value + 1.0f) * 0.5f; // GLFW reports triggers in [-1, 1]
		return ApplyDeadzone(value, s_GamepadDeadzone);
	}

	float Input::GetGamepadAxisRaw(GamepadAxis axis, uint32_t gamepad)
	{
		if (!ValidGamepad(gamepad))
			return 0.0f;
		return s_CurrentGamepads[gamepad].Axes[static_cast<size_t>(axis)];
	}

	glm::vec2 Input::GetGamepadLeftStick(uint32_t gamepad)
	{
		const glm::vec2 stick(GetGamepadAxisRaw(GamepadAxis::LeftX, gamepad), GetGamepadAxisRaw(GamepadAxis::LeftY, gamepad));
		return ApplyRadialDeadzone(stick, s_GamepadDeadzone);
	}

	glm::vec2 Input::GetGamepadRightStick(uint32_t gamepad)
	{
		const glm::vec2 stick(GetGamepadAxisRaw(GamepadAxis::RightX, gamepad), GetGamepadAxisRaw(GamepadAxis::RightY, gamepad));
		return ApplyRadialDeadzone(stick, s_GamepadDeadzone);
	}

	void Input::SetGamepadDeadzone(float deadzone)
	{
		s_GamepadDeadzone = glm::clamp(deadzone, 0.0f, 0.95f);
	}

	float Input::GetGamepadDeadzone()
	{
		return s_GamepadDeadzone;
	}

	GamepadType Input::RegisterGamepadConnection(uint32_t gamepad, std::string& outName)
	{
		if (!ValidGamepad(gamepad))
			return GamepadType::Unknown;

		const char* name = glfwGetGamepadName(gamepad);
		s_GamepadNames[gamepad] = name ? name : "Unknown Gamepad";
		s_GamepadTypes[gamepad] = ClassifyGamepad(gamepad, s_GamepadNames[gamepad]);
		s_CurrentGamepads[gamepad].Connected = true; // buttons/axes follow on the next Update()

		outName = s_GamepadNames[gamepad];
		return s_GamepadTypes[gamepad];
	}

	GamepadType Input::UnregisterGamepadConnection(uint32_t gamepad, std::string& outName)
	{
		if (!ValidGamepad(gamepad))
			return GamepadType::Unknown;

		outName = s_GamepadNames[gamepad];
		const GamepadType type = s_GamepadTypes[gamepad];
		s_CurrentGamepads[gamepad] = GamepadState{};
		return type;
	}

	void Input::UpdateKeyState(KeyCode key, bool pressed)
	{
		if (ValidKey(key))
			s_CurrentKeys[static_cast<size_t>(key)] = pressed;
	}

	void Input::UpdateMouseButtonState(MouseButton button, bool pressed)
	{
		if (ValidMouseButton(button))
			s_CurrentMouseButtons[static_cast<size_t>(button)] = pressed;
	}

	void Input::UpdateMousePosition(float x, float y)
	{
		s_MousePosition = { x, y };
	}

	void Input::AccumulateMouseScroll(float xOffset, float yOffset)
	{
		s_MouseScrollDelta += glm::vec2(xOffset, yOffset);
	}

	void Input::SeedMousePosition(float x, float y)
	{
		s_MousePosition = { x, y };
		s_PreviousMousePosition = { x, y };
	}

}
