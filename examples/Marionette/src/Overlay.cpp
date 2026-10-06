#include "Overlay.h"

#include <cstdint>

namespace Dingo::Overlay
{

	namespace
	{
		constexpr uint16_t k_FirstKey = static_cast<uint16_t>(KeyCode::Space);
		constexpr uint16_t k_LastKey = static_cast<uint16_t>(KeyCode::Menu);
		constexpr uint16_t k_FirstFunctionKey = static_cast<uint16_t>(KeyCode::F1);
		constexpr uint16_t k_LastFunctionKey = static_cast<uint16_t>(KeyCode::F25);
		constexpr uint16_t k_FirstModifier = static_cast<uint16_t>(KeyCode::LeftShift);

		bool s_RefocusPending = false;
		bool s_RefocusFrame = false;

		bool IsMenuKey(uint16_t code)
		{
			const bool function = code >= k_FirstFunctionKey && code <= k_LastFunctionKey;
			const bool modifier = code >= k_FirstModifier;
			return !function && !modifier;
		}
	}

	Entity MakeCamera(Scene& scene, const char* name, float orthoSize)
	{
		Entity entity = scene.CreateEntity(name);
		auto& camera = entity.AddComponent<CameraComponent>();
		camera.Type = CameraComponent::ProjectionType::Orthographic;
		camera.OrthographicSize = orthoSize;
		camera.Primary = true;
		return entity;
	}

	Entity MakeText(Scene& scene, Font* font, const char* name, float size, const glm::vec4& color, const glm::vec3& position, const char* text)
	{
		Entity entity = scene.CreateEntity(name);
		auto& component = entity.AddComponent<TextComponent>();
		component.Font = font;
		component.Size = size;
		component.Color = color;
		component.Centered = true;
		component.Text = text;
		entity.GetComponent<TransformComponent>().Position = position;
		return entity;
	}

	void NoteFocus(bool focused)
	{
		if (focused)
			s_RefocusPending = true;
	}

	void BeginFrame()
	{
		s_RefocusFrame = s_RefocusPending;
		s_RefocusPending = false;
	}

	bool IsRefocusFrame()
	{
		return s_RefocusFrame;
	}

	bool AnyInputPressed()
	{
		for (uint16_t code = k_FirstKey; code <= k_LastKey; ++code)
		{
			if (IsMenuKey(code) && Input::IsKeyPressed(static_cast<KeyCode>(code)))
				return true;
		}

		if (!s_RefocusFrame && Input::IsAnyMouseButtonPressed())
			return true;

		for (uint32_t button = 0; button < GamepadButtonCount; ++button)
		{
			if (Input::IsGamepadButtonPressed(static_cast<GamepadButton>(button)))
				return true;
		}
		return false;
	}

}
