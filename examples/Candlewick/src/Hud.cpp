#include "Hud.h"
#include "GameTuning.h"
#include "KeepMap.h"
#include "Lantern.h"
#include "Overlay.h"

#include <algorithm>
#include <string_view>

namespace
{
	using namespace Dingo;

	const char* SnuffHint()
	{
		if (!Input::IsGamepadConnected())
			return "Q  snuff / relight";
		return Input::GetGamepadType() == GamepadType::PlayStation ? "Q / (Square)  snuff / relight" : "Q / (X)  snuff / relight";
	}

	void SetText(Entity entity, std::string_view text, const glm::vec4& color)
	{
		auto& component = entity.GetComponent<TextComponent>();
		if (component.Text != text)
			component.Text = text;
		component.Color = color;
	}
}

namespace Dingo
{

	Hud::Hud(Scene& scene, const KeepMap& map, const Lantern& lantern, int startRoom)
		: m_Map(map)
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

		m_Fade = scene.CreateEntity("Fade");
		m_Fade.AddComponent<SpriteRendererComponent>().Color = glm::vec4(glm::vec3(COLOR_FADE), 0.0f);

		SetRoom(startRoom);
		Update(lantern);
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

	void Hud::Update(const Lantern& lantern)
	{
		const glm::vec2 viewport = Application::Get().GetRenderer2D().GetViewportSize();
		const float aspect = (viewport.y > 0.0f) ? viewport.x / viewport.y : 1.0f;
		const float halfH = HUD_ORTHO_SIZE * 0.5f;
		const float halfW = halfH * aspect;

		auto& fade = m_Fade.GetComponent<TransformComponent>();
		fade.Position = { 0.0f, 0.0f, HUD_FADE_Z };
		fade.Size = { 2.0f * halfW + HUD_FADE_MARGIN, 2.0f * halfH + HUD_FADE_MARGIN };

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
		SetText(m_KeyHint, SnuffHint(), COLOR_TEXT_DIM);
	}

}
