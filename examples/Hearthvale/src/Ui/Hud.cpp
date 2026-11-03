#include "Ui/Hud.h"
#include "GameTuning.h"

namespace Dingo
{

	using namespace UiMetrics;

	namespace
	{
		UIRootParams CornerRoot(UIAnchor anchor, const glm::vec2& offset)
		{
			return UIRootParams().SetAnchor(anchor).SetOffset(offset).SetLayer(UI_HUD_LAYER).SetBlocksInput(false).SetNavigable(false)
				.SetScale(UIScale::ScaleWithScreen())
				.SetLayout(UIBoxParams().SetSize(UISize::Fixed(HUD_SIDE_W), UISize::Fit()).SetDirection(UIDirection::Column).SetGap(4.0f)
					.SetPadding(HUD_PAD_X, HUD_PAD_Y).SetBackground(UI_HUD_PANEL_COLOR).SetCornerRadius(6.0f));
		}
	}

	void Hud::Draw(const UiKit& kit, const HudContent& content, const std::string& toast, bool showHints)
	{
		DrawClock(kit, content);
		DrawResources(kit, content);
		DrawBottom(kit, content, toast, showHints);
	}

	void Hud::DrawClock(const UiKit& kit, const HudContent& content)
	{
		if (!DingoUI::BeginRoot("hud_clock", CornerRoot(UIAnchor::TopLeft, { HUD_MARGIN, HUD_MARGIN })))
			return;
		DingoUI::Text(content.Clock, LineText(kit, HUD_LINE_SIZE, UI_TEXT_COLOR));
		DingoUI::ProgressBar("day", content.DayProgress, UIWidgetParams().SetSize(UISize::Fixed(HUD_SIDE_W - 2.0f * HUD_PAD_X), UISize::Fixed(HUD_BAR_H)));
		DingoUI::EndRoot();
	}

	void Hud::DrawResources(const UiKit& kit, const HudContent& content)
	{
		if (!DingoUI::BeginRoot("hud_resources", CornerRoot(UIAnchor::TopRight, { -HUD_MARGIN, HUD_MARGIN })))
			return;
		DingoUI::Text(content.Coins, LineText(kit, HUD_LINE_SIZE, UI_TEXT_PICK_COLOR));
		DingoUI::Text(content.Resources, LineText(kit, HUD_LINE_SIZE, UI_TEXT_COLOR));
		DingoUI::EndRoot();
	}

	void Hud::DrawBottom(const UiKit& kit, const HudContent& content, const std::string& toast, bool showHints)
	{
		if (!DingoUI::BeginRoot("hud_bottom", UIRootParams().SetAnchor(UIAnchor::Bottom).SetOffset({ 0.0f, -HUD_MARGIN }).SetLayer(UI_HUD_LAYER)
			.SetBlocksInput(false).SetNavigable(false).SetScale(UIScale::ScaleWithScreen())
			.SetLayout(UIBoxParams().SetSize(UISize::Fixed(HUD_STRIP_W), UISize::Fit()).SetDirection(UIDirection::Column).SetGap(HUD_SLOT_GAP).SetAlign(UIAlign::Center))))
			return;

		if (!toast.empty())
		{
			if (DingoUI::BeginBox(UIBoxParams().SetPadding(HUD_PAD_X, HUD_PAD_Y).SetBackground(UI_HUD_PANEL_COLOR).SetCornerRadius(6.0f)))
			{
				DingoUI::Text(toast, LineText(kit, HUD_TOAST_SIZE, UI_TEXT_PICK_COLOR));
				DingoUI::EndBox();
			}
		}

		if (DingoUI::BeginBox(UIBoxParams().SetDirection(UIDirection::Row).SetGap(HUD_SLOT_GAP)))
		{
			for (size_t i = 0; i < content.Hotbar.size(); ++i)
			{
				const HudSlot& slot = content.Hotbar[i];
				UIBoxParams tile = UIBoxParams().SetSize(UISize::Fixed(HUD_SLOT_W), UISize::Fixed(HUD_SLOT_H)).SetPadding(HUD_SLOT_PAD)
					.SetBackground(slot.Color * glm::vec4(0.6f, 0.6f, 0.6f, 1.0f)).SetCornerRadius(5.0f).SetJustify(UIJustify::Center).SetAlign(UIAlign::Center);
				if (slot.Selected)
					tile.SetBorder(3.0f, UI_SELECTED_COLOR);
				else
					tile.SetBorder(1.0f, glm::vec4(0.0f, 0.0f, 0.0f, 0.6f));
				DingoUI::PushId(static_cast<int>(i));
				if (DingoUI::BeginBox(tile))
				{
					if (!slot.Text.empty())
						DingoUI::Text(slot.Text, LineText(kit, HUD_SLOT_SIZE, UI_TEXT_COLOR).SetAlign(UITextAlign::Center));
					DingoUI::EndBox();
				}
				DingoUI::PopId();
			}
			DingoUI::EndBox();
		}

		if (showHints)
		{
			if (DingoUI::BeginBox(UIBoxParams().SetDirection(UIDirection::Column).SetAlign(UIAlign::Center).SetPadding(HUD_PAD_X, HUD_PAD_Y)
				.SetBackground(UI_HUD_PANEL_COLOR).SetCornerRadius(6.0f)))
			{
				const glm::vec4 stripA = content.BuildMode && !content.Affordable ? UI_TEXT_BAD_COLOR : UI_TEXT_PICK_COLOR;
				DingoUI::Text(content.StripA, LineText(kit, HUD_STRIP_SIZE, stripA));
				DingoUI::Text(content.StripB, LineText(kit, HUD_STRIP_SIZE, UI_TEXT_COLOR));
				DingoUI::EndBox();
			}
		}
		DingoUI::EndRoot();
	}

}
