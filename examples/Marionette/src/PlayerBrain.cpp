#include "PlayerBrain.h"
#include "CameraRig.h"
#include "GameTuning.h"
#include "Overlay.h"

#include <DingoEngine.h>

#include <algorithm>

namespace
{
	using namespace Dingo;

	bool Down(KeyCode key, KeyCode alternative)
	{
		return Input::IsKeyDown(key) || Input::IsKeyDown(alternative);
	}
}

namespace Dingo
{

	FighterIntent PlayerBrain::Think(float, const Fighter&, const Fighter&)
	{
		// x to the camera's right, y away from the camera.
		glm::vec2 input(0.0f);
		if (Down(KEY_MOVE_FORWARD, KEY_MOVE_FORWARD_ALT) || Input::IsGamepadButtonDown(GamepadButton::DPadUp))
			input.y += 1.0f;
		if (Down(KEY_MOVE_BACK, KEY_MOVE_BACK_ALT) || Input::IsGamepadButtonDown(GamepadButton::DPadDown))
			input.y -= 1.0f;
		if (Down(KEY_MOVE_LEFT, KEY_MOVE_LEFT_ALT) || Input::IsGamepadButtonDown(GamepadButton::DPadLeft))
			input.x -= 1.0f;
		if (Down(KEY_MOVE_RIGHT, KEY_MOVE_RIGHT_ALT) || Input::IsGamepadButtonDown(GamepadButton::DPadRight))
			input.x += 1.0f;

		float magnitude = 0.0f;
		if (glm::length(input) > 0.0f)
		{
			input = glm::normalize(input);
			magnitude = Input::IsKeyDown(KEY_WALK) ? WALK_INTENT : 1.0f;
		}
		else
		{
			const glm::vec2 stick = Input::GetGamepadLeftStick();
			input = glm::vec2(stick.x, -stick.y);
			magnitude = std::min(glm::length(input), 1.0f);
			if (magnitude > 0.0f)
				input = glm::normalize(input);
		}

		const GroundAxes axes = GetArenaCameraAxes();
		const bool mouse = !Overlay::IsRefocusFrame();
		FighterIntent intent;
		intent.Move = (axes.Right * input.x + axes.Forward * input.y) * magnitude;
		intent.Light = Input::IsKeyPressed(KEY_LIGHT) || (mouse && Input::IsMouseButtonPressed(MouseButton::Left))
			|| Input::IsGamepadButtonPressed(GamepadButton::X);
		intent.Heavy = Input::IsKeyPressed(KEY_HEAVY) || (mouse && Input::IsMouseButtonPressed(MouseButton::Right))
			|| Input::IsGamepadButtonPressed(GamepadButton::Y);
		intent.Dodge = Input::IsKeyPressed(KEY_DODGE) || Input::IsGamepadButtonPressed(GamepadButton::A);
		intent.Block = Down(KEY_BLOCK, KEY_BLOCK_ALT) || Input::IsGamepadButtonDown(GamepadButton::RightBumper);
		return intent;
	}

}
