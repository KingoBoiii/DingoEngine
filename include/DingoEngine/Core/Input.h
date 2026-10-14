#pragma once
#include "KeyCodes.h"
#include "MouseButtons.h"
#include "GamepadCodes.h"
#include "CursorMode.h"

#include <glm/glm.hpp>

#include <string>

struct GLFWwindow;

namespace Dingo
{

	/// Frame-coherent input snapshot, polled with static functions from the main thread (usually in
	/// Layer::OnUpdate). The snapshot is taken once per frame, so every key and button edge reaches
	/// exactly one OnUpdate.
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

		/// True only on the frame the key went down; false for a key code out of range.
		static bool IsKeyPressed(KeyCode keycode);
		/// True every frame the key is held; false for a key code out of range.
		static bool IsKeyDown(KeyCode keycode);
		/// True only on the frame the key was let go; false for a key code out of range.
		static bool IsKeyReleased(KeyCode keycode);
		/// True while the key is not held.
		static bool IsKeyUp(KeyCode keycode);

		/**************************************************
		***		MOUSE									***
		**************************************************/

		/// True only on the frame the button went down; false for a button out of range.
		static bool IsMouseButtonPressed(MouseButton button);
		/// True every frame the button is held; false for a button out of range.
		static bool IsMouseButtonDown(MouseButton button);
		/// True only on the frame the button was let go; false for a button out of range.
		static bool IsMouseButtonReleased(MouseButton button);
		/// True while the button is not held.
		static bool IsMouseButtonUp(MouseButton button);

		/// Cursor position in window (screen) pixels, origin at the top-left, +Y down.
		/// While Locked it is an unbounded virtual position; unlocking restores the
		/// cursor to where it was locked.
		static glm::vec2 GetMousePosition();
		/// Cursor movement since last frame, in window pixels. Zero for 2 frames after a cursor
		/// mode change or a refocus while Locked (see SetCursorMode).
		static glm::vec2 GetMouseDelta();
		/// Scroll wheel movement this frame; +Y = scroll up, X = horizontal scroll.
		static glm::vec2 GetMouseScrollDelta();

		/**************************************************
		***		CURSOR									***
		**************************************************/

		/// Sets how the cursor behaves over the window; it works before the window exists too.
		/// GetMouseDelta() reads zero for 2 frames after a mode change or a refocus while
		/// Locked, so capturing the cursor never kicks the camera. While Locked, the OS
		/// frees the cursor on focus loss and re-locks it on refocus; the mode itself never
		/// changes on its own, so a game that pauses on focus loss should set Normal itself.
		/// A graphics-API restart resets the mode to Normal.
		static void SetCursorMode(CursorMode mode);
		/// The mode last set with SetCursorMode; Normal by default.
		static CursorMode GetCursorMode();

		/// Raw motion skips OS pointer acceleration. Applies only while Locked, where
		/// supported. Enabled by default.
		static void SetRawMouseMotion(bool enabled);
		/// The requested setting (true by default), whether or not it is supported or in effect.
		static bool IsRawMouseMotionEnabled();
		/// True when the platform supports raw motion; false before the window exists.
		static bool IsRawMouseMotionSupported();

		/**************************************************
		***		GAMEPAD									***
		**************************************************/

		// Gamepads are polled every frame; only devices with a gamepad mapping
		// known to GLFW are reported. Slot indices are stable while connected.

		/// True when slot `gamepad` (0 to k_MaxGamepads - 1) holds a connected pad that GLFW maps
		/// as a gamepad; false for an out-of-range slot.
		static bool IsGamepadConnected(uint32_t gamepad = 0);
		/// The pad's name, or an empty string when none is connected. Copy it to keep it.
		static const std::string& GetGamepadName(uint32_t gamepad = 0);
		/// Xbox / PlayStation / Nintendo / Steam, for e.g. button-prompt glyphs. Unknown when the
		/// slot is empty.
		static GamepadType GetGamepadType(uint32_t gamepad = 0);

		/// True only on the frame the button went down; false when the pad is not connected.
		static bool IsGamepadButtonPressed(GamepadButton button, uint32_t gamepad = 0);
		/// True every frame the button is held; false when the pad is not connected.
		static bool IsGamepadButtonDown(GamepadButton button, uint32_t gamepad = 0);
		/// True only on the frame the button was let go; false when the pad is not connected.
		static bool IsGamepadButtonReleased(GamepadButton button, uint32_t gamepad = 0);

		/// Deadzone-filtered and rescaled. Sticks in [-1, 1] (+Y = down, GLFW
		/// convention), triggers remapped to [0, 1]. 0 when the pad is not connected.
		static float GetGamepadAxis(GamepadAxis axis, uint32_t gamepad = 0);
		/// Raw GLFW axis value in [-1, 1], no deadzone.
		static float GetGamepadAxisRaw(GamepadAxis axis, uint32_t gamepad = 0);

		/// Stick as a vector with a radial deadzone; length <= 1, +Y = down.
		///@{
		static glm::vec2 GetGamepadLeftStick(uint32_t gamepad = 0);
		static glm::vec2 GetGamepadRightStick(uint32_t gamepad = 0);
		///@}

		/// Sets the deadzone used by GetGamepadAxis and the stick getters, clamped to 0..0.95;
		/// the default is 0.15. Settings saves and restores it.
		static void SetGamepadDeadzone(float deadzone);
		/// The current deadzone, 0 to 0.95.
		static float GetGamepadDeadzone();

		/// Rumble, on XInput (Xbox-compatible) pads under Windows only: on any other pad it does
		/// nothing and returns false. lowFrequency drives the heavy left motor, highFrequency the
		/// light right one, each from 0 to 1. It stops after `seconds`, even while the app is
		/// paused in the background, or when it is set again; 0 seconds stops it.
		static bool SetGamepadRumble(float lowFrequency, float highFrequency, float seconds, uint32_t gamepad = 0);
		/// Stops the pad's rumble at once; does nothing if it is not rumbling.
		static void StopGamepadRumble(uint32_t gamepad = 0);
		/// True for a connected XInput pad on Windows, the only kind SetGamepadRumble can drive.
		static bool IsGamepadRumbleSupported(uint32_t gamepad = 0);

		/**************************************************
		***		ANY										***
		**************************************************/

		/// True if any key went down this frame, e.g. for a "press any key" screen.
		static bool IsAnyKeyPressed();
		/// True while any key is held.
		static bool IsAnyKeyDown();
		/// True if any mouse button went down this frame.
		static bool IsAnyMouseButtonPressed();
		/// True while any mouse button is held.
		static bool IsAnyMouseButtonDown();

	private:
		static void Update();
		// Before the first update after a pause, in place of Update.
		static void Resume();
		// Every pass of the main loop, paused or not: ends rumble that has run its time.
		static void UpdateRumble();

		static void UpdateKeyState(KeyCode key, bool pressed);
		static void UpdateMouseButtonState(MouseButton button, bool pressed);
		static void UpdateMousePosition(float x, float y);
		static void AccumulateMouseScroll(float xOffset, float yOffset);
		static void SeedMousePosition(float x, float y);

		// Applies the stored cursor mode, so SetCursorMode works before the window exists.
		static void AttachWindow(GLFWwindow* window);
		static void OnWindowFocusChanged(bool focused);

		// Called from the GLFW joystick callback so type/name are already
		// classified when the connect/disconnect event reaches client code.
		static GamepadType RegisterGamepadConnection(uint32_t gamepad, std::string& outName);
		static GamepadType UnregisterGamepadConnection(uint32_t gamepad, std::string& outName);
		// Every joystick, gamepad mapping or not: keeps the XInput pairing in step with GLFW's.
		static void OnJoystickConnected();
		static void OnJoystickDisconnected(uint32_t joystick);

	private:
		friend class Application; // drives Update() once per frame
		friend class Window;      // feeds callback state
	};

}
