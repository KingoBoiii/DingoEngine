#pragma once
#include "Farm/Calendar.h"
#include "Placing/BuildController.h"
#include "Placing/Inventory.h"
#include "Ui/ScreenStack.h"
#include "Ui/UiLayout.h"

#include <string>
#include <vector>

namespace Dingo
{

	// Design pixels, the units of a DingoUI root scaled with the screen. Every focusable widget has a fixed size, so the
	// rectangles below are what DingoUI lays out and the pure checks can reason about them.
	namespace UiMetrics
	{
		inline constexpr float PAD = 20.0f;
		inline constexpr float GAP = 8.0f;
		inline constexpr float HEADER_H = 36.0f;
		inline constexpr float TAB_H = 40.0f;
		inline constexpr float CAPTION_H = 22.0f;
		inline constexpr float DETAIL_H = 44.0f;
		inline constexpr float BUTTON_PAD_X = 6.0f;
		inline constexpr float BUTTON_PAD_Y = 4.0f;

		inline constexpr float SLOT_W = 112.0f;
		inline constexpr float SLOT_BUTTON_H = 60.0f;
		inline constexpr float SLOT_COUNT_H = 20.0f;
		inline constexpr float SLOT_H = SLOT_BUTTON_H + SLOT_COUNT_H;
		// The backpack starts under this hotbar slot, so Up and Down between the two rows stay in a column.
		inline constexpr int GRID_START_COLUMN = 1;

		inline constexpr float CELL_W = 176.0f;
		inline constexpr float CELL_BUTTON_H = 48.0f;
		inline constexpr float CELL_COST_H = 36.0f;
		inline constexpr float CELL_H = CELL_BUTTON_H + CELL_COST_H;
		inline constexpr float LABEL_COL_W = 110.0f;
		// Tabs are as wide as the cells below them and line up with them, so a sideways move along a row never lands on a tab.
		inline constexpr float PALETTE_TAB_INDENT = LABEL_COL_W + GAP;

		inline constexpr float TITLE_SIZE = 26.0f;
		inline constexpr float TAB_SIZE = 16.0f;
		inline constexpr float CAPTION_SIZE = 15.0f;
		inline constexpr float SLOT_SIZE = 15.0f;
		inline constexpr float COUNT_SIZE = 14.0f;
		inline constexpr float CELL_SIZE = 16.0f;
		inline constexpr float COST_SIZE = 14.0f;
		inline constexpr float DETAIL_SIZE = 16.0f;
		inline constexpr int SLOT_NAME_CHARS = 9;
		inline constexpr int BUILD_NAME_CHARS = 14;

		inline constexpr float HUD_MARGIN = 16.0f;
		inline constexpr float HUD_PAD_X = 14.0f;
		inline constexpr float HUD_PAD_Y = 8.0f;
		inline constexpr float HUD_SIDE_W = 320.0f;
		inline constexpr float HUD_LINE_SIZE = 20.0f;
		inline constexpr float HUD_SLOT_W = 96.0f;
		inline constexpr float HUD_SLOT_H = 64.0f;
		inline constexpr float HUD_SLOT_GAP = 6.0f;
		inline constexpr float HUD_SLOT_SIZE = 13.0f;
		inline constexpr float HUD_SLOT_PAD = 4.0f;
		inline constexpr float HUD_STRIP_W = 960.0f;
		inline constexpr float HUD_STRIP_SIZE = 15.0f;
		inline constexpr float HUD_TOAST_SIZE = 18.0f;
		inline constexpr float HUD_BAR_H = 8.0f;
	}

	inline constexpr const char* TAB_INVENTORY_LABEL = "Inventory##tab";
	inline constexpr const char* TAB_BUILD_LABEL = "Build##tab";
	inline constexpr const char* TAB_TOOLS_LABEL = "Use tools##tab";

	struct ScreenWidget
	{
		std::string Label;
		UiRect Rect;
		float TextSize = 0.0f;
	};

	struct ScreenText
	{
		std::string Text;
		float Size = 0.0f;
		UiRect Box;
		bool Wrap = false;
	};

	// Coordinates are relative to the panel's top-left corner. Widgets are in declaration order, which is DingoUI's Tab order.
	struct ScreenLayout
	{
		ScreenId Id = ScreenId::None;
		UiRect Panel;
		std::vector<ScreenWidget> Widgets;
		std::vector<ScreenText> Texts;
		std::string DefaultFocus;

		std::vector<UiRect> WidgetRects() const;
		int IndexOf(const std::string& label) const;
	};

	// The part of a label DingoUI shows: everything before "##".
	std::string VisibleLabel(const std::string& label);
	std::string SlotLabel(int index, const ItemStack& stack);
	std::string SlotCountText(const ItemStack& stack);
	std::string BuildLabel(const BuildDef& def);
	std::string BuildCaption(const BuildDef& def, const Inventory& inventory);
	const char* CategoryName(BuildCategory category);

	std::string InventoryDetail(int picked, const Inventory& inventory);
	std::string PaletteDetail(BuildId selected, const Inventory& inventory);

	ScreenLayout BuildInventoryLayout(const Inventory& inventory, int picked);
	ScreenLayout BuildPaletteLayout(const Inventory& inventory, BuildId selected);

	enum class NavDir : uint8_t { Left, Right, Up, Down };

	// DingoUI's directional focus move over the widgets' rectangles (DingoUINav.cpp FindNavTarget): the nearest widget
	// in the direction, scored by distance along the move, twice the gap across it, and a quarter of the offset.
	// Returns -1 when nothing lies that way.
	int FindNavTarget(const std::vector<UiRect>& rects, int origin, NavDir direction);
	// Tab order with wraparound.
	int FindTabTarget(int count, int origin, bool backwards);
	// How many widgets a player reaches from `start` with directional moves and Tab, the start included.
	int CountReachable(const std::vector<UiRect>& rects, int start);

	struct HudSlot
	{
		std::string Text;
		glm::vec4 Color{ 0.0f };
		bool Selected = false;
	};

	struct HudContent
	{
		std::string Clock;
		std::string Coins;
		std::string Resources;
		std::string StripA;
		std::string StripB;
		bool BuildMode = false;
		bool Affordable = true;
		float DayProgress = 0.0f;
		std::vector<HudSlot> Hotbar;
	};

	HudSlot MakeHudSlot(const ItemStack& stack, bool selected);
	std::string HudPlaceLine(PlaceResult preview, int rotationQuarterTurns);
	HudContent BuildHudContent(const Calendar& calendar, const Inventory& inventory, const BuildController& controller);

}
