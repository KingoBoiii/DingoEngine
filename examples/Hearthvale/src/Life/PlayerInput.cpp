#include "Life/PlayerInput.h"

#include <DingoEngine.h>

#include <algorithm>

namespace Dingo
{

	glm::vec2 CombineMove(const glm::vec2& keys, const glm::vec2& stick, float deadzone)
	{
		glm::vec2 move = keys + stick;
		const float length = glm::length(move);
		if (length < deadzone)
			return glm::vec2(0.0f);
		return length > 1.0f ? move / length : move;
	}

	void PollPlayerIntent(PlayerIntent& intent)
	{
		glm::vec2 keys(0.0f);
		if (Input::IsKeyDown(Key::W)) keys.y += 1.0f;
		if (Input::IsKeyDown(Key::S)) keys.y -= 1.0f;
		if (Input::IsKeyDown(Key::D)) keys.x += 1.0f;
		if (Input::IsKeyDown(Key::A)) keys.x -= 1.0f;

		// The stick's y is down-positive.
		const glm::vec2 leftStick = Input::GetGamepadLeftStick();
		intent.Move = CombineMove(keys, glm::vec2(leftStick.x, -leftStick.y), 0.0f);

		const bool control = Input::IsKeyDown(Key::LeftControl) || Input::IsKeyDown(Key::RightControl);
		const glm::vec2 scroll = Input::GetMouseScrollDelta();

		intent.Use |= Input::IsMouseButtonPressed(MouseButton::Left) || Input::IsKeyPressed(Key::Space) || Input::IsGamepadButtonPressed(GamepadButton::A);
		intent.Interact |= Input::IsMouseButtonPressed(MouseButton::Right) || Input::IsKeyPressed(Key::F) || Input::IsGamepadButtonPressed(GamepadButton::X);
		intent.RotatePlacement |= Input::IsKeyPressed(Key::R) || Input::IsGamepadButtonPressed(GamepadButton::Y);
		intent.ToggleInventory |= Input::IsKeyPressed(Key::Tab) || Input::IsGamepadButtonPressed(GamepadButton::Back);
		intent.ToggleBuild |= Input::IsKeyPressed(Key::B) || Input::IsGamepadButtonPressed(GamepadButton::B);
		intent.CycleSeed |= Input::IsKeyPressed(Key::T) || Input::IsGamepadButtonPressed(GamepadButton::DPadUp);
		intent.Pause |= Input::IsKeyPressed(Key::Escape) || Input::IsGamepadButtonPressed(GamepadButton::Start);

		intent.HotbarPrevious |= Input::IsGamepadButtonPressed(GamepadButton::LeftBumper) || (!control && scroll.y > 0.0f);
		intent.HotbarNext |= Input::IsGamepadButtonPressed(GamepadButton::RightBumper) || (!control && scroll.y < 0.0f);
		for (int slot = 0; slot < 9; ++slot)
		{
			if (Input::IsKeyPressed(static_cast<KeyCode>(static_cast<int>(Key::D1) + slot)))
				intent.HotbarSlot = slot;
		}

		// E is the camera here, so the keyboard's interact is F; the plan gave both to E.
		if (Input::IsKeyPressed(Key::Q) || Input::IsGamepadButtonPressed(GamepadButton::DPadLeft))
			intent.CameraSteps -= 1;
		if (Input::IsKeyPressed(Key::E) || Input::IsGamepadButtonPressed(GamepadButton::DPadRight))
			intent.CameraSteps += 1;

		intent.CameraZoomAxis = -Input::GetGamepadRightStick().y;
		if (control)
			intent.CameraZoomWheel += scroll.y;
	}

}
