#include "Hud.h"
#include "Braziers.h"
#include "GameTuning.h"
#include "KeepMap.h"
#include "Lantern.h"
#include "Overlay.h"

#include <algorithm>
#include <string_view>

namespace
{
	using namespace Dingo;

	std::string SnuffHint()
	{
		if (!Input::IsGamepadConnected())
			return "Q  snuff / relight";
		return std::string("Q / ") + Overlay::PadLabel(GamepadButton::X) + "  snuff / relight";
	}

	std::string HoldPrompt(const char* action)
	{
		if (!Input::IsGamepadConnected())
			return std::string("Hold E to ") + action;
		return std::string("Hold E / ") + Overlay::PadLabel(GamepadButton::A) + " to " + action;
	}

	std::string PauseHint()
	{
		const bool pad = Input::IsGamepadConnected();
		std::string hint = pad ? std::string("Esc / ") + Overlay::PadLabel(GamepadButton::Start) : std::string("Esc");
		hint += "  resume   " "\xC2\xB7" "   Enter";
		if (pad)
			hint += std::string(" / ") + Overlay::PadLabel(GamepadButton::B);
		return hint + "  title";
	}

	void DeclareText(const char* id, UIAnchor anchor, float left, float baseline, float size, std::string_view text, const glm::vec4& color)
	{
		if (text.empty() || color.a <= 0.0f)
			return;
		if (!DingoUI::BeginRoot(id, Overlay::HudRoot(anchor).SetLayer(UI_LAYER_TEXT).SetOffset({ UiPx(left), Overlay::TextTop(baseline, size) })))
			return;
		DingoUI::Text(text, UITextParams().SetSize(UiPx(size)).SetColor(color).SetWrap(false));
		DingoUI::EndRoot();
	}

	void DeclareMeter(const char* id, UIAnchor anchor, float left, float top, float width, float height, float inset, float fraction, const glm::vec4& fill)
	{
		const UIBoxParams frame = UIBoxParams().SetWidth(UISize::Fixed(UiPx(width))).SetHeight(UISize::Fixed(UiPx(height)))
			.SetPadding(UiPx(inset)).SetDirection(UIDirection::Row).SetBackground(COLOR_METER_BG);
		if (!DingoUI::BeginRoot(id, Overlay::HudRoot(anchor).SetLayer(UI_LAYER_METER).SetOffset({ UiPx(left), UiPx(top) }).SetLayout(frame)))
			return;

		const float fillWidth = UiPx(width - 2.0f * inset) * fraction;
		if (fillWidth >= 0.5f)
		{
			DingoUI::BeginBox(UIBoxParams().SetWidth(UISize::Fixed(fillWidth)).SetHeight(UISize::Fixed(UiPx(height - 2.0f * inset))).SetBackground(fill));
			DingoUI::EndBox();
		}
		DingoUI::EndRoot();
	}
}

namespace Dingo
{

	Hud::Hud(const KeepMap& map, const Lantern&, int startRoom)
		: m_Map(map)
		, m_SnuffHint(&SnuffHint)
		, m_LightBrazierPrompt([] { return HoldPrompt("light the brazier"); })
		, m_LightAltarPrompt([] { return HoldPrompt("light the altar"); })
		, m_RefuelPrompt([] { return HoldPrompt("refill the lantern"); })
		, m_PauseHintText(&PauseHint)
	{
		SetRoom(startRoom);
	}

	void Hud::SetRoom(int room)
	{
		if (room < 0 || room == m_Room || room >= static_cast<int>(m_Map.GetRooms().size()))
			return;

		m_Room = room;
	}

	void Hud::SetFade(float amount)
	{
		m_Fade = std::clamp(amount, 0.0f, 1.0f);
	}

	void Hud::SetBrazierPrompt(BrazierPrompt prompt, float progress)
	{
		m_PromptKind = prompt;
		m_Progress = std::clamp(progress, 0.0f, 1.0f);
	}

	void Hud::ShowToast(const std::string& text)
	{
		m_ToastText = text;
		m_ToastTime = CHECKPOINT_TOAST_TIME;
	}

	void Hud::SetPaused(bool paused)
	{
		m_Paused = paused;
	}

	PauseChoice Hud::TakePauseChoice()
	{
		return std::exchange(m_PauseChoice, PauseChoice::None);
	}

	void Hud::DeclarePause()
	{
		if (!m_Paused)
			return;
		if (!DingoUI::BeginRoot("pause", Overlay::MenuRoot(COLOR_PAUSE_DIM).SetLayer(UI_LAYER_PAUSE)))
			return;

		const UITextParams centered = UITextParams().SetWrap(false).SetAlign(UITextAlign::Center);
		float bottom = 0.0f;
		{
			const Overlay::Band band(bottom, Overlay::TextTop(ScreenTop(HUD_PAUSE_TITLE_RISE), HUD_PAUSE_TITLE_SIZE), UiPx(HUD_PAUSE_TITLE_SIZE));
			DingoUI::Text("Paused", UITextParams(centered).SetSize(UiPx(HUD_PAUSE_TITLE_SIZE)).SetColor(COLOR_TITLE));
		}
		{
			const Overlay::Band band(bottom, Overlay::TextTop(ScreenTop(-HUD_PAUSE_HINT_DROP), HUD_PAUSE_HINT_SIZE), UiPx(HUD_PAUSE_HINT_SIZE));
			DingoUI::Text(m_PauseHintText.Get(), UITextParams(centered).SetSize(UiPx(HUD_PAUSE_HINT_SIZE)).SetColor(COLOR_TEXT));
		}
		{
			const Overlay::Band band(bottom, Overlay::ButtonTop(ScreenTop(HUD_PAUSE_BUTTON_Y), MENU_BUTTON_SIZE), 2.0f * Overlay::ButtonHeight(MENU_BUTTON_SIZE) + UiPx(MENU_BUTTON_GAP));
			const UIWidgetParams width = UIWidgetParams().SetWidth(UISize::Fixed(UiPx(MENU_BUTTON_WIDTH)));
			DingoUI::BeginBox(UIBoxParams().SetGap(UiPx(MENU_BUTTON_GAP)));
			if (Overlay::Button("Resume", MENU_BUTTON_SIZE, UIWidgetParams(width).SetDefaultFocus()))
				m_PauseChoice = PauseChoice::Resume;
			if (Overlay::Button("Return to title", MENU_BUTTON_SIZE, width))
				m_PauseChoice = PauseChoice::Title;
			DingoUI::EndBox();
		}
		DingoUI::EndRoot();
	}

	void Hud::Update(float deltaTime, const Lantern& lantern)
	{
		const float oilCenter = UI_SCREEN_UNITS - HUD_OIL_BAR_RISE;
		const float fillFraction = std::clamp(lantern.GetOil() / OIL_MAX, 0.0f, 1.0f);
		const bool snuffed = lantern.GetState() == Lantern::State::Snuffed;

		DeclareText("room", UIAnchor::TopLeft, HUD_PADDING, HUD_ROOM_LABEL_DROP, HUD_ROOM_LABEL_SIZE,
			m_Room >= 0 ? std::string_view(m_Map.GetRooms()[m_Room].Name) : std::string_view(), COLOR_TEXT);

		DeclareMeter("oil_meter", UIAnchor::TopLeft, HUD_PADDING, oilCenter - 0.5f * HUD_OIL_BAR_HEIGHT, HUD_OIL_BAR_WIDTH, HUD_OIL_BAR_HEIGHT,
			HUD_OIL_BAR_INSET, fillFraction, snuffed ? COLOR_METER_SNUFFED : COLOR_METER_LIT);

		const char* state = "Lit";
		glm::vec4 stateColor = COLOR_TITLE;
		if (lantern.IsOutOfOil())
		{
			state = "Out of oil";
			stateColor = COLOR_TEXT_ALERT;
		}
		else if (lantern.IsTooLowToStrike())
		{
			state = "Too little oil to strike";
			stateColor = COLOR_TEXT_ALERT;
		}
		else if (snuffed)
		{
			state = "Snuffed";
			stateColor = COLOR_TEXT_DIM;
		}
		else if (lantern.GetState() == Lantern::State::Striking)
		{
			state = "Striking...";
		}
		DeclareText("lantern_state", UIAnchor::TopLeft, HUD_PADDING, oilCenter - HUD_OIL_STATE_RISE, HUD_OIL_STATE_SIZE, state, stateColor);
		DeclareText("lantern_hint", UIAnchor::TopLeft, HUD_PADDING, oilCenter + HUD_OIL_HINT_DROP, HUD_OIL_HINT_SIZE, m_SnuffHint.Get(), COLOR_TEXT_DIM);

		const bool prompting = !m_Paused && m_PromptKind != BrazierPrompt::None;
		const bool holding = prompting && m_PromptKind != BrazierPrompt::NeedLantern && m_PromptKind != BrazierPrompt::NoOil;
		std::string_view prompt;
		glm::vec4 promptColor = COLOR_TEXT;
		if (!prompting)
		{
		}
		else if (m_PromptKind == BrazierPrompt::NeedLantern)
		{
			prompt = "Relight your lantern first";
			promptColor = COLOR_TEXT_DIM;
		}
		else if (m_PromptKind == BrazierPrompt::NoOil)
		{
			prompt = "Out of oil - find a flask or a lit brazier";
			promptColor = COLOR_TEXT_ALERT;
		}
		else if (m_PromptKind == BrazierPrompt::Refuel)
		{
			prompt = m_RefuelPrompt.Get();
		}
		else
		{
			prompt = m_PromptKind == BrazierPrompt::LightAltar ? m_LightAltarPrompt.Get() : m_LightBrazierPrompt.Get();
		}
		DeclareText("prompt", UIAnchor::Top, 0.0f, UI_SCREEN_UNITS - HUD_PROMPT_RISE, HUD_PROMPT_SIZE, prompt, promptColor);
		if (holding)
		{
			DeclareMeter("hold_meter", UIAnchor::Top, 0.0f, UI_SCREEN_UNITS - HUD_PROGRESS_RISE - 0.5f * HUD_PROGRESS_HEIGHT, HUD_PROGRESS_WIDTH, HUD_PROGRESS_HEIGHT,
				HUD_PROGRESS_INSET, m_Progress, COLOR_METER_LIT);
		}

		m_ToastTime = std::max(0.0f, m_ToastTime - deltaTime);
		glm::vec4 toastColor = COLOR_TITLE;
		toastColor.a = std::clamp(m_ToastTime / HUD_TOAST_FADE_TIME, 0.0f, 1.0f);
		DeclareText("toast", UIAnchor::Top, 0.0f, HUD_TOAST_DROP, HUD_TOAST_SIZE, m_ToastTime > 0.0f ? std::string_view(m_ToastText) : std::string_view(), toastColor);

		if (m_Fade > 0.0f && DingoUI::BeginRoot("fade", UIRootParams().SetAnchor(UIAnchor::Stretch).SetLayer(UI_LAYER_FADE).SetBlocksInput(false)
				.SetLayout(UIBoxParams().SetBackground(glm::vec4(glm::vec3(COLOR_FADE), COLOR_FADE.a * m_Fade)))))
		{
			DingoUI::EndRoot();
		}

		DeclarePause();
	}

}
