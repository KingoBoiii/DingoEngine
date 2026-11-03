#include "Ui/Screens.h"
#include "Ui/ScreenParts.h"
#include "GameTuning.h"

#include <format>

namespace Dingo
{

	using namespace UiMetrics;

	namespace
	{
		void DrawSlot(const UiKit& kit, ScreenHost& host, const Inventory& inventory, int index, const std::string& focusRequest, const std::string& defaultLabel)
		{
			const ItemStack& stack = inventory.GetSlot(index);
			const std::string label = SlotLabel(index, stack);
			const bool picked = host.GetPicked() == index;
			const bool equipped = index == inventory.GetSelectedSlot();

			glm::vec4 fill = UI_BUTTON_COLOR;
			if (!stack.Empty())
				fill = ItemSwatch(stack.Item) * glm::vec4(0.55f, 0.55f, 0.55f, 1.0f);
			glm::vec4 border(0.0f);
			if (picked)
				border = UI_TEXT_PICK_COLOR;
			else if (equipped)
				border = UI_SELECTED_COLOR;
			const UIStyle style = TintedButton(kit.Theme, fill, SLOT_SIZE, stack.Empty() ? UI_TEXT_DIM_COLOR : UI_TEXT_COLOR, border);

			if (!DingoUI::BeginBox(UIBoxParams().SetSize(UISize::Fixed(SLOT_W), UISize::Fixed(SLOT_H)).SetDirection(UIDirection::Column)))
				return;
			ApplyFocusRequest(focusRequest, label);
			if (DingoUI::Button(label, UIWidgetParams().SetSize(UISize::Fixed(SLOT_W), UISize::Fixed(SLOT_BUTTON_H)).SetStyle(&style).SetDefaultFocus(label == defaultLabel)).Clicked)
				host.ActivateSlot(index);
			DrawCaption(kit, SlotCountText(stack), SLOT_W, SLOT_COUNT_H, COUNT_SIZE, UI_TEXT_COLOR, false);
			DingoUI::EndBox();
		}
	}

	void DrawInventoryScreen(const UiKit& kit, ScreenHost& host, const Inventory& inventory, const std::string& focusRequest)
	{
		const ScreenLayout layout = BuildInventoryLayout(inventory, host.GetPicked());
		const float contentW = layout.Panel.W - 2.0f * PAD;

		if (!DingoUI::BeginRoot("screen_inventory", ScreenRoot()))
			return;
		if (DingoUI::IsBackPressed())
			host.Back();

		if (DingoUI::BeginPanel(ScreenPanel(layout.Panel)))
		{
			DrawHeader(kit, "Inventory", std::format("Coins {}   Wood {}   Stone {}   Fibre {}", inventory.GetCoins(),
				inventory.Count(ItemId::Wood), inventory.Count(ItemId::Stone), inventory.Count(ItemId::Fibre)), contentW);

			if (DingoUI::BeginBox(UIBoxParams().SetSize(UISize::Fixed(contentW), UISize::Fixed(TAB_H)).SetDirection(UIDirection::Row).SetGap(GAP)))
			{
				DrawTab(kit, TAB_INVENTORY_LABEL, SLOT_W, true, focusRequest);
				if (DrawTab(kit, TAB_BUILD_LABEL, SLOT_W, false, focusRequest))
					host.SwitchScreen(true);
				DingoUI::EndBox();
			}

			DrawCaption(kit, "Hotbar", contentW, CAPTION_H, CAPTION_SIZE, UI_TEXT_DIM_COLOR, false);
			if (DingoUI::BeginBox(UIBoxParams().SetSize(UISize::Fixed(contentW), UISize::Fixed(SLOT_H)).SetDirection(UIDirection::Row).SetGap(GAP)))
			{
				for (int i = 0; i < HOTBAR_SLOTS; ++i)
					DrawSlot(kit, host, inventory, i, focusRequest, layout.DefaultFocus);
				DingoUI::EndBox();
			}

			DrawCaption(kit, "Backpack", contentW, CAPTION_H, CAPTION_SIZE, UI_TEXT_DIM_COLOR, false);
			const float gridH = static_cast<float>(INVENTORY_ROWS) * SLOT_H + static_cast<float>(INVENTORY_ROWS - 1) * GAP;
			if (DingoUI::BeginBox(UIBoxParams().SetSize(UISize::Fixed(contentW), UISize::Fixed(gridH)).SetDirection(UIDirection::Column).SetGap(GAP)))
			{
				for (int row = 0; row < INVENTORY_ROWS; ++row)
				{
					if (!DingoUI::BeginBox(UIBoxParams().SetSize(UISize::Fixed(contentW), UISize::Fixed(SLOT_H)).SetDirection(UIDirection::Row).SetGap(GAP)
						.SetPadding(static_cast<float>(GRID_START_COLUMN) * (SLOT_W + GAP), 0.0f)))
						continue;
					for (int column = 0; column < INVENTORY_COLUMNS; ++column)
						DrawSlot(kit, host, inventory, HOTBAR_SLOTS + row * INVENTORY_COLUMNS + column, focusRequest, layout.DefaultFocus);
					DingoUI::EndBox();
				}
				DingoUI::EndBox();
			}

			DrawCaption(kit, InventoryDetail(host.GetPicked(), inventory), contentW, DETAIL_H, DETAIL_SIZE, UI_TEXT_COLOR, true);
			DingoUI::EndPanel();
		}
		DingoUI::EndRoot();
	}

}
