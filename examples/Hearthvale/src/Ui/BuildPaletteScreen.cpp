#include "Ui/Screens.h"
#include "Ui/ScreenParts.h"
#include "GameTuning.h"

#include <format>

namespace Dingo
{

	using namespace UiMetrics;

	namespace
	{
		void DrawCell(const UiKit& kit, ScreenHost& host, const Inventory& inventory, const BuildController& controller, const BuildDef& def, const std::string& focusRequest, bool defaultFocus)
		{
			const std::string label = BuildLabel(def);
			const bool known = inventory.KnowsPlan(def.Id);
			const bool affordable = known && CanAfford(inventory, def.Cost);
			const bool current = controller.GetMode() == BuildMode::Build && controller.GetBuildable() == def.Id;

			glm::vec4 text = UI_TEXT_COLOR;
			if (!known)
				text = UI_TEXT_DIM_COLOR;
			else if (!affordable)
				text = UI_TEXT_BAD_COLOR;
			const UIStyle style = TintedButton(kit.Cell, UI_BUTTON_COLOR, CELL_SIZE, text, current ? UI_SELECTED_COLOR : glm::vec4(0.0f));

			if (!DingoUI::BeginBox(UIBoxParams().SetSize(UISize::Fixed(CELL_W), UISize::Fixed(CELL_H)).SetDirection(UIDirection::Column)))
				return;
			ApplyFocusRequest(focusRequest, label);
			if (DingoUI::Button(label, UIWidgetParams().SetSize(UISize::Fixed(CELL_W), UISize::Fixed(CELL_BUTTON_H)).SetStyle(&style).SetDefaultFocus(defaultFocus)).Clicked)
				host.ChooseBuildable(def.Id);
			DrawCaption(kit, BuildCaption(def, inventory), CELL_W, CELL_COST_H, COST_SIZE, known && affordable ? UI_TEXT_DIM_COLOR : UI_TEXT_BAD_COLOR, false);
			DingoUI::EndBox();
		}
	}

	void DrawBuildPaletteScreen(const UiKit& kit, ScreenHost& host, const Inventory& inventory, const BuildController& controller, const std::string& focusRequest)
	{
		const BuildId selected = controller.GetBuildable();
		const ScreenLayout layout = BuildPaletteLayout(inventory, selected);
		const float contentW = layout.Panel.W - 2.0f * PAD;

		if (!DingoUI::BeginRoot("screen_build", ScreenRoot()))
			return;
		if (DingoUI::IsBackPressed())
			host.Back();

		if (DingoUI::BeginPanel(ScreenPanel(layout.Panel)))
		{
			DrawHeader(kit, "Build", std::format("Coins {}   Wood {}   Stone {}   Fibre {}", inventory.GetCoins(),
				inventory.Count(ItemId::Wood), inventory.Count(ItemId::Stone), inventory.Count(ItemId::Fibre)), contentW);

			if (DingoUI::BeginBox(UIBoxParams().SetSize(UISize::Fixed(contentW), UISize::Fixed(TAB_H)).SetDirection(UIDirection::Row).SetGap(GAP)
				.SetPadding(PALETTE_TAB_INDENT, 0.0f, 0.0f, 0.0f)))
			{
				if (DrawTab(kit, TAB_INVENTORY_LABEL, CELL_W, false, focusRequest))
					host.SwitchScreen(true);
				DrawTab(kit, TAB_BUILD_LABEL, CELL_W, true, focusRequest);
				if (DrawTab(kit, TAB_TOOLS_LABEL, CELL_W, false, focusRequest))
					host.UseTools();
				DingoUI::EndBox();
			}

			const std::string defaultFocus = layout.DefaultFocus;
			constexpr int k_Categories = static_cast<int>(BuildCategory::Building) + 1;
			for (int category = 0; category < k_Categories; ++category)
			{
				if (!DingoUI::BeginBox(UIBoxParams().SetSize(UISize::Fixed(contentW), UISize::Fixed(CELL_H)).SetDirection(UIDirection::Row).SetGap(GAP)))
					continue;
				DrawCaption(kit, CategoryName(static_cast<BuildCategory>(category)), LABEL_COL_W, CELL_H, CELL_SIZE, UI_TEXT_PICK_COLOR, false);
				for (const BuildDef& def : GetBuildDefs())
				{
					if (static_cast<int>(def.Category) == category)
						DrawCell(kit, host, inventory, controller, def, focusRequest, BuildLabel(def) == defaultFocus);
				}
				DingoUI::EndBox();
			}

			DrawCaption(kit, PaletteDetail(selected, inventory), contentW, DETAIL_H, DETAIL_SIZE, UI_TEXT_COLOR, true);
			DingoUI::EndPanel();
		}
		DingoUI::EndRoot();
	}

}
