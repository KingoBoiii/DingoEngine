#include "FlyCamera.h"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>

namespace
{
	struct Preset
	{
		glm::vec3 Position;
		float Yaw;
		float Pitch;
	};

	constexpr Preset k_Presets[] = {
		{ { 0.0f, 1.7f, 6.5f }, 0.0f, 3.0f },
		{ { -0.65f, 1.4f, 4.2f }, 180.0f, 0.0f },
		{ { -3.0f, 1.6f, 0.0f }, 90.0f, 2.0f },
	};

	constexpr float k_Speed = 3.0f;
	constexpr float k_FastFactor = 3.0f;
	constexpr float k_LookDegreesPerPixel = 0.12f;
}

namespace Dingo
{

	void FlyCamera::Attach(Entity camera)
	{
		m_Camera = camera;
		JumpToPreset(1);
	}

	void FlyCamera::JumpToPreset(int preset)
	{
		m_Preset = std::clamp(preset, 1, 3);
		const Preset& target = k_Presets[m_Preset - 1];
		m_Position = target.Position;
		m_Yaw = target.Yaw;
		m_Pitch = target.Pitch;
		Apply();
	}

	glm::vec3 FlyCamera::GetPresetPosition(int preset)
	{
		return k_Presets[std::clamp(preset, 1, 3) - 1].Position;
	}

	void FlyCamera::Update(float deltaTime)
	{
		const bool rightDown = Input::IsMouseButtonDown(MouseButton::Button1);
		if (rightDown != m_Looking)
		{
			m_Looking = rightDown;
			Input::SetCursorMode(rightDown ? CursorMode::Locked : CursorMode::Normal);
		}

		if (Input::IsKeyPressed(Key::D1)) JumpToPreset(1);
		if (Input::IsKeyPressed(Key::D2)) JumpToPreset(2);
		if (Input::IsKeyPressed(Key::D3)) JumpToPreset(3);

		if (m_Looking)
		{
			const glm::vec2 delta = Input::GetMouseDelta();
			m_Yaw -= delta.x * k_LookDegreesPerPixel;
			m_Pitch = std::clamp(m_Pitch - delta.y * k_LookDegreesPerPixel, -89.0f, 89.0f);

			const glm::quat orientation = glm::angleAxis(glm::radians(m_Yaw), glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(glm::radians(m_Pitch), glm::vec3(1.0f, 0.0f, 0.0f));
			const glm::vec3 forward = orientation * glm::vec3(0.0f, 0.0f, -1.0f);
			const glm::vec3 right = orientation * glm::vec3(1.0f, 0.0f, 0.0f);

			glm::vec3 move(0.0f);
			if (Input::IsKeyDown(Key::W)) move += forward;
			if (Input::IsKeyDown(Key::S)) move -= forward;
			if (Input::IsKeyDown(Key::D)) move += right;
			if (Input::IsKeyDown(Key::A)) move -= right;
			if (Input::IsKeyDown(Key::E)) move.y += 1.0f;
			if (Input::IsKeyDown(Key::Q)) move.y -= 1.0f;
			if (glm::dot(move, move) > 0.0f)
			{
				const float speed = k_Speed * (Input::IsKeyDown(Key::LeftShift) ? k_FastFactor : 1.0f);
				m_Position += glm::normalize(move) * speed * deltaTime;
			}
		}

		Apply();
	}

	void FlyCamera::Apply()
	{
		if (!m_Camera.IsValid())
			return;
		Transform3DComponent& transform = m_Camera.GetComponent<Transform3DComponent>();
		transform.Position = m_Position;
		transform.Rotation = glm::angleAxis(glm::radians(m_Yaw), glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(glm::radians(m_Pitch), glm::vec3(1.0f, 0.0f, 0.0f));
	}

}
