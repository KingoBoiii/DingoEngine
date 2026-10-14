#pragma once
#include "DingoEngine/Core/Input.h"

namespace Dingo::Internal
{

	// The engine's side of Input: the consume mask that DingoUI and a capturing ImGui use to hide
	// presses from the game. A consumed button or key reads as up to every Input query until it
	// is released, the release frame included; consumed scroll reads zero for the frame.
	struct InputBackend
	{
		static void ConsumeKey(KeyCode keycode) { Input::ConsumeKey(keycode); }
		static void ConsumeMouseButton(MouseButton button) { Input::ConsumeMouseButton(button); }
		static void ConsumeScroll() { Input::ConsumeScroll(); }
		static void ConsumeGamepadButton(GamepadButton button, uint32_t gamepad) { Input::ConsumeGamepadButton(button, gamepad); }
		static void ConsumeNewPresses(bool mouse, bool keyboard) { Input::ConsumeNewPresses(mouse, keyboard); }
		// Input statics outlive an Application across RequestRestart, so the mask is cleared with
		// the DingoUI context.
		static void ClearConsumed() { Input::ClearConsumed(); }

		// Engine hotkeys (the debug window's F-keys) answer even while a capture hides the key.
		static bool IsKeyPressedUnconsumed(KeyCode keycode) { return Input::IsKeyPressedUnconsumed(keycode); }

		// The test app's simulated devices: sets the live state the window callbacks would.
		static void SimulateMouseButton(MouseButton button, bool pressed) { Input::UpdateMouseButtonState(button, pressed); }
		static void SimulateKey(KeyCode keycode, bool pressed) { Input::UpdateKeyState(keycode, pressed); }
	};

}
