#include "Ui/MenuPages.h"
#include "Defs/CreditDefs.h"

#include <algorithm>
#include <format>

namespace Dingo
{

	using namespace UiMetrics;
	using namespace PageMetrics;
	using namespace MenuMetrics;

	namespace
	{
		struct Slice
		{
			int Page = 0;
			int Count = 1;
			int Begin = 0;
			int End = 0;
		};

		Slice SliceOf(int total, int perPage, int page)
		{
			Slice slice;
			slice.Count = (std::max)(1, (total + perPage - 1) / perPage);
			slice.Page = (std::clamp)(page, 0, slice.Count - 1);
			slice.Begin = slice.Page * perPage;
			slice.End = (std::min)(total, slice.Begin + perPage);
			return slice;
		}

		PageRow StatusRow(const MenuView& view, float width)
		{
			return MakeRow({ TextCell(view.Status, width, STATUS_H, NOTE_SIZE, PageTone::Normal, true) });
		}

		PageRow MenuButtonRow(const char* label, PageAction action, int arg = 0, bool enabled = true)
		{
			return MakeRow({ ButtonCell(label, MENU_W, MENU_H, MENU_SIZE, action, arg, enabled) });
		}

		PageRow BackRow(const std::string& label, float width = NAV_W)
		{
			return MakeRow({ ButtonCell(label, width, NAV_H, ROW_SIZE, PageAction::Back) });
		}

		PageRow PagedNavRow(const std::string& suffix, bool pagesEnabled)
		{
			return MakeRow({
				ButtonCell("Prev##" + suffix, NAV_W, NAV_H, ROW_SIZE, PageAction::PrevPage, 0, pagesEnabled),
				ButtonCell("Next##" + suffix, NAV_W, NAV_H, ROW_SIZE, PageAction::NextPage, 0, pagesEnabled),
				ButtonCell("Back##" + suffix, NAV_W, NAV_H, ROW_SIZE, PageAction::Back) });
		}

		PageCell TabCell(const std::string& label, int index, int current)
		{
			PageCell cell = ButtonCell(label, NAV_W, TAB_H, TAB_SIZE, PageAction::Tab, index);
			cell.Highlight = index == current;
			return cell;
		}

		std::string Coins(int amount)
		{
			return std::format("{} coins", amount);
		}

		std::string FirstEnabledButton(const PageSpec& page, PageAction action)
		{
			for (const PageRow& row : page.Rows)
			{
				for (const PageCell& cell : row.Cells)
				{
					if (cell.Kind != CellKind::Text && cell.Enabled && cell.Action == action)
						return cell.Label;
				}
			}
			return {};
		}

		PageSpec BuildTitle(const MenuView& view)
		{
			PageSpec page;
			page.Id = ScreenId::Title;
			page.ContentW = MENU_W;
			page.Rows.push_back(MakeRow({ TextCell("Hearthvale", MENU_W, BIG_TITLE_H, BIG_TITLE_SIZE, PageTone::Pick) }));
			page.Rows.push_back(MakeRow({ TextCell("A valley to rebuild", MENU_W, CAPTION_H, NOTE_SIZE, PageTone::Dim) }));
			const bool canContinue = AnySlotOccupied(view.Slots);
			page.Rows.push_back(MenuButtonRow("New game##title", PageAction::TitleNew));
			page.Rows.push_back(MenuButtonRow("Continue##title", PageAction::TitleContinue, 0, canContinue));
			page.Rows.push_back(MenuButtonRow("Options##title", PageAction::OpenOptions));
			page.Rows.push_back(MenuButtonRow("Credits##title", PageAction::OpenCredits));
			page.Rows.push_back(MenuButtonRow("Quit##title", PageAction::QuitGame));
			page.DefaultFocus = canContinue ? "Continue##title" : "New game##title";
			return page;
		}

		PageSpec BuildSlots(const MenuView& view)
		{
			PageSpec page;
			page.Id = ScreenId::Slots;
			page.ContentW = SLOTS_W;
			switch (view.SlotsMode)
			{
			case SlotMode::NewGame: page.Title = "New game"; page.Header = "Pick a slot to start in"; break;
			case SlotMode::Load: page.Title = "Load game"; page.Header = "Pick a slot to load"; break;
			case SlotMode::Save: page.Title = "Save game"; page.Header = "Pick a slot to save to"; break;
			}

			for (int slot = 0; slot < SlotProvider::SLOT_COUNT; ++slot)
			{
				const SlotInfo info = static_cast<size_t>(slot) < view.Slots.size() ? view.Slots[static_cast<size_t>(slot)] : SlotInfo{};
				const bool enabled = view.SlotsMode != SlotMode::Load || info.Occupied;
				const bool confirming = view.SlotConfirm == slot;
				const std::string name = confirming ? "Overwrite?" : std::format("Slot {}", slot + 1);
				PageCell button = ButtonCell(std::format("{}##slot{}", name, slot), SAVE_SLOT_W, SAVE_SLOT_H, 18.0f, PageAction::SlotPick, slot, enabled);
				button.Tone = confirming ? PageTone::Bad : PageTone::Normal;
				const std::string summary = info.Occupied ? info.Summary : std::string("Empty");
				page.Rows.push_back(MakeRow({ button,
					TextCell(summary, SLOTS_W - SAVE_SLOT_W - GAP, SAVE_SLOT_H, NOTE_SIZE, info.Occupied ? PageTone::Normal : PageTone::Dim, true) }));
			}
			page.Rows.push_back(StatusRow(view, SLOTS_W));
			page.Rows.push_back(BackRow("Back##slots"));
			const std::string first = FirstEnabledButton(page, PageAction::SlotPick);
			page.DefaultFocus = first.empty() ? "Back##slots" : first;
			return page;
		}

		PageSpec BuildPause()
		{
			PageSpec page;
			page.Id = ScreenId::Pause;
			page.Title = "Paused";
			page.ContentW = MENU_W;
			page.Rows.push_back(MenuButtonRow("Resume##pause", PageAction::PauseResume));
			page.Rows.push_back(MenuButtonRow("Save game##pause", PageAction::PauseSave));
			page.Rows.push_back(MenuButtonRow("Options##pause", PageAction::OpenOptions));
			page.Rows.push_back(MenuButtonRow("Quit game##pause", PageAction::QuitGame));
			page.DefaultFocus = "Resume##pause";
			return page;
		}

		PageRow VolumeRow(const char* name, const char* id, float value, int arg)
		{
			return MakeRow({ TextCell(name, OPTION_LABEL_W, ROW_H, ROW_SIZE),
				SliderCell(std::format("##opt_{}", id), OPTION_CONTROL_W, ROW_H, value, 0.0f, 1.0f, OPTION_VOLUME_STEP, PageAction::OptionSlider, arg),
				TextCell(VolumeText(value), VALUE_W, ROW_H, ROW_SIZE) });
		}

		PageRow ToggleRow(const char* name, const std::string& label, PageAction action, bool enabled = true)
		{
			return MakeRow({ TextCell(name, OPTION_LABEL_W, ROW_H, ROW_SIZE), ButtonCell(label, OPTION_CONTROL_W, ROW_H, ROW_SIZE, action, 0, enabled) });
		}

		PageSpec BuildOptions(const MenuView& view)
		{
			const OptionsState& o = view.Options;
			PageSpec page;
			page.Id = ScreenId::Options;
			page.Title = "Options";
			page.ContentW = OPTIONS_W;
			page.Rows.push_back(VolumeRow("Master volume", "master", o.Master, 0));
			page.Rows.push_back(VolumeRow("Music", "music", o.Music, 1));
			page.Rows.push_back(VolumeRow("Ambience", "ambience", o.Ambience, 2));
			page.Rows.push_back(VolumeRow("Effects", "sfx", o.Sfx, 3));
			page.Rows.push_back(VolumeRow("Interface", "ui", o.Ui, 4));
			page.Rows.push_back(ToggleRow("VSync", std::string(o.VSync ? "On" : "Off") + "##opt_vsync", PageAction::OptionVSync));
			page.Rows.push_back(ToggleRow("Fullscreen", std::string(o.Fullscreen ? "On" : "Off") + "##opt_fullscreen", PageAction::OptionFullscreen));
			page.Rows.push_back(ToggleRow("Display mode", DisplayModeText(o) + "##opt_kind", PageAction::OptionFullscreenKind, o.Fullscreen));
			page.Rows.push_back(MakeRow({ TextCell("Camera speed", OPTION_LABEL_W, ROW_H, ROW_SIZE),
				SliderCell("##opt_camera", OPTION_CONTROL_W, ROW_H, o.CameraSpeed, OPTION_CAMERA_MIN, OPTION_CAMERA_MAX, OPTION_CAMERA_STEP, PageAction::OptionSlider, 5),
				TextCell(CameraSpeedText(o.CameraSpeed), VALUE_W, ROW_H, ROW_SIZE) }));
			page.Rows.push_back(ToggleRow("Hold to place", std::string(o.HoldToPlace ? "On" : "Off") + "##opt_hold", PageAction::OptionHoldToPlace));
			page.Rows.push_back(BackRow("Back##options", 160.0f));
			page.DefaultFocus = "##opt_master";
			return page;
		}

		PageSpec BuildCredits(const MenuView& view)
		{
			const std::span<const CreditEntry> credits = GetCredits();
			const Slice slice = SliceOf(static_cast<int>(credits.size()), CREDIT_ROWS, view.Page);
			PageSpec page;
			page.Id = ScreenId::Credits;
			page.Title = "Credits";
			page.Header = std::format("Page {} of {}", slice.Page + 1, slice.Count);
			page.ContentW = CREDITS_W;
			page.PageIndex = slice.Page;
			page.PageCount = slice.Count;
			page.Rows.push_back(MakeRow({ TextCell("Pack", 210.0f, CAPTION_H, NOTE_SIZE, PageTone::Dim), TextCell("Author", 220.0f, CAPTION_H, NOTE_SIZE, PageTone::Dim),
				TextCell("Licence", 80.0f, CAPTION_H, NOTE_SIZE, PageTone::Dim), TextCell("Source", 230.0f, CAPTION_H, NOTE_SIZE, PageTone::Dim) }));
			for (int i = slice.Begin; i < slice.End; ++i)
			{
				const CreditEntry& entry = credits[static_cast<size_t>(i)];
				page.Rows.push_back(MakeRow({ TextCell(entry.Pack, 210.0f, ROW_H, ROW_SIZE, PageTone::Normal, true), TextCell(entry.Author, 220.0f, ROW_H, NOTE_SIZE, PageTone::Normal, true),
					TextCell(entry.Licence, 80.0f, ROW_H, NOTE_SIZE, PageTone::Normal, true), TextCell(entry.Url, 230.0f, ROW_H, NOTE_SIZE, PageTone::Normal, true) }));
			}
			page.Rows.push_back(PagedNavRow("credits", slice.Count > 1));
			page.DefaultFocus = slice.Count > 1 ? "Next##credits" : "Back##credits";
			return page;
		}

		PageRow ItemRow(const std::string& label, int arg, bool enabled, const std::string& price, PageTone priceTone, const std::string& have)
		{
			return MakeRow({ ButtonCell(label, ITEM_NAME_W, ITEM_H, ROW_SIZE, PageAction::Row, arg, enabled),
				TextCell(price, ITEM_PRICE_W, ITEM_H, NOTE_SIZE, priceTone), TextCell(have, ITEM_HAVE_W, ITEM_H, NOTE_SIZE, PageTone::Dim) });
		}

		PageRow EmptyListRow(const char* text)
		{
			return MakeRow({ TextCell(text, LIST_W, ITEM_H, ROW_SIZE, PageTone::Dim) });
		}

		void AddListRows(PageSpec& page, std::vector<PageRow>& rows, const char* emptyText, int pageWanted)
		{
			const Slice slice = SliceOf(static_cast<int>(rows.size()), LIST_ROWS, pageWanted);
			page.PageIndex = slice.Page;
			page.PageCount = slice.Count;
			if (rows.empty())
				page.Rows.push_back(EmptyListRow(emptyText));
			for (int i = slice.Begin; i < slice.End; ++i)
				page.Rows.push_back(std::move(rows[static_cast<size_t>(i)]));
		}

		PageSpec BuildShop(const MenuView& view)
		{
			const bool shopkeeper = view.Seller == Merchant::Shopkeeper;
			const Inventory& pack = *view.Pack;
			const int tab = shopkeeper ? view.Tab : 0;
			const int quantity = QUANTITIES[(std::clamp)(view.Quantity, 0, QUANTITY_COUNT - 1)];

			PageSpec page;
			page.Id = ScreenId::Shop;
			page.Title = std::format("{}'s {}", GetVillagerDef(shopkeeper ? VillagerId::Shopkeeper : VillagerId::Carpenter).Name, shopkeeper ? "shop" : "workshop");
			page.ContentW = LIST_W;

			std::vector<PageCell> tabs;
			tabs.push_back(TabCell("Buy##shoptab", 0, tab));
			if (shopkeeper)
			{
				tabs.push_back(TabCell("Sell##shoptab", 1, tab));
				tabs.push_back(ButtonCell(std::format("Amount x{}##shopqty", quantity), 150.0f, TAB_H, TAB_SIZE, PageAction::CycleQuantity));
			}
			page.Rows.push_back(MakeRow(tabs));

			std::vector<PageRow> rows;
			if (tab == 0)
			{
				const std::vector<ShopOffer> offers = ShopStock(view.Seller);
				for (size_t i = 0; i < offers.size(); ++i)
				{
					const ShopOffer& offer = offers[i];
					const PageTone tone = pack.GetCoins() < offer.Price ? PageTone::Bad : PageTone::Normal;
					if (offer.Kind == OfferKind::Item)
					{
						const ItemDef& def = GetItemDef(offer.Item);
						rows.push_back(ItemRow(std::format("{}##buy{}", def.Name, i), static_cast<int>(i), true, Coins(offer.Price), tone, std::format("have {}", pack.Count(offer.Item))));
					}
					else
					{
						const bool known = pack.KnowsPlan(offer.Build);
						rows.push_back(ItemRow(std::format("Plan: {}##buy{}", GetBuildDef(offer.Build).Name, i), static_cast<int>(i), !known, Coins(offer.Price), tone, known ? "owned" : "not owned"));
					}
				}
			}
			else
			{
				for (const ItemId item : SellableItems(pack))
				{
					rows.push_back(ItemRow(std::format("{}##sell{}", GetItemDef(item).Name, static_cast<int>(item)), static_cast<int>(item), true,
						Coins(ShopSellBackPrice(item)), PageTone::Normal, std::format("have {}", pack.Count(item))));
				}
			}
			AddListRows(page, rows, tab == 0 ? "Nothing for sale." : "Nothing here the shop will buy.", view.Page);

			page.Header = std::format("{} coins   Page {}/{}", pack.GetCoins(), page.PageIndex + 1, page.PageCount);
			page.Rows.push_back(StatusRow(view, LIST_W));
			page.Rows.push_back(PagedNavRow("shop", page.PageCount > 1));
			const std::string first = FirstEnabledButton(page, PageAction::Row);
			page.DefaultFocus = first.empty() ? "Buy##shoptab" : first;
			return page;
		}

		PageSpec BuildBin(const MenuView& view)
		{
			const Inventory& pack = *view.Pack;
			const ShippingBin& bin = *view.Bin;
			const int quantity = QUANTITIES[(std::clamp)(view.Quantity, 0, QUANTITY_COUNT - 1)];

			PageSpec page;
			page.Id = ScreenId::ShippingBin;
			page.Title = "Shipping bin";
			page.ContentW = LIST_W;

			page.Rows.push_back(MakeRow({ TabCell("Ship##bintab", 0, view.Tab), TabCell("Take back##bintab", 1, view.Tab),
				ButtonCell(std::format("Amount x{}##binqty", quantity), 150.0f, TAB_H, TAB_SIZE, PageAction::CycleQuantity) }));

			std::vector<PageRow> rows;
			if (view.Tab == 0)
			{
				for (const ItemId item : ShippableItems(pack))
				{
					rows.push_back(ItemRow(std::format("{}##ship{}", GetItemDef(item).Name, static_cast<int>(item)), static_cast<int>(item), true,
						Coins(GetItemDef(item).SellPrice), PageTone::Normal, std::format("have {}", pack.Count(item))));
				}
			}
			else
			{
				for (const ItemId item : BinItems(bin))
				{
					rows.push_back(ItemRow(std::format("{}##take{}", GetItemDef(item).Name, static_cast<int>(item)), static_cast<int>(item), true,
						Coins(GetItemDef(item).SellPrice * bin.Held(item)), PageTone::Normal, std::format("in bin {}", bin.Held(item))));
				}
			}
			AddListRows(page, rows, view.Tab == 0 ? "Nothing in your pack can be shipped." : "The bin is empty.", view.Page);

			page.Header = std::format("In bin {} coins   Page {}/{}", bin.PendingValue(), page.PageIndex + 1, page.PageCount);
			page.Rows.push_back(MakeRow({ TextCell("Goods are paid for when the day rolls over.", LIST_W, CAPTION_H, NOTE_SIZE, PageTone::Dim) }));
			page.Rows.push_back(StatusRow(view, LIST_W));
			page.Rows.push_back(PagedNavRow("bin", page.PageCount > 1));
			const std::string first = FirstEnabledButton(page, PageAction::Row);
			page.DefaultFocus = first.empty() ? (view.Tab == 0 ? "Ship##bintab" : "Take back##bintab") : first;
			return page;
		}

		std::string FestivalNote(const CommunityBoard& board)
		{
			switch (board.GetFestival())
			{
			case FestivalPhase::NotYet: return "Ship goods in the bin, build and give gifts here to fill the boards.";
			case FestivalPhase::Pending: return "Every board is done. The mayor has something to say.";
			case FestivalPhase::Held: return std::format("The festival was held. Requests are tier {} now.", board.GetTier());
			}
			return {};
		}

		PageSpec BuildBoard(const MenuView& view)
		{
			const CommunityBoard& board = *view.Board;
			const Inventory& pack = *view.Pack;
			const int openCount = OpenBoardCount(board, view.Day);
			int index = (std::clamp)(view.BoardIndex, 0, CommunityBoard::BoardCount - 1);
			if (!board.IsOpen(index, view.Day))
				index = (std::max)(0, NthOpenBoard(board, view.Day, 0));

			PageSpec page;
			page.Id = ScreenId::Board;
			page.Title = "Community board";
			page.Header = std::format("{} board ({} of {} open)", ToString(static_cast<Season>(index)), OpenBoardOrdinal(board, view.Day, index) + 1, openCount);
			page.ContentW = BOARD_W;
			page.PageIndex = index;
			page.PageCount = CommunityBoard::BoardCount;

			const BoardDef& def = GetBoardDef(static_cast<Season>(index));
			for (int i = 0; i < BOARD_REQUESTS; ++i)
			{
				const BoardRequest& request = def.Requests[static_cast<size_t>(i)];
				const RequestState& state = board.GetRequest(index, i);
				const PageTone tone = state.Done ? PageTone::Dim : PageTone::Normal;
				const int target = board.TargetCount(index, i);
				std::vector<PageCell> cells;
				cells.push_back(TextCell(request.Text, BOARD_TEXT_W, BOARD_ROW_H, ROW_SIZE, tone, true));
				cells.push_back(TextCell(std::format("{} / {}", (std::min)(state.Progress, target), target), 110.0f, BOARD_ROW_H, NOTE_SIZE, tone));
				cells.push_back(TextCell(state.Done ? std::string("Done") : std::format("+{} coins", board.RewardFor(index, i)), 120.0f, BOARD_ROW_H, NOTE_SIZE, state.Done ? PageTone::Pick : tone));
				if (request.Kind == RequestKind::Gift)
				{
					const bool canGive = !state.Done && state.Progress < target && pack.Count(request.Item) > 0;
					cells.push_back(ButtonCell(std::format("Give##board{}", i), 120.0f, ITEM_H, ROW_SIZE, PageAction::BoardGive, i, canGive));
				}
				page.Rows.push_back(MakeRow(std::move(cells)));
			}

			page.Rows.push_back(MakeRow({ TextCell(FestivalNote(board), BOARD_W, CAPTION_H, NOTE_SIZE, PageTone::Dim) }));
			page.Rows.push_back(StatusRow(view, BOARD_W));

			std::vector<PageCell> nav;
			nav.push_back(ButtonCell("Prev board##board", NAV_W + 20.0f, NAV_H, ROW_SIZE, PageAction::PrevPage, 0, openCount > 1));
			nav.push_back(ButtonCell("Next board##board", NAV_W + 20.0f, NAV_H, ROW_SIZE, PageAction::NextPage, 0, openCount > 1));
			if (board.GetFestival() == FestivalPhase::Pending)
				nav.push_back(ButtonCell("Hear the mayor##board", 200.0f, NAV_H, ROW_SIZE, PageAction::FestivalStart));
			nav.push_back(ButtonCell("Back##board", NAV_W, NAV_H, ROW_SIZE, PageAction::Back));
			page.Rows.push_back(MakeRow(std::move(nav)));

			if (board.GetFestival() == FestivalPhase::Pending)
				page.DefaultFocus = "Hear the mayor##board";
			else
			{
				const std::string give = FirstEnabledButton(page, PageAction::BoardGive);
				page.DefaultFocus = give.empty() ? "Back##board" : give;
			}
			return page;
		}

		PageSpec BuildDialogue(const MenuView& view)
		{
			const std::vector<std::string> pages = DialoguePages(view.Dialogue);
			const Slice slice = SliceOf(static_cast<int>(pages.size()), 1, view.DialoguePage);
			PageSpec page;
			page.Id = ScreenId::Dialogue;
			page.Title = view.Dialogue.Speaker;
			page.Header = pages.size() > 1 ? std::format("{}/{}", slice.Page + 1, slice.Count) : std::string();
			page.ContentW = DIALOGUE_W;
			page.PageIndex = slice.Page;
			page.PageCount = slice.Count;
			page.Rows.push_back(MakeRow({ TextCell(pages[static_cast<size_t>(slice.Page)], DIALOGUE_W, DIALOGUE_H, DIALOGUE_SIZE, PageTone::Normal, true) }));
			const bool last = slice.Page + 1 >= slice.Count;
			std::vector<PageCell> buttons;
			if (last && view.Dialogue.GiftTo >= 0)
			{
				const ItemStack held = view.Pack ? view.Pack->GetSelected() : ItemStack{};
				const bool giftable = !held.Empty() && GetItemDef(held.Item).Kind != ItemKind::Tool;
				const std::string label = giftable ? std::format("Give {}", GetItemDef(held.Item).Name) : std::string("Give gift");
				buttons.push_back(ButtonCell(label + "##gift", 280.0f, NAV_H, ROW_SIZE, PageAction::DialogueGift, view.Dialogue.GiftTo, giftable));
			}
			buttons.push_back(ButtonCell(std::string(last ? "Close" : "Next") + "##dialogue", 160.0f, NAV_H, ROW_SIZE, PageAction::DialogueNext));
			page.Rows.push_back(MakeRow(std::move(buttons)));
			page.DefaultFocus = std::string(last ? "Close" : "Next") + "##dialogue";
			return page;
		}
	}

	DialogueScript PlaceholderVillagerLine(VillagerId villager)
	{
		const VillagerDef& def = GetVillagerDef(villager);
		DialogueScript script;
		script.Speaker = def.Name;
		switch (villager)
		{
		case VillagerId::Shopkeeper: script.Text = "Seeds and timber are on the shelves whenever you need them."; break;
		case VillagerId::Carpenter: script.Text = "Bring me coins and I will sell you the plans for something sturdy."; break;
		case VillagerId::Mayor: script.Text = "The valley is coming back to life, one board at a time."; break;
		case VillagerId::Child: script.Text = "Do you have anything shiny? I like strawberries best."; break;
		case VillagerId::Count: break;
		}
		return script;
	}

	DialogueScript FestivalScript()
	{
		DialogueScript script;
		script.Speaker = GetVillagerDef(VillagerId::Mayor).Name;
		script.Text = "Neighbours, look around you. Every request on the board is done, and the valley has never looked so alive. "
			"Tonight we light the lanterns, set out the tables and celebrate together. Thank you for everything you have built here. "
			"The boards will fill again with bigger requests, if you are ready for them.";
		script.End = DialogueEnd::AcknowledgeFestival;
		return script;
	}

	std::vector<std::string> DialoguePages(const DialogueScript& script)
	{
		const UiMeasure measure = [](const std::string& text, float size, float maxWidth) { return EstimateText(text, size, maxWidth); };
		return PaginateText(script.Text, DIALOGUE_SIZE, DIALOGUE_W, DIALOGUE_LINES, measure);
	}

	bool AnySlotOccupied(const std::vector<SlotInfo>& slots)
	{
		return std::any_of(slots.begin(), slots.end(), [](const SlotInfo& slot) { return slot.Occupied; });
	}

	int OpenBoardCount(const CommunityBoard& board, int day)
	{
		int count = 0;
		for (int i = 0; i < CommunityBoard::BoardCount; ++i)
			count += board.IsOpen(i, day) ? 1 : 0;
		return count;
	}

	int NthOpenBoard(const CommunityBoard& board, int day, int n)
	{
		int seen = 0;
		for (int i = 0; i < CommunityBoard::BoardCount; ++i)
		{
			if (board.IsOpen(i, day) && seen++ == n)
				return i;
		}
		return -1;
	}

	int OpenBoardOrdinal(const CommunityBoard& board, int day, int boardIndex)
	{
		int ordinal = 0;
		for (int i = 0; i < boardIndex; ++i)
			ordinal += board.IsOpen(i, day) ? 1 : 0;
		return ordinal;
	}

	std::vector<ItemId> SellableItems(const Inventory& pack)
	{
		std::vector<ItemId> items;
		for (int i = 0; i < ITEM_COUNT; ++i)
		{
			const ItemId item = static_cast<ItemId>(i);
			if (ShopSellBackPrice(item) > 0 && pack.Count(item) > 0)
				items.push_back(item);
		}
		return items;
	}

	std::vector<ItemId> ShippableItems(const Inventory& pack)
	{
		std::vector<ItemId> items;
		for (int i = 0; i < ITEM_COUNT; ++i)
		{
			const ItemId item = static_cast<ItemId>(i);
			if (GetItemDef(item).SellPrice > 0 && pack.Count(item) > 0)
				items.push_back(item);
		}
		return items;
	}

	std::vector<ItemId> BinItems(const ShippingBin& bin)
	{
		std::vector<ItemId> items;
		for (int i = 0; i < ITEM_COUNT; ++i)
		{
			if (bin.Held(static_cast<ItemId>(i)) > 0)
				items.push_back(static_cast<ItemId>(i));
		}
		return items;
	}

	PageSpec BuildMenuPage(ScreenId id, const MenuView& view)
	{
		switch (id)
		{
		case ScreenId::Title: return BuildTitle(view);
		case ScreenId::Slots: return BuildSlots(view);
		case ScreenId::Pause: return BuildPause();
		case ScreenId::Options: return BuildOptions(view);
		case ScreenId::Credits: return BuildCredits(view);
		case ScreenId::Shop: return BuildShop(view);
		case ScreenId::ShippingBin: return BuildBin(view);
		case ScreenId::Board: return BuildBoard(view);
		case ScreenId::Dialogue: return BuildDialogue(view);
		case ScreenId::None:
		case ScreenId::Inventory:
		case ScreenId::BuildPalette:
			break;
		}
		return {};
	}

}
