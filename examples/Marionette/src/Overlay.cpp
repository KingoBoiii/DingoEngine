#include "Overlay.h"

namespace Dingo::Overlay
{

	namespace
	{
		constexpr const char* k_FontPath = "fonts/arialbd.ttf";
	}

	Font* LoadFont(const char* who)
	{
		Font* font = Font::Create(k_FontPath);
		if (!font)
			DE_ERROR("Marionette: failed to load {} font '{}'", who, k_FontPath);
		return font;
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

	bool AnyInputPressed()
	{
		if (Input::IsAnyKeyPressed() || Input::IsAnyMouseButtonPressed())
			return true;

		for (uint32_t button = 0; button < GamepadButtonCount; ++button)
		{
			if (Input::IsGamepadButtonPressed(static_cast<GamepadButton>(button)))
				return true;
		}
		return false;
	}

}
