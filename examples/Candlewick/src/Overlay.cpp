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

	const char* PadLabel(GamepadButton button)
	{
		const bool playStation = Input::GetGamepadType() == GamepadType::PlayStation;
		switch (button)
		{
		case GamepadButton::A: return playStation ? "(X)" : "(A)";
		case GamepadButton::B: return playStation ? "(Circle)" : "(B)";
		case GamepadButton::X: return playStation ? "(Square)" : "(X)";
		case GamepadButton::Y: return playStation ? "(Triangle)" : "(Y)";
		case GamepadButton::Start: return playStation ? "(Options)" : "(Start)";
		default: return "";
		}
	}

	std::string ConfirmPrompt(const char* action)
	{
		if (!Input::IsGamepadConnected())
			return std::string("Press Space to ") + action;
		return std::string("Press Space or ") + PadLabel(GamepadButton::A) + " to " + action;
	}

	std::string ControlsSummary()
	{
		if (!Input::IsGamepadConnected())
			return "Move  WASD     Snuff  Q     Light a brazier  hold E     Pause  Esc";
		return std::string("Move  WASD / stick     Snuff  Q / ") + PadLabel(GamepadButton::X)
			+ "     Light a brazier  hold E / " + PadLabel(GamepadButton::A)
			+ "     Pause  Esc / " + PadLabel(GamepadButton::Start);
	}

	const std::string& PadText::Get()
	{
		const bool connected = Input::IsGamepadConnected();
		const GamepadType type = connected ? Input::GetGamepadType() : GamepadType::Unknown;
		if (!m_Built || connected != m_Connected || type != m_Type)
		{
			m_Text = m_Build();
			m_Connected = connected;
			m_Type = type;
			m_Built = true;
		}
		return m_Text;
	}

	bool ConfirmPressed()
	{
		return Input::IsKeyPressed(Key::Space) || Input::IsKeyPressed(Key::Enter)
			|| Input::IsGamepadButtonPressed(GamepadButton::A) || Input::IsGamepadButtonPressed(GamepadButton::Start);
	}

}
