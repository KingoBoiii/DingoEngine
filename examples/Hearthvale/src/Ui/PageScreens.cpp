#include "Ui/Screens.h"
#include "Ui/ScreenParts.h"
#include "GameTuning.h"

#include <format>

namespace Dingo
{

	using namespace UiMetrics;

	namespace
	{
		glm::vec4 ToneColor(PageTone tone)
		{
			switch (tone)
			{
			case PageTone::Dim: return UI_TEXT_DIM_COLOR;
			case PageTone::Pick: return UI_TEXT_PICK_COLOR;
			case PageTone::Bad: return UI_TEXT_BAD_COLOR;
			case PageTone::Normal: break;
			}
			return UI_TEXT_COLOR;
		}

		void DrawTextCell(const UiKit& kit, const PageCell& cell)
		{
			if (!DingoUI::BeginBox(UIBoxParams().SetSize(UISize::Fixed(cell.Width), UISize::Fixed(cell.Height)).SetDirection(UIDirection::Column).SetJustify(UIJustify::Center)))
				return;
			if (!cell.Label.empty())
				DingoUI::Text(cell.Label, LineText(kit, cell.TextSize, ToneColor(cell.Tone), cell.Wrap).SetWidth(UISize::Fixed(cell.Width)));
			DingoUI::EndBox();
		}

		void DrawButtonCell(const UiKit& kit, ScreenHost& host, const PageSpec& page, const PageCell& cell, const std::string& focusRequest)
		{
			ApplyFocusRequest(focusRequest, cell.Label);
			const glm::vec4 fill = cell.Highlight ? glm::vec4(0.4f, 0.3f, 0.12f, 1.0f) : UI_BUTTON_COLOR;
			const glm::vec4 border = cell.Highlight ? UI_SELECTED_COLOR : glm::vec4(0.0f);
			const UIStyle style = TintedButton(kit.Theme, fill, cell.TextSize, ToneColor(cell.Highlight ? PageTone::Pick : cell.Tone), border);
			const UIInteraction result = DingoUI::Button(cell.Label, UIWidgetParams().SetSize(UISize::Fixed(cell.Width), UISize::Fixed(cell.Height))
				.SetStyle(&style).SetEnabled(cell.Enabled).SetDefaultFocus(cell.Label == page.DefaultFocus));
			if (result.Clicked)
				host.Act(cell.Action, cell.Arg, cell.Label);
		}

		void DrawSliderCell(ScreenHost& host, const PageSpec& page, const PageCell& cell, const std::string& focusRequest)
		{
			if (!DingoUI::BeginBox(UIBoxParams().SetSize(UISize::Fixed(cell.Width), UISize::Fixed(cell.Height)).SetDirection(UIDirection::Column)
				.SetJustify(UIJustify::Center).SetAlign(UIAlign::Center)))
				return;
			ApplyFocusRequest(focusRequest, cell.Label);
			float value = cell.Value;
			const UIInteraction result = DingoUI::Slider(cell.Label, &value, cell.Min, cell.Max, UISliderParams()
				.SetWidth(UISize::Fixed(cell.Width - 2.0f * PageMetrics::SLIDER_RING)).SetStep(cell.Step).SetShowValue(false)
				.SetEnabled(cell.Enabled).SetDefaultFocus(cell.Label == page.DefaultFocus));
			if (result.Changed)
				host.SetSlider(cell.Arg, value);
			DingoUI::EndBox();
		}
	}

	void DrawPage(const UiKit& kit, ScreenHost& host, const PageSpec& page, const std::string& focusRequest)
	{
		const ScreenLayout layout = LayoutPage(page);
		const std::string rootLabel = std::format("screen_{}", ToString(page.Id));

		if (!DingoUI::BeginRoot(rootLabel, ScreenRoot()))
			return;
		if (DingoUI::IsBackPressed())
			host.Back();

		if (DingoUI::BeginPanel(ScreenPanel(layout.Panel)))
		{
			if (!page.Title.empty() && DingoUI::BeginBox(UIBoxParams().SetSize(UISize::Fixed(page.ContentW), UISize::Fixed(page.HeaderH)).SetDirection(UIDirection::Row).SetGap(GAP).SetAlign(UIAlign::Center)))
			{
				DingoUI::Text(page.Title, LineText(kit, page.TitleSize, UI_TEXT_PICK_COLOR).SetWidth(UISize::Fixed(PageMetrics::TITLE_W)));
				if (!page.Header.empty())
					DingoUI::Text(page.Header, LineText(kit, TAB_SIZE, UI_TEXT_COLOR).SetWidth(UISize::Fixed(page.ContentW - PageMetrics::TITLE_W - GAP)));
				DingoUI::EndBox();
			}

			for (const PageRow& row : page.Rows)
			{
				if (!DingoUI::BeginBox(UIBoxParams().SetSize(UISize::Fixed(page.ContentW), UISize::Fixed(row.Height)).SetDirection(UIDirection::Row).SetGap(GAP)
					.SetPadding(row.Indent, 0.0f)))
					continue;
				for (const PageCell& cell : row.Cells)
				{
					switch (cell.Kind)
					{
					case CellKind::Text: DrawTextCell(kit, cell); break;
					case CellKind::Button: DrawButtonCell(kit, host, page, cell, focusRequest); break;
					case CellKind::Slider: DrawSliderCell(host, page, cell, focusRequest); break;
					}
				}
				DingoUI::EndBox();
			}
			DingoUI::EndPanel();
		}
		DingoUI::EndRoot();
	}

}
