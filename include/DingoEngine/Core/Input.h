#pragma once
#include "KeyCodes.h"
#include "MouseButtons.h"
#include "GamepadCodes.h"

#include <glm/glm.hpp>

#include <string>

namespace Dingo
{

	/// Frame-coherent input snapshot.
	/// Naming convention (applies to keys, mouse buttons and gamepad buttons):
	///   IsXPressed  - became pressed this frame (edge)
	///   IsXDown     - currently held
	///   IsXReleased - became released this frame (edge)
	///   IsXUp       - currently not held
	class Input
	{
	public:
		/**************************************************
		***		KEYBOARD								***
		**************************************************/

		static bool IsKeyPressed(KeyCode keycode);
		static bool IsKeyDown(KeyCode keycode);
		static bool IsKeyReleased(KeyCode keycode);
		static bool IsKeyUp(KeyCode keycode);

		/**************************************************
		***		MOUSE									***
		**************************************************/

		static bool IsMouseButtonPressed(MouseButton button);
		static bool IsMouseButtonDown(MouseButton button);
		static bool IsMouseButtonReleased(MouseButton button);
		static bool IsMouseButtonUp(MouseButton button);

		// Cursor position in window (screen) pixels, origin at the top-left, +Y down.
		static glm::vec2 GetMousePosition();
		// Cursor movement since last frame, in window pixels.
		static glm::vec2 GetMouseDelta();
		// Scroll wheel movement this frame; +Y = scroll up, X = horizontal scroll.
		static glm::vec2 GetMouseScrollDelta();

		/**************************************************
		***		GAMEPAD									***
		**************************************************/

		// Gamepads are polled every frame; only devices with a gamepad mapping
		// known to GLFW are reported. Slot indices are stable while connected.

		static bool IsGamepadConnected(uint32_t gamepad = 0);
		static const std::string& GetGamepadName(uint32_t gamepad = 0);
		// Xbox / PlayStation / Nintendo / Steam, for e.g. button-prompt glyphs.
		static GamepadType GetGamepadType(uint32_t gamepad = 0);

		static bool IsGamepadButtonPressed(GamepadButton button, uint32_t gamepad = 0);
		static bool IsGamepadButtonDown(GamepadButton button, uint32_t gamepad = 0);
		static bool IsGamepadButtonReleased(GamepadButton button, uint32_t gamepad = 0);

		// Deadzone-filtered and rescaled. Sticks in [-1, 1] (+Y = down, GLFW
		// convention), triggers remapped to [0, 1].
		static float GetGamepadAxis(GamepadAxis axis, uint32_t gamepad = 0);
		// Raw GLFW axis value in [-1, 1], no deadzone.
		static float GetGamepadAxisRaw(GamepadAxis axis, uint32_t gamepad = 0);

		// Stick as a vector with a radial deadzone; length <= 1, +Y = down.
		static glm::vec2 GetGamepadLeftStick(uint32_t gamepad = 0);
		static glm::vec2 GetGamepadRightStick(uint32_t gamepad = 0);

		static void SetGamepadDeadzone(float deadzone);
		static float GetGamepadDeadzone();

	private:
		static void Update();

		static void UpdateKeyState(KeyCode key, bool pressed);
		static void UpdateMouseButtonState(MouseButton button, bool pressed);
		static void UpdateMousePosition(float x, float y);
		static void AccumulateMouseScroll(float xOffset, float yOffset);
		static void SeedMousePosition(float x, float y);

		// Called from the GLFW joystick callback so type/name are already
		// classified when the connect/disconnect event reaches client code.
		static GamepadType RegisterGamepadConnection(uint32_t gamepad, std::string& outName);
		static GamepadType UnregisterGamepadConnection(uint32_t gamepad, std::string& outName);

	private:
		friend class Application; // drives Update() once per frame
		friend class Window;      // feeds callback state
	};

}
