#include "Look/ClockHud.h"
#include "GameTuning.h"

#include <format>

namespace Dingo
{

	namespace
	{
		constexpr const char* k_FontPath = "fonts/Inter-Bold.ttf";
		constexpr float k_Margin = 16.0f;
		constexpr float k_PaddingX = 14.0f;
		constexpr float k_PaddingY = 8.0f;
	}

	ClockHud::ClockHud()
	{
		AssetManager& assets = Application::Get().GetAssetManager();
		m_Font = assets.GetFont(assets.Load(k_FontPath));
		if (!m_Font)
		{
			DE_ERROR("Hearthvale: failed to load font '{}'", k_FontPath);
		}
	}

	void ClockHud::Draw(const Calendar& calendar)
	{
		if (!m_Font)
			return;

		if (!DingoUI::BeginRoot("clock", UIRootParams().SetAnchor(UIAnchor::TopLeft).SetOffset({ k_Margin, k_Margin }).SetBlocksInput(false)
			.SetLayout(UIBoxParams().SetPadding(k_PaddingX, k_PaddingY).SetBackground(HUD_PANEL_COLOR))))
			return;

		const std::string text = std::format("{} {}  {}  {}", ToString(calendar.GetSeason()), calendar.GetDayOfSeason(), calendar.GetClockText(), ToString(calendar.GetWeather()));
		DingoUI::Text(text, UITextParams().SetSize(HUD_CLOCK_SIZE).SetColor(HUD_TEXT_COLOR).SetWrap(false).SetFont(m_Font));
		DingoUI::EndRoot();
	}

}
