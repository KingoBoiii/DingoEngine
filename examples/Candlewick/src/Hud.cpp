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

	std::string LightPrompt(const char* target)
	{
		if (!Input::IsGamepadConnected())
			return std::string("Hold E to light the ") + target;
		return std::string("Hold E / ") + Overlay::PadLabel(GamepadButton::A) + " to light the " + target;
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

	void SetText(Entity entity, std::string_view text, const glm::vec4& color)
	{
		auto& component = entity.GetComponent<TextComponent>();
		if (component.Text != text)
			component.Text = text;
		component.Color = color;
	}

	void SetAlpha(Entity entity, float alpha)
	{
		entity.GetComponent<SpriteRendererComponent>().Color.a = alpha;
	}
}

namespace Dingo
{

	Hud::Hud(Scene& scene, const KeepMap& map, const Lantern& lantern, int startRoom)
		: m_Map(map)
		, m_SnuffHint(&SnuffHint)
		, m_LightBrazierPrompt([] { return LightPrompt("brazier"); })
		, m_LightAltarPrompt([] { return LightPrompt("altar"); })
		, m_PauseHintText(&PauseHint)
	{
		m_Font = Overlay::LoadFont("HUD");
		Overlay::MakeCamera(scene, "UICamera", HUD_ORTHO_SIZE);
		m_RoomLabel = Overlay::MakeText(scene, m_Font, "RoomLabel", HUD_ROOM_LABEL_SIZE, COLOR_TEXT, false);

		m_OilBackground = scene.CreateEntity("OilMeterBackground");
		m_OilBackground.AddComponent<SpriteRendererComponent>().Color = COLOR_METER_BG;
		m_OilFill = scene.CreateEntity("OilMeterFill");
		m_OilFill.AddComponent<SpriteRendererComponent>().Color = COLOR_METER_LIT;

		m_LanternState = Overlay::MakeText(scene, m_Font, "LanternState", HUD_OIL_STATE_SIZE, COLOR_TEXT, false);
		m_KeyHint = Overlay::MakeText(scene, m_Font, "LanternHint", HUD_OIL_HINT_SIZE, COLOR_TEXT_DIM, false);

		m_Prompt = Overlay::MakeText(scene, m_Font, "BrazierPrompt", HUD_PROMPT_SIZE, COLOR_TEXT, true);
		m_ProgressBackground = scene.CreateEntity("BrazierProgressBackground");
		m_ProgressBackground.AddComponent<SpriteRendererComponent>().Color = glm::vec4(glm::vec3(COLOR_METER_BG), 0.0f);
		m_ProgressFill = scene.CreateEntity("BrazierProgressFill");
		m_ProgressFill.AddComponent<SpriteRendererComponent>().Color = glm::vec4(glm::vec3(COLOR_METER_LIT), 0.0f);

		m_Toast = Overlay::MakeText(scene, m_Font, "Toast", HUD_TOAST_SIZE, COLOR_TITLE, true);

		m_PauseDim = scene.CreateEntity("PauseDim");
		m_PauseDim.AddComponent<SpriteRendererComponent>().Color = glm::vec4(glm::vec3(COLOR_PAUSE_DIM), 0.0f);
		m_PauseTitle = Overlay::MakeText(scene, m_Font, "PauseTitle", HUD_PAUSE_TITLE_SIZE, COLOR_TITLE, true);
		m_PauseHint = Overlay::MakeText(scene, m_Font, "PauseHint", HUD_PAUSE_HINT_SIZE, COLOR_TEXT, true);

		m_Fade = scene.CreateEntity("Fade");
		m_Fade.AddComponent<SpriteRendererComponent>().Color = glm::vec4(glm::vec3(COLOR_FADE), 0.0f);

		SetRoom(startRoom);
		Update(0.0f, lantern);
	}

	Hud::~Hud()
	{
		DestroyAndDelete(m_Font);
	}

	void Hud::SetRoom(int room)
	{
		if (room < 0 || room == m_Room || room >= static_cast<int>(m_Map.GetRooms().size()))
			return;

		m_Room = room;
		m_RoomLabel.GetComponent<TextComponent>().Text = m_Map.GetRooms()[room].Name;
	}

	void Hud::SetFade(float amount)
	{
		m_Fade.GetComponent<SpriteRendererComponent>().Color.a = COLOR_FADE.a * std::clamp(amount, 0.0f, 1.0f);
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

	void Hud::Update(float deltaTime, const Lantern& lantern)
	{
		const glm::vec2 viewport = Application::Get().GetRenderer2D().GetViewportSize();
		const float aspect = (viewport.y > 0.0f) ? viewport.x / viewport.y : 1.0f;
		const float halfH = HUD_ORTHO_SIZE * 0.5f;
		const float halfW = halfH * aspect;

		auto& fade = m_Fade.GetComponent<TransformComponent>();
		fade.Position = { 0.0f, 0.0f, HUD_FADE_Z };
		fade.Size = { 2.0f * halfW + HUD_FADE_MARGIN, 2.0f * halfH + HUD_FADE_MARGIN };

		auto& dim = m_PauseDim.GetComponent<TransformComponent>();
		dim.Position = { 0.0f, 0.0f, HUD_PAUSE_Z };
		dim.Size = fade.Size;
		SetAlpha(m_PauseDim, m_Paused ? COLOR_PAUSE_DIM.a : 0.0f);

		m_RoomLabel.GetComponent<TransformComponent>().Position = { -halfW + HUD_PADDING, halfH - HUD_ROOM_LABEL_DROP, 0.0f };

		const float barLeft = -halfW + HUD_PADDING;
		const float barY = -halfH + HUD_OIL_BAR_RISE;
		const float fillMax = HUD_OIL_BAR_WIDTH - 2.0f * HUD_OIL_BAR_INSET;
		const float fillWidth = fillMax * std::clamp(lantern.GetOil() / OIL_MAX, 0.0f, 1.0f);
		const bool snuffed = lantern.GetState() == Lantern::State::Snuffed;

		auto& background = m_OilBackground.GetComponent<TransformComponent>();
		background.Position = { barLeft + HUD_OIL_BAR_WIDTH * 0.5f, barY, 0.0f };
		background.Size = { HUD_OIL_BAR_WIDTH, HUD_OIL_BAR_HEIGHT };

		// Equal-z sprites are not ordered, so the fill sits a hair in front of its background.
		auto& fill = m_OilFill.GetComponent<TransformComponent>();
		fill.Position = { barLeft + HUD_OIL_BAR_INSET + fillWidth * 0.5f, barY, HUD_OIL_FILL_LIFT };
		fill.Size = { fillWidth, HUD_OIL_BAR_HEIGHT - 2.0f * HUD_OIL_BAR_INSET };
		m_OilFill.GetComponent<SpriteRendererComponent>().Color = snuffed ? COLOR_METER_SNUFFED : COLOR_METER_LIT;

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

		m_LanternState.GetComponent<TransformComponent>().Position = { barLeft, barY + HUD_OIL_STATE_RISE, 0.0f };
		SetText(m_LanternState, state, stateColor);

		m_KeyHint.GetComponent<TransformComponent>().Position = { barLeft, barY - HUD_OIL_HINT_DROP, 0.0f };
		SetText(m_KeyHint, m_SnuffHint.Get(), COLOR_TEXT_DIM);

		const bool prompting = !m_Paused && m_PromptKind != BrazierPrompt::None;
		const bool holding = prompting && m_PromptKind != BrazierPrompt::NeedLantern;
		m_Prompt.GetComponent<TransformComponent>().Position = { 0.0f, -halfH + HUD_PROMPT_RISE, 0.0f };
		if (!prompting)
			SetText(m_Prompt, "", COLOR_TEXT);
		else if (holding)
			SetText(m_Prompt, m_PromptKind == BrazierPrompt::LightAltar ? m_LightAltarPrompt.Get() : m_LightBrazierPrompt.Get(), COLOR_TEXT);
		else
			SetText(m_Prompt, "Relight your lantern first", COLOR_TEXT_DIM);

		const float progressFillMax = HUD_PROGRESS_WIDTH - 2.0f * HUD_PROGRESS_INSET;
		const float progressWidth = progressFillMax * m_Progress;
		const float progressY = -halfH + HUD_PROGRESS_RISE;
		auto& progressBackground = m_ProgressBackground.GetComponent<TransformComponent>();
		progressBackground.Position = { 0.0f, progressY, 0.0f };
		progressBackground.Size = { HUD_PROGRESS_WIDTH, HUD_PROGRESS_HEIGHT };
		auto& progressFill = m_ProgressFill.GetComponent<TransformComponent>();
		progressFill.Position = { -progressFillMax * 0.5f + progressWidth * 0.5f, progressY, HUD_OIL_FILL_LIFT };
		progressFill.Size = { progressWidth, HUD_PROGRESS_HEIGHT - 2.0f * HUD_PROGRESS_INSET };
		SetAlpha(m_ProgressBackground, holding ? COLOR_METER_BG.a : 0.0f);
		SetAlpha(m_ProgressFill, holding ? COLOR_METER_LIT.a : 0.0f);

		m_ToastTime = std::max(0.0f, m_ToastTime - deltaTime);
		glm::vec4 toastColor = COLOR_TITLE;
		toastColor.a = std::clamp(m_ToastTime / HUD_TOAST_FADE_TIME, 0.0f, 1.0f);
		m_Toast.GetComponent<TransformComponent>().Position = { 0.0f, halfH - HUD_TOAST_DROP, 0.0f };
		SetText(m_Toast, m_ToastTime > 0.0f ? std::string_view(m_ToastText) : std::string_view(), toastColor);

		m_PauseTitle.GetComponent<TransformComponent>().Position = { 0.0f, HUD_PAUSE_TITLE_RISE, 0.0f };
		m_PauseHint.GetComponent<TransformComponent>().Position = { 0.0f, -HUD_PAUSE_HINT_DROP, 0.0f };
		SetText(m_PauseTitle, m_Paused ? "Paused" : "", COLOR_TITLE);
		SetText(m_PauseHint, m_Paused ? std::string_view(m_PauseHintText.Get()) : std::string_view(), COLOR_TEXT);
	}

}
