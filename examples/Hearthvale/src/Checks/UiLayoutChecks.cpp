#include "Checks/Checks.h"
#include "Ui/ScreenHost.h"
#include "Ui/ScreenLayouts.h"
#include "Ui/ScreenStack.h"
#include "Ui/UiLayout.h"
#include "ValleyDirector.h"

#include <cmath>
#include <format>
#include <set>

namespace Dingo
{

	namespace
	{
		using namespace UiMetrics;

		struct Resolution
		{
			float W;
			float H;
		};

		constexpr Resolution k_Resolutions[] = {
			{ 1280.0f, 720.0f }, { 3840.0f, 2160.0f }, { 1920.0f, 1080.0f }, { 1280.0f, 1024.0f },
			{ 3440.0f, 1440.0f }, { 1024.0f, 768.0f }, { 2560.0f, 1080.0f } };

		const UiMeasure k_Estimate = [](const std::string& text, float size, float maxWidth) { return EstimateText(text, size, maxWidth); };

		struct UiRig
		{
			ValleyDirector Director;
			Inventory Stock;
			BuildController Controller;
			ScreenHost Host;

			explicit UiRig(bool freeze = false)
				: Director(MakeConfig(freeze)), Controller(Director.GetGrid(), Stock, [](const TileRect&) {}), Host(Director, Stock, Controller)
			{
				Stock.GiveStarterLoadout();
			}

			static DirectorConfig MakeConfig(bool freeze)
			{
				DirectorConfig config;
				config.FreezeTime = freeze;
				return config;
			}

			int Tick() const { return Director.GetCalendar().GetTickOfDay(); }
		};

		bool WidgetsFit(const ScreenLayout& layout)
		{
			for (const ScreenWidget& widget : layout.Widgets)
			{
				if (!TextFitsBox(VisibleLabel(widget.Label), widget.TextSize, widget.Rect.W - 2.0f * BUTTON_PAD_X, widget.Rect.H - 2.0f * BUTTON_PAD_Y, false, k_Estimate))
					return false;
			}
			return true;
		}

		bool TextsFit(const ScreenLayout& layout)
		{
			for (const ScreenText& text : layout.Texts)
			{
				if (!TextFitsBox(text.Text, text.Size, text.Box.W, text.Box.H, text.Wrap, k_Estimate))
					return false;
			}
			return true;
		}

		bool EverythingInside(const ScreenLayout& layout)
		{
			std::vector<UiRect> rects = layout.WidgetRects();
			for (const ScreenText& text : layout.Texts)
				rects.push_back(text.Box);
			return AllInside(rects, layout.Panel);
		}

		bool NoWidgetsOverlap(const ScreenLayout& layout)
		{
			const std::vector<UiRect> rects = layout.WidgetRects();
			for (size_t a = 0; a < rects.size(); ++a)
			{
				for (size_t b = a + 1; b < rects.size(); ++b)
				{
					if (rects[a].Overlaps(rects[b]))
						return false;
				}
			}
			return true;
		}

		bool LabelsUnique(const ScreenLayout& layout)
		{
			std::set<std::string> seen;
			for (const ScreenWidget& widget : layout.Widgets)
			{
				if (!seen.insert(widget.Label).second)
					return false;
			}
			return true;
		}

		bool FitsEveryResolution(const ScreenLayout& layout)
		{
			for (const Resolution& resolution : k_Resolutions)
			{
				const glm::vec2 design = UiDesignSize(resolution.W, resolution.H);
				if (layout.Panel.W > design.x || layout.Panel.H > design.y)
					return false;
			}
			return true;
		}

		Inventory FullInventory()
		{
			Inventory inventory;
			for (int item = 0; item < ITEM_COUNT; ++item)
				inventory.Add(static_cast<ItemId>(item), 99);
			inventory.AddCoins(999999);
			return inventory;
		}

		void CheckMath(CheckReport& report)
		{
			report.Check(std::abs(UiScaleFactor(1920.0f, 1080.0f) - 1.0f) < 0.001f && std::abs(UiScaleFactor(3840.0f, 2160.0f) - 2.0f) < 0.001f
				&& std::abs(UiScaleFactor(1280.0f, 720.0f) - 2.0f / 3.0f) < 0.001f, "UiLayout: the scale factor is 1 at 1080p, 2 at 4K and two thirds at 720p");
			const glm::vec2 ultrawide = UiDesignSize(3440.0f, 1440.0f);
			report.Check(std::abs(UiDesignSize(3840.0f, 2160.0f).x - 1920.0f) < 0.01f && std::abs(ultrawide.x - 2226.0f) < 2.0f && std::abs(ultrawide.y - 932.0f) < 2.0f,
				"UiLayout: the design size of a 16:9 screen is 1920 x 1080 and of 3440 x 1440 about 2226 x 932");

			const UiText one = EstimateText("abcd", 10.0f, 0.0f);
			const UiText two = EstimateText("ab\ncd", 10.0f, 0.0f);
			const UiText wrapped = EstimateText("aaaa bbbb cccc", 10.0f, 60.0f);
			report.Check(one.Lines == 1 && std::abs(one.Width - 24.8f) < 0.01f && two.Lines == 2 && two.Height > one.Height && wrapped.Lines == 2 && EstimateText("", 10.0f, 0.0f).Lines == 0,
				"UiLayout: the text estimate counts lines for newlines and wraps at spaces");

			report.Check(SplitName("Quality Sprinkler", 14) == "Quality\nSprinkler" && SplitName("Stone Fence", 14) == "Stone Fence" && SplitName("Strawberry", 9) == "Strawberry"
				&& SplitName("Watering Can", 9) == "Watering\nCan" && SplitName("Strawberry Seeds", 9) == "Strawberry\nSeeds", "UiLayout: long names break at the space nearest the middle and single words stay whole");

			BuildCost cost;
			cost.Coins = 400;
			cost.Wood = 30;
			cost.Stone = 10;
			BuildCost wood;
			wood.Wood = 2;
			report.Check(CostLines(cost) == "400 coins\n30 wood, 10 stone" && CostInline(cost) == "400 coins, 30 wood, 10 stone" && CostLines(wood) == "2 wood" && CostLines(BuildCost{}) == "free",
				"UiLayout: a cost reads as coins then materials, and as free when it is nothing");

			std::set<int> hues;
			for (int crop = 0; crop < CROP_COUNT; ++crop)
			{
				const glm::vec4 color = ItemSwatch(HarvestItem(static_cast<CropId>(crop)));
				hues.insert(static_cast<int>(color.r * 255.0f) * 65536 + static_cast<int>(color.g * 255.0f) * 256 + static_cast<int>(color.b * 255.0f));
			}
			report.Check(static_cast<int>(hues.size()) == CROP_COUNT && ItemSwatch(ItemId::Count).a == 1.0f, "UiLayout: every crop's harvest has its own swatch colour");
		}

		void CheckStack(CheckReport& report)
		{
			ScreenStack stack;
			const bool first = stack.Open(ScreenId::Inventory, "");
			const bool duplicate = stack.Open(ScreenId::Inventory, "x");
			const bool none = stack.Open(ScreenId::None, "x");
			const bool second = stack.Open(ScreenId::BuildPalette, "Build##tab");
			report.Check(first && !duplicate && !none && second && stack.Depth() == 2 && stack.Top() == ScreenId::BuildPalette && stack.Contains(ScreenId::Inventory),
				"ScreenStack: a screen opens once, None never opens, and the last opened is the top");
			stack.Close();
			report.Check(stack.Top() == ScreenId::Inventory && stack.TakeReturnFocus() == "Build##tab" && stack.TakeReturnFocus().empty(),
				"ScreenStack: closing a screen hands back the widget that opened it, once");
			const bool replaced = stack.Replace(ScreenId::BuildPalette);
			report.Check(replaced && stack.Top() == ScreenId::BuildPalette && stack.Depth() == 1 && !stack.Replace(ScreenId::BuildPalette), "ScreenStack: replacing swaps the top and refuses a screen that is already open");
			stack.CloseAll();
			report.Check(stack.IsEmpty() && stack.Top() == ScreenId::None && !stack.Close() && stack.TakeReturnFocus().empty(), "ScreenStack: closing all empties it and closing an empty stack does nothing");
			ScreenStack deep;
			deep.Open(ScreenId::Inventory);
			deep.Open(ScreenId::BuildPalette);
			report.Check(ScreenStack::MAX_DEPTH >= 2 && deep.Depth() == 2, "ScreenStack: there is room for a second screen over the first");
		}

		void CheckHost(CheckReport& report)
		{
			{
				UiRig rig;
				PlayerIntent intent;
				intent.ToggleInventory = true;
				intent.Use = true;
				rig.Host.Update(intent, false);
				const int tick = rig.Tick();
				for (int i = 0; i < 5; ++i)
					rig.Director.Step();
				report.Check(rig.Host.Top() == ScreenId::Inventory && rig.Director.IsTimePaused() && !intent.Use && !intent.ToggleInventory && rig.Tick() == tick,
					"Screens: the inventory opens on its toggle, swallows the pulses and stops the clock");

				PlayerIntent close;
				close.ToggleInventory = true;
				rig.Host.Update(close, false);
				rig.Director.Step();
				report.Check(!rig.Host.IsOpen() && !rig.Director.IsTimePaused() && rig.Tick() == tick + 1, "Screens: the same toggle closes it and the clock runs again");
			}
			{
				UiRig rig(true);
				PlayerIntent open;
				open.ToggleInventory = true;
				rig.Host.Update(open, false);
				PlayerIntent close;
				close.ToggleInventory = true;
				rig.Host.Update(close, false);
				report.Check(!rig.Host.IsOpen() && rig.Director.IsTimePaused(), "Screens: closing a screen leaves a frozen clock frozen");
			}
			{
				UiRig rig;
				PlayerIntent blocked;
				blocked.ToggleInventory = true;
				rig.Host.Update(blocked, true);
				report.Check(!rig.Host.IsOpen(), "Screens: nothing opens during the sleep fade");
			}
			{
				UiRig rig;
				PlayerIntent build;
				build.ToggleBuild = true;
				rig.Host.Update(build, false);
				report.Check(rig.Host.Top() == ScreenId::BuildPalette && !build.ToggleBuild && rig.Controller.GetMode() == BuildMode::Tools && rig.Director.IsTimePaused(),
					"Screens: B in tool mode opens the build palette and leaves the mode alone");

				PlayerIntent toInventory;
				toInventory.HotbarNext = true;
				rig.Host.Update(toInventory, false);
				report.Check(rig.Host.Top() == ScreenId::Inventory && rig.Host.GetStack().Depth() == 1 && !toInventory.HotbarNext, "Screens: the bumpers swap the palette and the inventory");

				PlayerIntent toPalette;
				toPalette.ToggleBuild = true;
				rig.Host.Update(toPalette, false);
				PlayerIntent closePalette;
				closePalette.ToggleBuild = true;
				rig.Host.Update(closePalette, false);
				report.Check(!rig.Host.IsOpen() && !rig.Director.IsTimePaused(), "Screens: B inside the inventory goes to the palette and B inside the palette closes it");
			}
			{
				UiRig rig;
				rig.Controller.SetMode(BuildMode::Build);
				PlayerIntent build;
				build.ToggleBuild = true;
				rig.Host.Update(build, false);
				report.Check(!rig.Host.IsOpen() && build.ToggleBuild, "Screens: B in build mode is left for the controller, which returns to tools");
			}
			{
				UiRig rig;
				PlayerIntent both;
				both.ToggleInventory = true;
				both.Pause = true;
				rig.Host.Update(both, false);
				report.Check(rig.Host.Top() == ScreenId::Inventory, "Screens: a screen opened and a back press in one frame make one change");
				PlayerIntent pause;
				pause.Pause = true;
				rig.Host.Update(pause, false);
				report.Check(!rig.Host.IsOpen(), "Screens: the pause input closes the open screen");
			}
			{
				UiRig rig;
				PlayerIntent open;
				open.ToggleInventory = true;
				rig.Host.Update(open, false);
				const ItemStack hoe = rig.Stock.GetSlot(0);
				rig.Host.ActivateSlot(0);
				const int free = HOTBAR_SLOTS + 5;
				rig.Host.ActivateSlot(free);
				report.Check(rig.Stock.GetSlot(free) == hoe && rig.Stock.GetSlot(0).Empty() && rig.Host.GetPicked() < 0 && rig.Host.TakeFocusRequest() == SlotLabel(free, hoe),
					"Screens: A on a stack picks it up and A on another slot puts it there, focusing the destination");

				rig.Host.ActivateSlot(free);
				rig.Host.ActivateSlot(free);
				report.Check(rig.Host.GetPicked() < 0 && rig.Stock.GetSlot(free) == hoe, "Screens: A on the held slot again puts it back");

				rig.Host.ActivateSlot(0);
				report.Check(rig.Host.GetPicked() < 0, "Screens: A on an empty slot picks up nothing");

				rig.Host.ActivateSlot(free);
				PlayerIntent back;
				back.Pause = true;
				rig.Host.Update(back, false);
				const bool cancelled = rig.Host.IsOpen() && rig.Host.GetPicked() < 0;
				PlayerIntent again;
				again.Pause = true;
				rig.Host.Update(again, false);
				report.Check(cancelled && !rig.Host.IsOpen(), "Screens: back cancels a held stack before it closes the screen");

				rig.Stock.Add(ItemId::Wood, 10);
				rig.Stock.Add(ItemId::Wood, 99);
				PlayerIntent open2;
				open2.ToggleInventory = true;
				rig.Host.Update(open2, false);
				const int total = rig.Stock.Count(ItemId::Wood);
				int woodSlots[2] = { -1, -1 };
				int found = 0;
				for (int i = 0; i < INVENTORY_SLOTS && found < 2; ++i)
				{
					if (rig.Stock.GetSlot(i).Item == ItemId::Wood && !rig.Stock.GetSlot(i).Empty())
						woodSlots[found++] = i;
				}
				rig.Host.ActivateSlot(woodSlots[1]);
				rig.Host.ActivateSlot(woodSlots[0]);
				report.Check(found == 2 && rig.Stock.Count(ItemId::Wood) == total, "Screens: moving a stack onto the same item merges and loses nothing");
			}
			{
				UiRig rig;
				PlayerIntent open;
				open.ToggleInventory = true;
				rig.Host.Update(open, false);
				PlayerIntent equip;
				equip.HotbarSlot = 3;
				rig.Host.Update(equip, false);
				report.Check(rig.Stock.GetSelectedSlot() == 3 && !equip.Use && equip.HotbarSlot < 0, "Screens: a digit equips that hotbar slot while the inventory is open");
			}
			{
				UiRig rig;
				PlayerIntent open;
				open.ToggleBuild = true;
				rig.Host.Update(open, false);
				const uint32_t serial = rig.Host.GetToastSerial();
				const bool locked = rig.Host.ChooseBuildable(BuildId::StoneFence);
				report.Check(!locked && rig.Host.IsOpen() && rig.Host.GetToastSerial() == serial + 1 && rig.Controller.GetMode() == BuildMode::Tools && !rig.Host.GetToast().empty(),
					"Screens: a buildable whose plan is not bought is refused with a message and the palette stays open");
				const bool chosen = rig.Host.ChooseBuildable(BuildId::Gate);
				report.Check(chosen && !rig.Host.IsOpen() && rig.Controller.GetMode() == BuildMode::Build && rig.Controller.GetBuildable() == BuildId::Gate && !rig.Director.IsTimePaused(),
					"Screens: choosing a known buildable enters build mode with it, closes the palette and runs the clock");

				PlayerIntent again;
				again.ToggleInventory = true;
				rig.Host.Update(again, false);
				rig.Host.SwitchScreen(false);
				rig.Host.UseTools();
				report.Check(rig.Host.IsOpen() && rig.Controller.GetMode() == BuildMode::Build, "Screens: use tools does nothing outside the palette");
				PlayerIntent toPalette;
				toPalette.ToggleBuild = true;
				rig.Host.Update(toPalette, false);
				rig.Host.UseTools();
				report.Check(!rig.Host.IsOpen() && rig.Controller.GetMode() == BuildMode::Tools, "Screens: use tools on the palette returns to tool mode and closes it");
			}
			{
				UiRig rig;
				rig.Host.Open(ScreenId::Inventory);
				rig.Host.Open(ScreenId::BuildPalette, TAB_BUILD_LABEL);
				PlayerIntent settle;
				rig.Host.Update(settle, false);
				rig.Host.Back();
				report.Check(rig.Host.Top() == ScreenId::Inventory && rig.Host.TakeFocusRequest() == TAB_BUILD_LABEL && rig.Director.IsTimePaused(),
					"Screens: closing a screen over another returns the focus to the widget that opened it and keeps the clock stopped");
				PlayerIntent next;
				rig.Host.Update(next, false);
				rig.Host.Back();
				report.Check(!rig.Host.IsOpen() && !rig.Director.IsTimePaused() && rig.Host.TakeFocusRequest().empty(), "Screens: the last screen closing restores the clock and asks for no focus");
			}
		}

		void CheckLayouts(CheckReport& report)
		{
			Inventory empty;
			Inventory loaded = FullInventory();
			Inventory starter;
			starter.GiveStarterLoadout();

			for (const Inventory* inventory : { &empty, &starter, &loaded })
			{
				const char* name = inventory == &empty ? "an empty pack" : (inventory == &starter ? "the starter pack" : "a full pack");
				const ScreenLayout inv = BuildInventoryLayout(*inventory, -1);
				const ScreenLayout held = BuildInventoryLayout(*inventory, 0);
				const ScreenLayout palette = BuildPaletteLayout(*inventory, BuildId::WoodFence);
				report.Check(inv.Widgets.size() == 2 + INVENTORY_SLOTS && palette.Widgets.size() == 3 + BUILD_COUNT,
					std::format("Layout ({}): the inventory has 2 tabs and {} slots and the palette 3 tabs and {} buildables", name, INVENTORY_SLOTS, BUILD_COUNT));
				report.Check(EverythingInside(inv) && EverythingInside(held) && EverythingInside(palette), std::format("Layout ({}): every widget and text sits inside its panel", name));
				report.Check(NoWidgetsOverlap(inv) && NoWidgetsOverlap(palette) && LabelsUnique(inv) && LabelsUnique(palette), std::format("Layout ({}): no two widgets overlap and every label is unique", name));
				report.Check(WidgetsFit(inv) && WidgetsFit(palette), std::format("Layout ({}): every button's label fits inside the button", name));
				report.Check(TextsFit(inv) && TextsFit(held) && TextsFit(palette), std::format("Layout ({}): every text fits its box", name));
				report.Check(inv.IndexOf(inv.DefaultFocus) >= 0 && palette.IndexOf(palette.DefaultFocus) >= 0, std::format("Layout ({}): the default focus is one of the screen's widgets", name));
			}

			const ScreenLayout inv = BuildInventoryLayout(starter, -1);
			const ScreenLayout palette = BuildPaletteLayout(starter, BuildId::WoodFence);
			report.Check(FitsEveryResolution(inv) && FitsEveryResolution(palette),
				std::format("Layout: both panels fit the screen at 1280x720, 3840x2160 and five more sizes (inventory {}x{}, palette {}x{} design px)", inv.Panel.W, inv.Panel.H, palette.Panel.W, palette.Panel.H));

			bool allNames = true;
			for (int item = 0; item < ITEM_COUNT; ++item)
			{
				const ItemStack stack{ static_cast<ItemId>(item), 99 };
				allNames = allNames && TextFitsBox(VisibleLabel(SlotLabel(0, stack)), SLOT_SIZE, SLOT_W - 2.0f * BUTTON_PAD_X, SLOT_BUTTON_H - 2.0f * BUTTON_PAD_Y, false, k_Estimate)
					&& TextFitsBox(SlotCountText(stack), COUNT_SIZE, SLOT_W, SLOT_COUNT_H, false, k_Estimate);
			}
			report.Check(allNames, std::format("Layout: all {} item names and a count of 99 fit a slot", ITEM_COUNT));

			bool stable = SlotLabel(3, ItemStack{ ItemId::Wood, 5 }) == SlotLabel(3, ItemStack{ ItemId::Wood, 50 }) && SlotLabel(3, ItemStack{ ItemId::Wood, 5 }) != SlotLabel(4, ItemStack{ ItemId::Wood, 5 });
			report.Check(stable && SlotLabel(7, ItemStack{}).starts_with("Empty##"), "Layout: a slot's label ignores its count, so its widget keeps its identity as the count changes");

			Inventory known;
			known.LearnAllPlans();
			bool captions = true;
			for (const BuildDef& def : GetBuildDefs())
			{
				captions = captions && TextFitsBox(BuildCaption(def, empty), COST_SIZE, CELL_W, CELL_COST_H, false, k_Estimate)
					&& TextFitsBox(BuildCaption(def, known), COST_SIZE, CELL_W, CELL_COST_H, false, k_Estimate)
					&& TextFitsBox(VisibleLabel(BuildLabel(def)), CELL_SIZE, CELL_W - 2.0f * BUTTON_PAD_X, CELL_BUTTON_H - 2.0f * BUTTON_PAD_Y, false, k_Estimate)
					&& TextFitsBox(PaletteDetail(def.Id, empty), DETAIL_SIZE, palette.Panel.W - 2.0f * PAD, DETAIL_H, true, k_Estimate);
			}
			report.Check(captions, std::format("Layout: all {} buildables' names, locked and priced captions and detail lines fit", BUILD_COUNT));

			const ScreenLayout lockedPalette = BuildPaletteLayout(empty, BuildId::WoodFence);
			bool lockedShows = false;
			for (const ScreenText& text : lockedPalette.Texts)
				lockedShows = lockedShows || text.Text.starts_with("Plan needed");
			report.Check(lockedShows && !BuildPaletteLayout(known, BuildId::WoodFence).Texts.empty(), "Layout: a buildable whose plan is not bought says so under its button");
		}

		void CheckNavigation(CheckReport& report)
		{
			Inventory starter;
			starter.GiveStarterLoadout();
			const ScreenLayout inv = BuildInventoryLayout(starter, -1);
			const ScreenLayout palette = BuildPaletteLayout(starter, BuildId::WoodFence);
			const std::vector<UiRect> invRects = inv.WidgetRects();
			const std::vector<UiRect> paletteRects = palette.WidgetRects();

			report.Check(CountReachable(invRects, inv.IndexOf(inv.DefaultFocus)) == static_cast<int>(invRects.size()),
				"Navigation: every widget of the inventory is reachable by focus moves from its default focus");
			report.Check(CountReachable(paletteRects, palette.IndexOf(palette.DefaultFocus)) == static_cast<int>(paletteRects.size()),
				"Navigation: every widget of the build palette is reachable by focus moves from its default focus");

			const auto find = [&inv, &starter](int index) { return inv.IndexOf(SlotLabel(index, starter.GetSlot(index))); };

			bool rowsRoundTrip = true;
			for (int i = 0; i + 1 < HOTBAR_SLOTS; ++i)
			{
				const int right = FindNavTarget(invRects, find(i), NavDir::Right);
				rowsRoundTrip = rowsRoundTrip && right == find(i + 1) && FindNavTarget(invRects, right, NavDir::Left) == find(i);
			}
			for (int row = 0; row < INVENTORY_ROWS; ++row)
			{
				for (int column = 0; column + 1 < INVENTORY_COLUMNS; ++column)
				{
					const int here = find(HOTBAR_SLOTS + row * INVENTORY_COLUMNS + column);
					const int right = FindNavTarget(invRects, here, NavDir::Right);
					rowsRoundTrip = rowsRoundTrip && right == find(HOTBAR_SLOTS + row * INVENTORY_COLUMNS + column + 1) && FindNavTarget(invRects, right, NavDir::Left) == here;
				}
			}
			report.Check(rowsRoundTrip, "Navigation: right then left returns to the same slot along the hotbar and every backpack row");

			bool columnsRoundTrip = true;
			for (int row = 0; row + 1 < INVENTORY_ROWS; ++row)
			{
				for (int column = 0; column < INVENTORY_COLUMNS; ++column)
				{
					const int here = find(HOTBAR_SLOTS + row * INVENTORY_COLUMNS + column);
					const int down = FindNavTarget(invRects, here, NavDir::Down);
					columnsRoundTrip = columnsRoundTrip && down == find(HOTBAR_SLOTS + (row + 1) * INVENTORY_COLUMNS + column) && FindNavTarget(invRects, down, NavDir::Up) == here;
				}
			}
			for (int column = 0; column < INVENTORY_COLUMNS; ++column)
			{
				const int hotbar = find(GRID_START_COLUMN + column);
				const int down = FindNavTarget(invRects, hotbar, NavDir::Down);
				columnsRoundTrip = columnsRoundTrip && down == find(HOTBAR_SLOTS + column) && FindNavTarget(invRects, down, NavDir::Up) == hotbar;
			}
			report.Check(columnsRoundTrip, "Navigation: down then up returns to the same slot in every backpack column and from the hotbar into it");

			const int tabInventory = inv.IndexOf(TAB_INVENTORY_LABEL);
			const int tabBuild = inv.IndexOf(TAB_BUILD_LABEL);
			const int downFromTab = FindNavTarget(invRects, tabInventory, NavDir::Down);
			report.Check(FindNavTarget(invRects, tabInventory, NavDir::Right) == tabBuild && downFromTab == find(0) && FindNavTarget(invRects, find(0), NavDir::Up) == tabInventory,
				"Navigation: the tabs sit above the hotbar, and up from its first slot returns to the first tab");
			report.Check(FindNavTarget(invRects, find(0), NavDir::Left) < 0 && FindNavTarget(invRects, tabInventory, NavDir::Up) < 0, "Navigation: a move off the edge goes nowhere, so the focus stays put");
			report.Check(FindTabTarget(static_cast<int>(invRects.size()), static_cast<int>(invRects.size()) - 1, false) == 0 && FindTabTarget(static_cast<int>(invRects.size()), 0, true) == static_cast<int>(invRects.size()) - 1,
				"Navigation: Tab wraps around the inventory");

			bool paletteRows = true;
			int checked = 0;
			for (size_t i = 0; i + 1 < paletteRects.size(); ++i)
			{
				const UiRect& a = paletteRects[i];
				const UiRect& b = paletteRects[i + 1];
				if (palette.Widgets[i].Label.find("##build") == std::string::npos || palette.Widgets[i + 1].Label.find("##build") == std::string::npos || std::abs(a.Y - b.Y) > 0.5f)
					continue;
				const int right = FindNavTarget(paletteRects, static_cast<int>(i), NavDir::Right);
				paletteRows = paletteRows && right == static_cast<int>(i + 1) && FindNavTarget(paletteRects, right, NavDir::Left) == static_cast<int>(i);
				++checked;
			}
			report.Check(paletteRows && checked > 0, std::format("Navigation: right then left returns to the same buildable along every palette row ({} pairs)", checked));

			bool paletteColumns = true;
			for (size_t i = 0; i < paletteRects.size(); ++i)
			{
				if (palette.Widgets[i].Label.find("##build") == std::string::npos)
					continue;
				const int down = FindNavTarget(paletteRects, static_cast<int>(i), NavDir::Down);
				const bool lastRow = paletteRects[i].Y > palette.Panel.H - CELL_H - DETAIL_H - 2.0f * PAD;
				if (lastRow)
					paletteColumns = paletteColumns && down < 0;
				else
					paletteColumns = paletteColumns && down >= 0 && paletteRects[static_cast<size_t>(down)].Y > paletteRects[i].Y;
			}
			report.Check(paletteColumns, "Navigation: down from a buildable reaches the next category's row, and nothing lies below the last one");
		}

		void CheckHud(CheckReport& report)
		{
			Calendar calendar(1);
			Inventory starter;
			starter.GiveStarterLoadout();
			starter.AddCoins(999999);
			ValleyDirector director{ DirectorConfig{} };
			BuildController controller(director.GetGrid(), starter, [](const TileRect&) {});

			bool texts = true;
			const float sideInner = HUD_SIDE_W - 2.0f * HUD_PAD_X;
			for (int day = 0; day < DAYS_PER_YEAR; day += 7)
			{
				for (const Weather weather : { Weather::Clear, Weather::Rain, Weather::Snow })
				{
					calendar.Reset(day, DAY_TICKS - 1);
					calendar.ForceWeather(weather);
					const HudContent content = BuildHudContent(calendar, starter, controller);
					texts = texts && TextFitsBox(content.Clock, HUD_LINE_SIZE, sideInner, 40.0f, false, k_Estimate) && TextFitsBox(content.Coins, HUD_LINE_SIZE, sideInner, 40.0f, false, k_Estimate)
						&& TextFitsBox(content.Resources, HUD_LINE_SIZE, sideInner, 40.0f, false, k_Estimate) && content.DayProgress >= 0.0f && content.DayProgress <= 1.0f;
				}
			}
			report.Check(texts, "Hud: the clock, coins and materials lines fit their panels in every season and weather");

			bool strips = true;
			const HudContent tools = BuildHudContent(calendar, starter, controller);
			strips = strips && !tools.BuildMode && TextFitsBox(tools.StripA, HUD_STRIP_SIZE, HUD_STRIP_W, 40.0f, false, k_Estimate) && TextFitsBox(tools.StripB, HUD_STRIP_SIZE, HUD_STRIP_W, 40.0f, false, k_Estimate);
			controller.SetMode(BuildMode::Build);
			for (const BuildDef& def : GetBuildDefs())
			{
				controller.SelectBuildable(def.Id);
				const HudContent build = BuildHudContent(calendar, starter, controller);
				strips = strips && build.BuildMode && TextFitsBox(build.StripA, HUD_STRIP_SIZE, HUD_STRIP_W, 40.0f, false, k_Estimate);
			}
			for (const PlaceResult result : { PlaceResult::Ok, PlaceResult::OutOfBounds, PlaceResult::BadGround, PlaceResult::Occupied, PlaceResult::PlanLocked, PlaceResult::CannotAfford })
				strips = strips && TextFitsBox(HudPlaceLine(result, 3), HUD_STRIP_SIZE, HUD_STRIP_W, 40.0f, false, k_Estimate);
			report.Check(strips, "Hud: the tool and build strips fit their width for every buildable and every placement result");

			bool slots = true;
			for (int item = 0; item < ITEM_COUNT; ++item)
			{
				const HudSlot slot = MakeHudSlot(ItemStack{ static_cast<ItemId>(item), 99 }, false);
				slots = slots && TextFitsBox(slot.Text, HUD_SLOT_SIZE, HUD_SLOT_W - 2.0f * HUD_SLOT_PAD, HUD_SLOT_H - 2.0f * HUD_SLOT_PAD, false, k_Estimate);
			}
			const HudContent content = BuildHudContent(calendar, starter, controller);
			int selected = 0;
			for (const HudSlot& slot : content.Hotbar)
				selected += slot.Selected ? 1 : 0;
			report.Check(slots && static_cast<int>(content.Hotbar.size()) == HOTBAR_SLOTS && selected == 1, "Hud: every item fits a hotbar tile and exactly the selected slot is marked");

			const float hotbarW = static_cast<float>(HOTBAR_SLOTS) * HUD_SLOT_W + static_cast<float>(HOTBAR_SLOTS - 1) * HUD_SLOT_GAP;
			bool placed = true;
			for (const Resolution& resolution : k_Resolutions)
			{
				const glm::vec2 design = UiDesignSize(resolution.W, resolution.H);
				placed = placed && HUD_STRIP_W + 2.0f * HUD_PAD_X <= design.x && hotbarW <= HUD_STRIP_W
					&& 2.0f * (HUD_MARGIN + HUD_SIDE_W) < design.x;
			}
			report.Check(placed, "Hud: the corner panels and the hotbar fit side by side at every test resolution");
		}
	}

	void RunUiLayoutChecks(CheckReport& report)
	{
		SuiteScope scope(report, "UI layout");
		CheckMath(report);
		CheckStack(report);
		CheckHost(report);
		CheckLayouts(report);
		CheckNavigation(report);
		CheckHud(report);
	}

}
