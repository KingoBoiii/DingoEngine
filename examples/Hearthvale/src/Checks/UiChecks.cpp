#include "Checks/MenuChecks.h"
#include "Checks/UiChecks.h"
#include "GameTuning.h"
#include "LaunchOptions.h"
#include "Ui/GameUi.h"
#include "Ui/ScreenHost.h"

#include <format>
#include <vector>

namespace Dingo
{

	namespace
	{
		constexpr int k_SettleFrames = 3;
		constexpr int k_MoveTarget = 20;

		bool Waited(int& frames)
		{
			if (++frames < k_SettleFrames)
				return false;
			frames = 0;
			return true;
		}

		UiMeasure MeasureWith(const Font& font)
		{
			return [&font](const std::string& text, float size, float maxWidth)
			{
				const TextMetrics metrics = font.MeasureText(text, size, maxWidth);
				return UiText{ metrics.Width, metrics.Height, static_cast<int>(metrics.LineCount) };
			};
		}

		bool LayoutFits(const ScreenLayout& layout, const UiMeasure& measure)
		{
			for (const ScreenWidget& widget : layout.Widgets)
			{
				if (!TextFitsBox(VisibleLabel(widget.Label), widget.TextSize, widget.Rect.W - 2.0f * UiMetrics::BUTTON_PAD_X, widget.Rect.H - 2.0f * UiMetrics::BUTTON_PAD_Y, false, measure))
					return false;
			}
			for (const ScreenText& text : layout.Texts)
			{
				if (!TextFitsBox(text.Text, text.Size, text.Box.W, text.Box.H, text.Wrap, measure))
					return false;
			}
			return true;
		}

		int FirstBuildableWithoutPlan(const Inventory& inventory)
		{
			for (const BuildDef& def : GetBuildDefs())
			{
				if (!inventory.KnowsPlan(def.Id))
					return static_cast<int>(def.Id);
			}
			return -1;
		}
	}

	void UiCheckRun::CheckFonts(CheckReport& report, GameUi& ui, const Inventory& inventory, BuildController& controller, const ValleyDirector& director)
	{
		const Font* font = ui.GetKit().TextFont;
		report.Check(font != nullptr, "UI: the interface font loaded");
		if (!font)
			return;
		const UiMeasure measure = MeasureWith(*font);

		Inventory full;
		for (int item = 0; item < ITEM_COUNT; ++item)
			full.Add(static_cast<ItemId>(item), 99);
		full.AddCoins(99999);
		full.LearnAllPlans();
		report.Check(LayoutFits(BuildInventoryLayout(full, -1), measure) && LayoutFits(BuildInventoryLayout(full, 3), measure) && LayoutFits(BuildInventoryLayout(inventory, -1), measure),
			"UI fonts: every inventory label, count and line of help fits its box when measured with the real font");

		bool palette = true;
		for (const BuildDef& def : GetBuildDefs())
			palette = palette && LayoutFits(BuildPaletteLayout(full, def.Id), measure) && LayoutFits(BuildPaletteLayout(inventory, def.Id), measure);
		report.Check(palette, "UI fonts: every build label, cost line and help line fits its box for every selection, plan bought or not");

		bool tiles = true;
		for (int item = 0; item < ITEM_COUNT; ++item)
		{
			const HudSlot slot = MakeHudSlot(ItemStack{ static_cast<ItemId>(item), 99 }, false);
			tiles = tiles && TextFitsBox(slot.Text, UiMetrics::HUD_SLOT_SIZE, UiMetrics::HUD_SLOT_W - 2.0f * UiMetrics::HUD_SLOT_PAD, UiMetrics::HUD_SLOT_H - 2.0f * UiMetrics::HUD_SLOT_PAD, false, measure);
		}
		report.Check(tiles, "UI fonts: every item name and a count of 99 fit a hotbar tile");

		const BuildMode mode = controller.GetMode();
		const BuildId buildable = controller.GetBuildable();
		const float sideW = UiMetrics::HUD_SIDE_W - 2.0f * UiMetrics::HUD_PAD_X;
		bool hud = true;
		for (const BuildMode probe : { BuildMode::Tools, BuildMode::Build })
		{
			controller.SetMode(probe);
			for (const BuildDef& def : GetBuildDefs())
			{
				controller.SelectBuildable(def.Id);
				const HudContent content = BuildHudContent(director.GetCalendar(), full, controller);
				hud = hud && TextFitsBox(content.Clock, UiMetrics::HUD_LINE_SIZE, sideW, 40.0f, false, measure) && TextFitsBox(content.Coins, UiMetrics::HUD_LINE_SIZE, sideW, 40.0f, false, measure)
					&& TextFitsBox(content.Resources, UiMetrics::HUD_LINE_SIZE, sideW, 40.0f, false, measure)
					&& TextFitsBox(content.StripA, UiMetrics::HUD_STRIP_SIZE, UiMetrics::HUD_STRIP_W, 40.0f, false, measure) && TextFitsBox(content.StripB, UiMetrics::HUD_STRIP_SIZE, UiMetrics::HUD_STRIP_W, 40.0f, false, measure);
			}
		}
		controller.SetMode(mode);
		controller.SelectBuildable(buildable);
		report.Check(hud, "UI fonts: the clock, coin, material and hint lines fit their panels in tool and build mode for every buildable");
	}

	void UiCheckRun::CheckMenuFonts(CheckReport& report, GameUi& ui)
	{
		const Font* font = ui.GetKit().TextFont;
		if (!font)
			return;
		const UiMeasure measure = MeasureWith(*font);
		int pages = 0;
		std::vector<std::string> failed;
		ForEachMenuPage([&](const std::string& name, const PageSpec& page)
		{
			++pages;
			if (!LayoutFits(LayoutPage(page), measure))
				failed.push_back(name);
		});
		report.Check(pages > 300 && failed.empty(), failed.empty() ? std::format("UI fonts: every label and text of all {} menu page variants fits its box when measured with the real font", pages)
			: std::format("UI fonts: {} of {} menu page variants overflow with the real font, first: {}", failed.size(), pages, failed.front()));
	}

	bool UiCheckRun::SweepMenus(CheckReport& report, ScreenHost& host, ValleyDirector& director)
	{
		struct Menu
		{
			const char* Name;
			ScreenId Id;
			bool (*Open)(ScreenHost&);
		};
		static const Menu menus[] = {
			{ "title", ScreenId::Title, [](ScreenHost& h) { h.OpenTitle(); return h.IsOpen(ScreenId::Title); } },
			{ "pause", ScreenId::Pause, [](ScreenHost& h) { h.Open(ScreenId::Pause); return h.IsOpen(ScreenId::Pause); } },
			{ "options", ScreenId::Options, [](ScreenHost& h) { h.Open(ScreenId::Options); return h.IsOpen(ScreenId::Options); } },
			{ "credits", ScreenId::Credits, [](ScreenHost& h) { h.Open(ScreenId::Credits); return h.IsOpen(ScreenId::Credits); } },
			{ "shop", ScreenId::Shop, [](ScreenHost& h) { return h.OpenShop(Merchant::Shopkeeper) && h.IsOpen(ScreenId::Shop); } },
			{ "workshop", ScreenId::Shop, [](ScreenHost& h) { return h.OpenShop(Merchant::Carpenter) && h.IsOpen(ScreenId::Shop); } },
			{ "shipping bin", ScreenId::ShippingBin, [](ScreenHost& h) { return h.OpenBin() && h.IsOpen(ScreenId::ShippingBin); } },
			{ "community board", ScreenId::Board, [](ScreenHost& h) { return h.OpenBoard() && h.IsOpen(ScreenId::Board); } },
			{ "dialogue", ScreenId::Dialogue, [](ScreenHost& h) { h.OpenVillager(VillagerId::Mayor); return h.IsOpen(ScreenId::Dialogue); } },
		};
		constexpr int count = static_cast<int>(sizeof(menus) / sizeof(menus[0]));

		if (m_MenuIndex >= count)
			return true;
		const Menu& menu = menus[m_MenuIndex];
		if (!m_MenuOpen)
		{
			const bool opened = menu.Open(host);
			report.Check(opened, std::format("UI menus: the {} screen opens on the live host", menu.Name));
			m_MenuOpen = true;
			m_Frames = 0;
			return false;
		}
		if (!Waited(m_Frames))
			return false;
		report.Check(host.Top() == menu.Id && director.IsTimePaused() && DingoUI::IsCapturingInput(),
			std::format("UI menus: the {} screen draws for several frames, holds the input and keeps the clock stopped {} {} {}", menu.Name, host.Top() == menu.Id, director.IsTimePaused(), DingoUI::IsCapturingInput()));
		host.CloseAll();
		m_MenuOpen = false;
		++m_MenuIndex;
		m_Frames = 0;
		return false;
	}

	bool UiCheckRun::Finish(CheckReport& report)
	{
		DE_INFO("UI window checks: {} passed, {} failed", report.GetPassed() - m_PassedAtStart, report.GetFailed() - m_FailedAtStart);
		m_Step = Step::Done;
		return true;
	}

	bool UiCheckRun::Tick(CheckReport& report, GameUi& ui, ScreenHost& host, Inventory& inventory, BuildController& controller, ValleyDirector& director)
	{
		switch (m_Step)
		{
		case Step::Start:
			m_PassedAtStart = report.GetPassed();
			m_FailedAtStart = report.GetFailed();
			CheckFonts(report, ui, inventory, controller, director);
			CheckMenuFonts(report, ui);
			if (!ui.IsUsable())
				return Finish(report);
			m_TickAtOpen = director.GetCalendar().GetTickOfDay();
			host.Open(ScreenId::Inventory);
			m_Step = Step::OpenInventory;
			return false;

		case Step::OpenInventory:
			if (!Waited(m_Frames))
				return false;
			report.Check(host.IsOpen(ScreenId::Inventory) && director.IsTimePaused(), "UI: opening the inventory stops the clock");
			report.Check(director.GetCalendar().GetTickOfDay() == m_TickAtOpen, "UI: the clock does not move while the inventory is open");
			report.Check(DingoUI::IsCapturingInput() && DingoUI::IsPointerOverUI(), "UI: an open screen holds the input, so the world gets no clicks");
			m_Step = Step::InventoryOpen;
			return false;

		case Step::InventoryOpen:
		{
			const ItemStack before = inventory.GetSlot(0);
			host.ActivateSlot(0);
			const bool picked = host.GetPicked() == 0;
			host.ActivateSlot(k_MoveTarget);
			const std::string request = host.TakeFocusRequest();
			report.Check(picked && host.GetPicked() < 0 && inventory.GetSlot(k_MoveTarget).Item == before.Item && inventory.GetSlot(0).Empty()
				&& request == SlotLabel(k_MoveTarget, inventory.GetSlot(k_MoveTarget)), "UI: picking up a stack and putting it on an empty slot moves it and focuses the destination");
			host.ActivateSlot(k_MoveTarget);
			host.ActivateSlot(0);
			host.TakeFocusRequest();
			report.Check(inventory.GetSlot(0).Item == before.Item && inventory.GetSlot(k_MoveTarget).Empty(), "UI: the stack goes back to where it was");
			m_Frames = 0;
			m_Step = Step::Moved;
			return false;
		}

		case Step::Moved:
			if (!Waited(m_Frames))
				return false;
			report.Check(host.IsOpen(ScreenId::Inventory) && DingoUI::IsCapturingInput(), "UI: the inventory is still up and drawing after its slots were rearranged");
			host.SwitchScreen(true);
			report.Check(host.Top() == ScreenId::BuildPalette && host.TakeFocusRequest() == TAB_BUILD_LABEL && director.IsTimePaused(),
				"UI: switching to the build palette keeps the clock stopped and asks for its tab to be focused");
			m_Step = Step::Palette;
			return false;

		case Step::Palette:
			if (Waited(m_Frames))
				m_Step = Step::PaletteOpen;
			return false;

		case Step::PaletteOpen:
		{
			report.Check(DingoUI::IsCapturingInput(), "UI: the build palette holds the input");
			const int locked = FirstBuildableWithoutPlan(inventory);
			if (locked >= 0)
			{
				const bool refused = !host.ChooseBuildable(static_cast<BuildId>(locked));
				report.Check(refused && host.IsOpen(ScreenId::BuildPalette) && controller.GetMode() == BuildMode::Tools && host.GetToast() == ToString(PlaceResult::PlanLocked),
					"UI: a buildable whose plan is not bought is refused with a message and the palette stays open");
			}
			const bool chosen = host.ChooseBuildable(BuildId::WoodFence);
			report.Check(chosen && !host.IsOpen() && controller.GetMode() == BuildMode::Build && controller.GetBuildable() == BuildId::WoodFence && !director.IsTimePaused(),
				"UI: choosing a buildable enters build mode with it, closes the palette and restarts the clock");
			m_TickAtOpen = director.GetCalendar().GetTickOfDay();
			m_Frames = 0;
			m_Step = Step::Chosen;
			return false;
		}

		case Step::Chosen:
			if (!Waited(m_Frames))
				return false;
			report.Check(!DingoUI::IsCapturingInput(), "UI: with every screen closed the input goes back to the game");
			report.Check(ui.GetToastLeft() > 0.0f && ui.GetToastText() == "Building: Wood Fence", "UI: choosing a buildable shows a toast");
			report.Check(GetLaunchOptions().Freeze || director.GetCalendar().GetTickOfDay() != m_TickAtOpen, "UI: the clock runs again after the last screen closes");
			controller.SetMode(BuildMode::Tools);
			m_Step = Step::Resumed;
			return false;

		case Step::Resumed:
			m_MenuIndex = 0;
			m_MenuOpen = false;
			m_Frames = 0;
			m_Step = Step::MenuSweep;
			return false;

		case Step::MenuSweep:
			if (!SweepMenus(report, host, director))
				return false;
			report.Check(!host.IsOpen() && !director.IsTimePaused(), "UI menus: with every menu closed again the clock runs");
			return Finish(report);

		case Step::Done:
			return true;
		}
		return true;
	}

}
