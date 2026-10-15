#include "Ui/ScreenLayouts.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>

namespace Dingo
{

	namespace
	{
		using namespace UiMetrics;

		constexpr float k_MinStep = 0.5f;
		constexpr float k_HeaderTitleW = 220.0f;

		std::string ItemName(const ItemStack& stack)
		{
			return stack.Empty() || stack.Item >= ItemId::Count ? std::string("Empty") : std::string(GetItemDef(stack.Item).Name);
		}

		std::string ResourceLine(const Inventory& inventory)
		{
			return std::format("Coins {}   Wood {}   Stone {}   Fibre {}", inventory.GetCoins(), inventory.Count(ItemId::Wood),
				inventory.Count(ItemId::Stone), inventory.Count(ItemId::Fibre));
		}

		void AddHeader(ScreenLayout& layout, const char* title, const Inventory& inventory, float contentW, float y)
		{
			layout.Texts.push_back({ title, TITLE_SIZE, { PAD, y, k_HeaderTitleW, HEADER_H }, false });
			layout.Texts.push_back({ ResourceLine(inventory), TAB_SIZE, { PAD + k_HeaderTitleW + GAP, y, contentW - k_HeaderTitleW - GAP, HEADER_H }, false });
		}

		void AddTab(ScreenLayout& layout, const char* label, int index, float indent, float width, float y)
		{
			layout.Widgets.push_back({ label, { PAD + indent + static_cast<float>(index) * (width + GAP), y, width, TAB_H }, TAB_SIZE });
		}

		void AddSlot(ScreenLayout& layout, const Inventory& inventory, int index, float x, float y)
		{
			const ItemStack& stack = inventory.GetSlot(index);
			layout.Widgets.push_back({ SlotLabel(index, stack), { x, y, SLOT_W, SLOT_BUTTON_H }, SLOT_SIZE });
			const std::string count = SlotCountText(stack);
			if (!count.empty())
				layout.Texts.push_back({ count, COUNT_SIZE, { x, y + SLOT_BUTTON_H, SLOT_W, SLOT_COUNT_H }, false });
		}
	}

	std::vector<UiRect> ScreenLayout::WidgetRects() const
	{
		std::vector<UiRect> rects;
		rects.reserve(Widgets.size());
		for (const ScreenWidget& widget : Widgets)
			rects.push_back(widget.Rect);
		return rects;
	}

	int ScreenLayout::IndexOf(const std::string& label) const
	{
		for (size_t i = 0; i < Widgets.size(); ++i)
		{
			if (Widgets[i].Label == label)
				return static_cast<int>(i);
		}
		return -1;
	}

	std::string VisibleLabel(const std::string& label)
	{
		const size_t split = label.find("##");
		return split == std::string::npos ? label : label.substr(0, split);
	}

	std::string SlotLabel(int index, const ItemStack& stack)
	{
		const std::string name = stack.Empty() ? std::string("Empty") : SplitName(ItemName(stack), SLOT_NAME_CHARS);
		return std::format("{}##slot{}", name, index);
	}

	std::string SlotCountText(const ItemStack& stack)
	{
		return stack.Count > 1 ? std::format("x{}", stack.Count) : std::string();
	}

	std::string BuildLabel(const BuildDef& def)
	{
		return std::format("{}##build{}", SplitName(def.Name, BUILD_NAME_CHARS), static_cast<int>(def.Id));
	}

	std::string BuildCaption(const BuildDef& def, const Inventory& inventory)
	{
		if (!inventory.KnowsPlan(def.Id))
			return std::format("Plan needed\n{} coins", def.PlanPrice);
		return CostLines(def.Cost);
	}

	const char* CategoryName(BuildCategory category)
	{
		switch (category)
		{
		case BuildCategory::Fence: return "Fences";
		case BuildCategory::Path: return "Paths";
		case BuildCategory::Farm: return "Farm";
		case BuildCategory::Light: return "Lights";
		case BuildCategory::Decor: return "Decor";
		case BuildCategory::Storage: return "Storage";
		case BuildCategory::Building: return "Buildings";
		}
		return "";
	}

	std::string InventoryDetail(int picked, const Inventory& inventory)
	{
		if (picked >= 0 && picked < INVENTORY_SLOTS && !inventory.GetSlot(picked).Empty())
		{
			return std::format("Holding {}: choose a slot to put it down, or the same slot to cancel.\nB: cancel   LB / RB: switch screen",
				ItemName(inventory.GetSlot(picked)));
		}
		return "A: pick up or put down a stack   1-9: equip a hotbar slot\nB: close   LB / RB: switch screen";
	}

	std::string PaletteDetail(BuildId selected, const Inventory& inventory)
	{
		const BuildDef& def = GetBuildDef(selected);
		const std::string cost = inventory.KnowsPlan(selected) ? CostInline(def.Cost) : std::format("plan {} coins", def.PlanPrice);
		return std::format("Selected: {} ({}x{}), {}\nA: choose   R / Y: rotate in the world   B: close   LB / RB: switch screen", def.Name, def.Width, def.Height, cost);
	}

	ScreenLayout BuildInventoryLayout(const Inventory& inventory, int picked)
	{
		ScreenLayout layout;
		layout.Id = ScreenId::Inventory;

		const float contentW = static_cast<float>(HOTBAR_SLOTS) * SLOT_W + static_cast<float>(HOTBAR_SLOTS - 1) * GAP;
		const float gridH = static_cast<float>(INVENTORY_ROWS) * SLOT_H + static_cast<float>(INVENTORY_ROWS - 1) * GAP;

		float y = PAD;
		AddHeader(layout, "Inventory", inventory, contentW, y);
		y += HEADER_H + GAP;

		AddTab(layout, TAB_INVENTORY_LABEL, 0, 0.0f, SLOT_W, y);
		AddTab(layout, TAB_BUILD_LABEL, 1, 0.0f, SLOT_W, y);
		y += TAB_H + GAP;

		layout.Texts.push_back({ "Hotbar", CAPTION_SIZE, { PAD, y, contentW, CAPTION_H }, false });
		y += CAPTION_H + GAP;
		for (int i = 0; i < HOTBAR_SLOTS; ++i)
			AddSlot(layout, inventory, i, PAD + static_cast<float>(i) * (SLOT_W + GAP), y);
		y += SLOT_H + GAP;

		layout.Texts.push_back({ "Backpack", CAPTION_SIZE, { PAD, y, contentW, CAPTION_H }, false });
		y += CAPTION_H + GAP;
		const float gridX = PAD + static_cast<float>(GRID_START_COLUMN) * (SLOT_W + GAP);
		for (int row = 0; row < INVENTORY_ROWS; ++row)
		{
			for (int column = 0; column < INVENTORY_COLUMNS; ++column)
				AddSlot(layout, inventory, HOTBAR_SLOTS + row * INVENTORY_COLUMNS + column, gridX + static_cast<float>(column) * (SLOT_W + GAP), y + static_cast<float>(row) * (SLOT_H + GAP));
		}
		y += gridH + GAP;

		layout.Texts.push_back({ InventoryDetail(picked, inventory), DETAIL_SIZE, { PAD, y, contentW, DETAIL_H }, true });
		y += DETAIL_H + PAD;

		layout.Panel = { 0.0f, 0.0f, contentW + 2.0f * PAD, y };
		const int selected = inventory.GetSelectedSlot();
		layout.DefaultFocus = SlotLabel(selected, inventory.GetSlot(selected));
		return layout;
	}

	ScreenLayout BuildPaletteLayout(const Inventory& inventory, BuildId selected)
	{
		ScreenLayout layout;
		layout.Id = ScreenId::BuildPalette;

		constexpr int k_Categories = static_cast<int>(BuildCategory::Building) + 1;
		int perCategory[k_Categories] = {};
		int widest = 0;
		for (const BuildDef& def : GetBuildDefs())
			widest = (std::max)(widest, ++perCategory[static_cast<int>(def.Category)]);

		const float cellsW = static_cast<float>(widest) * CELL_W + static_cast<float>(widest - 1) * GAP;
		const float contentW = LABEL_COL_W + GAP + cellsW;

		float y = PAD;
		AddHeader(layout, "Build", inventory, contentW, y);
		y += HEADER_H + GAP;

		AddTab(layout, TAB_INVENTORY_LABEL, 0, PALETTE_TAB_INDENT, CELL_W, y);
		AddTab(layout, TAB_BUILD_LABEL, 1, PALETTE_TAB_INDENT, CELL_W, y);
		AddTab(layout, TAB_TOOLS_LABEL, 2, PALETTE_TAB_INDENT, CELL_W, y);
		y += TAB_H + GAP;

		for (int category = 0; category < k_Categories; ++category)
		{
			layout.Texts.push_back({ CategoryName(static_cast<BuildCategory>(category)), CELL_SIZE, { PAD, y, LABEL_COL_W, CELL_H }, false });
			int column = 0;
			for (const BuildDef& def : GetBuildDefs())
			{
				if (static_cast<int>(def.Category) != category)
					continue;
				const float x = PAD + LABEL_COL_W + GAP + static_cast<float>(column) * (CELL_W + GAP);
				layout.Widgets.push_back({ BuildLabel(def), { x, y, CELL_W, CELL_BUTTON_H }, CELL_SIZE });
				layout.Texts.push_back({ BuildCaption(def, inventory), COST_SIZE, { x, y + CELL_BUTTON_H, CELL_W, CELL_COST_H }, false });
				++column;
			}
			y += CELL_H + GAP;
		}

		layout.Texts.push_back({ PaletteDetail(selected, inventory), DETAIL_SIZE, { PAD, y, contentW, DETAIL_H }, true });
		y += DETAIL_H + PAD;

		layout.Panel = { 0.0f, 0.0f, contentW + 2.0f * PAD, y };
		layout.DefaultFocus = BuildLabel(GetBuildDef(selected));
		return layout;
	}

	int FindNavTarget(const std::vector<UiRect>& rects, int origin, NavDir direction)
	{
		if (origin < 0 || origin >= static_cast<int>(rects.size()))
			return -1;
		const bool horizontal = direction == NavDir::Left || direction == NavDir::Right;
		const float sign = direction == NavDir::Right || direction == NavDir::Down ? 1.0f : -1.0f;
		const UiRect& from = rects[static_cast<size_t>(origin)];

		int best = -1;
		float bestScore = 0.0f;
		for (int i = 0; i < static_cast<int>(rects.size()); ++i)
		{
			if (i == origin)
				continue;
			const UiRect& rect = rects[static_cast<size_t>(i)];
			const float along = horizontal ? (rect.CenterX() - from.CenterX()) * sign : (rect.CenterY() - from.CenterY()) * sign;
			if (along <= k_MinStep)
				continue;
			const float gap = horizontal ? (std::max)(0.0f, (std::max)(rect.Y - from.Bottom(), from.Y - rect.Bottom()))
				: (std::max)(0.0f, (std::max)(rect.X - from.Right(), from.X - rect.Right()));
			const float offset = horizontal ? std::abs(rect.CenterY() - from.CenterY()) : std::abs(rect.CenterX() - from.CenterX());
			const float score = along + 2.0f * gap + 0.25f * offset;
			if (best < 0 || score < bestScore)
			{
				best = i;
				bestScore = score;
			}
		}
		return best;
	}

	int FindTabTarget(int count, int origin, bool backwards)
	{
		if (count <= 0)
			return -1;
		if (origin < 0 || origin >= count)
			return backwards ? count - 1 : 0;
		return backwards ? (origin + count - 1) % count : (origin + 1) % count;
	}

	int CountReachable(const std::vector<UiRect>& rects, int start)
	{
		if (start < 0 || start >= static_cast<int>(rects.size()))
			return 0;
		std::vector<bool> seen(rects.size(), false);
		std::vector<int> pending{ start };
		seen[static_cast<size_t>(start)] = true;
		int reached = 0;
		while (!pending.empty())
		{
			const int current = pending.back();
			pending.pop_back();
			++reached;
			const int neighbours[] = {
				FindNavTarget(rects, current, NavDir::Left), FindNavTarget(rects, current, NavDir::Right),
				FindNavTarget(rects, current, NavDir::Up), FindNavTarget(rects, current, NavDir::Down),
				FindTabTarget(static_cast<int>(rects.size()), current, false), FindTabTarget(static_cast<int>(rects.size()), current, true) };
			for (const int next : neighbours)
			{
				if (next >= 0 && !seen[static_cast<size_t>(next)])
				{
					seen[static_cast<size_t>(next)] = true;
					pending.push_back(next);
				}
			}
		}
		return reached;
	}

	HudSlot MakeHudSlot(const ItemStack& stack, bool selected)
	{
		HudSlot slot;
		slot.Selected = selected;
		slot.Color = stack.Empty() ? glm::vec4(0.2f, 0.18f, 0.16f, 0.8f) : ItemSwatch(stack.Item);
		if (!stack.Empty())
		{
			slot.Text = SplitName(GetItemDef(stack.Item).Name, SLOT_NAME_CHARS);
			if (stack.Count > 1)
				slot.Text += std::format("\nx{}", stack.Count);
		}
		return slot;
	}

	std::string HudPlaceLine(PlaceResult preview, int rotationQuarterTurns)
	{
		return std::format("{}   rotation {}   R / Y: rotate   Space / A: place   F / X: remove",
			preview == PlaceResult::Ok ? "Ready to place" : ToString(preview), rotationQuarterTurns * 90);
	}

	HudContent BuildHudContent(const Calendar& calendar, const Inventory& inventory, const BuildController& controller)
	{
		HudContent content;
		content.Clock = std::format("{} {}   {}   {}", ToString(calendar.GetSeason()), calendar.GetDayOfSeason(), calendar.GetClockText(), ToString(calendar.GetWeather()));
		content.Coins = std::format("Coins {}", inventory.GetCoins());
		content.Resources = std::format("Wood {}   Stone {}   Fibre {}", inventory.Count(ItemId::Wood), inventory.Count(ItemId::Stone), inventory.Count(ItemId::Fibre));
		content.DayProgress = (std::min)(1.0f, static_cast<float>(calendar.GetTickOfDay()) / static_cast<float>(DAY_TICKS));

		content.BuildMode = controller.GetMode() == BuildMode::Build;
		if (!content.BuildMode)
		{
			const ItemId held = controller.GetHeld();
			const CropId seed = controller.GetSeed();
			content.StripA = std::format("Holding: {}   Seed: {}", held == ItemId::Count ? "empty hands" : GetItemDef(held).Name,
				seed < CropId::Count ? std::format("{} x{}", GetItemDef(SeedItem(seed)).Name, inventory.Count(SeedItem(seed))) : std::string("none this season"));
			content.StripB = "Space / A: use   F / X: interact   B: build menu   Tab / View: inventory";
		}
		else
		{
			const BuildDef& def = GetBuildDef(controller.GetBuildable());
			const PlaceResult preview = controller.GetPreview();
			content.Affordable = CanAfford(inventory, def.Cost);
			content.StripA = std::format("{} ({}x{}): {}   B: back to tools", def.Name, def.Width, def.Height, CostInline(def.Cost));
			content.StripB = HudPlaceLine(preview, controller.GetRotation());
		}

		content.Hotbar.reserve(HOTBAR_SLOTS);
		for (int i = 0; i < HOTBAR_SLOTS; ++i)
			content.Hotbar.push_back(MakeHudSlot(inventory.GetSlot(i), i == inventory.GetSelectedSlot()));
		return content;
	}

}
