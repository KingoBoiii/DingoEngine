#include "Overlay.h"

namespace Dingo::Overlay
{

	namespace
	{
		constexpr const char* k_FontPath = "assets/fonts/arialbd.ttf";
	}

	Font* LoadFont(const char* who)
	{
		Font* font = Font::Create(k_FontPath);
		if (!font)
			DE_ERROR("Candlewick: failed to load {} font '{}'", who, k_FontPath);
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

	Entity MakeText(Scene& scene, Font* font, const char* name, float size, const glm::vec4& color, bool centered)
	{
		Entity entity = scene.CreateEntity(name);
		auto& text = entity.AddComponent<TextComponent>();
		text.Font = font;
		text.Size = size;
		text.Color = color;
		text.Centered = centered;
		return entity;
	}

	std::string ConfirmPrompt(const char* action)
	{
		if (!Input::IsGamepadConnected())
			return std::string("Press Space to ") + action;
		const char* glyph = Input::GetGamepadType() == GamepadType::PlayStation ? "(X)" : "(A)";
		return std::string("Press Space or ") + glyph + " to " + action;
	}

	bool ConfirmPressed()
	{
		return Input::IsKeyPressed(Key::Space) || Input::IsKeyPressed(Key::Enter)
			|| Input::IsGamepadButtonPressed(GamepadButton::A) || Input::IsGamepadButtonPressed(GamepadButton::Start);
	}

}
