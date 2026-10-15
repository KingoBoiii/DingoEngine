#include "Overlay.h"
#include "GameTuning.h"

#include <algorithm>

namespace Dingo::Overlay
{

	namespace
	{
		constexpr const char* k_FontPath = "fonts/Inter-Bold.ttf";
		constexpr float k_HeightMatch = 1.0f;

		float s_Ascent = 0.0f;
		float s_LineHeight = 0.0f;
		bool s_RefocusPending = false;
		bool s_RefocusFrame = false;
	}

	Font* LoadFont()
	{
		Font* font = Font::Create(k_FontPath);
		if (!font)
			DE_ERROR("Candlewick: failed to load the font '{}'", k_FontPath);
		return font;
	}

	void UseFont(Font* font)
	{
		s_Ascent = font ? font->GetAscender(1.0f) : 0.0f;
		s_LineHeight = font ? font->GetLineHeight(1.0f) : 0.0f;
		DingoUI::SetDefaultFont(font);
		DingoUI::SetTheme(MenuStyle());
	}

	void DropFont()
	{
		DingoUI::SetDefaultFont(nullptr);
	}

	UIScale Scale()
	{
		return UIScale::ScaleWithScreen(glm::vec2(1920.0f, 1080.0f), k_HeightMatch);
	}

	UIStyle MenuStyle()
	{
		return UIStyle::Default()
			.SetTextSize(UiPx(MENU_BUTTON_SIZE))
			.SetTextColor(COLOR_TEXT)
			.SetBackground(COLOR_METER_BG)
			.SetHoverColor({ 0.22f, 0.16f, 0.09f, 0.95f })
			.SetPressedColor({ 0.12f, 0.09f, 0.06f, 0.95f })
			.SetFocusedColor(COLOR_TITLE)
			.SetBorder(2.0f, { COLOR_TITLE.r, COLOR_TITLE.g, COLOR_TITLE.b, 0.45f })
			.SetCornerRadius(8.0f)
			.SetPadding(UiPx(MENU_BUTTON_PAD_X), UiPx(MENU_BUTTON_PAD_Y))
			.SetFocusWidth(3.0f)
			.SetTransition(0.12f);
	}

	UIRootParams HudRoot(UIAnchor anchor)
	{
		return UIRootParams().SetAnchor(anchor).SetScale(Scale()).SetBlocksInput(false);
	}

	UIRootParams MenuRoot(const glm::vec4& background)
	{
		return UIRootParams()
			.SetAnchor(UIAnchor::Stretch)
			.SetScale(Scale())
			.SetNavigable(true)
			.SetTransition(UITransition::Fade(UI_FADE_SECONDS))
			.SetLayout(UIBoxParams().SetAlign(UIAlign::Center).SetBackground(background));
	}

	float TextTop(float baseline, float size)
	{
		return UiPx(baseline - s_Ascent * size);
	}

	float ButtonTop(float baseline, float size)
	{
		return UiPx(baseline - MENU_BUTTON_PAD_Y - s_Ascent * size);
	}

	float ButtonHeight(float size)
	{
		return UiPx(s_LineHeight * size + 2.0f * MENU_BUTTON_PAD_Y);
	}

	bool Button(const std::string& label, float size, const UIWidgetParams& params, bool bare)
	{
		UIStyle style = UIStyle(DingoUI::GetStyle()).SetTextSize(UiPx(size));
		if (bare)
			style.SetBackground(glm::vec4(0.0f)).SetBorder(0.0f, glm::vec4(0.0f));
		DingoUI::PushStyle(style);
		const bool clicked = DingoUI::Button(label, params).Clicked;
		DingoUI::PopStyle();
		return clicked && !s_RefocusFrame;
	}

	Band::Band(float& bottom, float top, float height)
	{
		if (top > bottom)
		{
			DingoUI::BeginBox(UIBoxParams().SetHeight(UISize::Fixed(top - bottom)));
			DingoUI::EndBox();
		}
		bottom = (std::max)(top, bottom) + height;
		DingoUI::BeginBox(UIBoxParams().SetHeight(UISize::Fixed(height)).SetAlign(UIAlign::Center));
	}

	Band::~Band()
	{
		DingoUI::EndBox();
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

	bool BackPressed()
	{
		return Input::IsKeyPressed(Key::Escape) || (DingoUI::IsFocusVisible() && DingoUI::IsBackPressed());
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
