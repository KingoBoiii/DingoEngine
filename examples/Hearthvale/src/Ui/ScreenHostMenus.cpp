#include "Ui/ScreenHost.h"

#include <algorithm>
#include <format>

namespace Dingo
{

	namespace
	{
		const ShippingBin k_NoBin;
		const CommunityBoard k_NoBoard;

		std::vector<std::string> FocusableLabels(const PageSpec& page)
		{
			std::vector<std::string> labels;
			for (const ScreenWidget& widget : LayoutPage(page).Widgets)
			{
				if (widget.Focusable)
					labels.push_back(widget.Label);
			}
			return labels;
		}
	}

	int ScreenHost::GetQuantity() const
	{
		return MenuMetrics::QUANTITIES[(std::clamp)(m_QuantityIndex, 0, MenuMetrics::QUANTITY_COUNT - 1)];
	}

	bool ScreenHost::OpenSlots(SlotMode mode, std::string openerLabel)
	{
		m_SlotsMode = mode;
		return Open(ScreenId::Slots, std::move(openerLabel));
	}

	bool ScreenHost::OpenShop(Merchant seller, std::string openerLabel)
	{
		m_Seller = seller;
		return Open(ScreenId::Shop, std::move(openerLabel));
	}

	bool ScreenHost::OpenBin(std::string openerLabel)
	{
		if (!m_Services.State)
			return false;
		return Open(ScreenId::ShippingBin, std::move(openerLabel));
	}

	bool ScreenHost::OpenBoard(std::string openerLabel)
	{
		if (!m_Services.State)
			return false;
		CommunityBoard& board = m_Services.State->Board;
		const int day = m_Director.GetCalendar().GetAbsoluteDay();
		board.RefreshBuilds(m_Director.GetGrid(), day);

		int pick = (std::max)(0, NthOpenBoard(board, day, 0));
		for (int i = 0; i < CommunityBoard::BoardCount; ++i)
		{
			if (board.IsOpen(i, day) && !board.IsBoardDone(i))
			{
				pick = i;
				break;
			}
		}
		m_BoardIndex = pick;
		return Open(ScreenId::Board, std::move(openerLabel));
	}

	bool ScreenHost::OpenDialogue(DialogueScript script, std::string openerLabel)
	{
		m_Dialogue = std::move(script);
		return Open(ScreenId::Dialogue, std::move(openerLabel));
	}

	bool ScreenHost::OpenVillager(VillagerId villager, std::string openerLabel)
	{
		return OpenDialogue(m_VillagerLines ? m_VillagerLines(villager) : PlaceholderVillagerLine(villager), std::move(openerLabel));
	}

	bool ScreenHost::OpenLandmark(Landmark landmark)
	{
		switch (landmark)
		{
		case Landmark::ShippingBin: return OpenBin();
		case Landmark::Shop: return OpenShop(Merchant::Shopkeeper);
		case Landmark::Carpenter: return OpenShop(Merchant::Carpenter);
		case Landmark::NoticeBoard: return OpenBoard();
		case Landmark::MayorHouse: return OpenVillager(VillagerId::Mayor);
		case Landmark::Cottage: return OpenVillager(VillagerId::Child);
		case Landmark::FarmHouse:
		case Landmark::Count:
			break;
		}
		return false;
	}

	MenuView ScreenHost::MakeView() const
	{
		MenuView view;
		view.Pack = &m_Inventory;
		view.Bin = m_Services.State ? &m_Services.State->Bin : &k_NoBin;
		view.Board = m_Services.State ? &m_Services.State->Board : &k_NoBoard;
		view.Day = m_Director.GetCalendar().GetAbsoluteDay();
		view.Status = m_Status;
		for (int slot = 0; slot < SlotProvider::SLOT_COUNT; ++slot)
			view.Slots.push_back(m_Services.Slots ? m_Services.Slots->Describe(slot) : SlotInfo{});
		view.SlotsMode = m_SlotsMode;
		view.SlotConfirm = m_SlotConfirm;
		view.Options = m_Options;
		view.Seller = m_Seller;
		view.Tab = m_Tab;
		view.Quantity = m_QuantityIndex;
		view.Page = m_Page;
		view.BoardIndex = m_BoardIndex;
		view.Dialogue = m_Dialogue;
		view.DialoguePage = m_DialoguePage;
		return view;
	}

	PageSpec ScreenHost::BuildPage()
	{
		PageSpec page = BuildMenuPage(Top(), MakeView());
		switch (Top())
		{
		case ScreenId::Shop:
		case ScreenId::ShippingBin:
		case ScreenId::Credits:
			m_Page = page.PageIndex;
			break;
		case ScreenId::Board:
			m_BoardIndex = page.PageIndex;
			break;
		case ScreenId::Dialogue:
			m_DialoguePage = page.PageIndex;
			break;
		default:
			break;
		}
		return page;
	}

	std::string ScreenHost::FocusSameSlot(const PageSpec& before, const PageSpec& after, const std::string& label) const
	{
		const std::vector<std::string> was = FocusableLabels(before);
		const std::vector<std::string> now = FocusableLabels(after);
		if (now.empty())
			return after.DefaultFocus;
		const auto found = std::find(was.begin(), was.end(), label);
		if (found == was.end())
			return after.DefaultFocus;
		const size_t ordinal = static_cast<size_t>(found - was.begin());
		return now[(std::min)(ordinal, now.size() - 1)];
	}

	void ScreenHost::FlipPage(int direction, int pageCount)
	{
		if (pageCount <= 1)
			return;
		m_Page = (m_Page + direction + pageCount) % pageCount;
	}

	void ScreenHost::Act(PageAction action, int arg, const std::string& label)
	{
		if (!IsOpen())
			return;
		if (action == PageAction::Back)
		{
			Back();
			return;
		}

		const ScreenId screen = Top();
		const uint32_t serial = m_StackSerial;
		const PageSpec before = BuildPage();
		if (action != PageAction::SlotPick)
			m_SlotConfirm = -1;

		switch (screen)
		{
		case ScreenId::Title:
		case ScreenId::Pause:
			ActTitleOrPause(action, label);
			break;
		case ScreenId::Slots:
			if (action == PageAction::SlotPick)
				ActSlots(arg);
			break;
		case ScreenId::Options:
			ActOptions(action);
			break;
		case ScreenId::Credits:
			if (action == PageAction::PrevPage || action == PageAction::NextPage)
				FlipPage(action == PageAction::NextPage ? 1 : -1, before.PageCount);
			break;
		case ScreenId::Shop:
			ActShop(action, arg);
			break;
		case ScreenId::ShippingBin:
			ActBin(action, arg);
			break;
		case ScreenId::Board:
			ActBoard(action, arg);
			break;
		case ScreenId::Dialogue:
			if (action == PageAction::DialogueNext)
			{
				if (m_DialoguePage + 1 >= before.PageCount)
					FinishDialogue();
				else
					++m_DialoguePage;
			}
			break;
		case ScreenId::None:
		case ScreenId::Inventory:
		case ScreenId::BuildPalette:
			break;
		}

		if (m_StackSerial == serial && IsOpen())
		{
			const std::string focus = FocusSameSlot(before, BuildPage(), label);
			if (!focus.empty())
				m_FocusRequest = focus;
		}
	}

	void ScreenHost::ActTitleOrPause(PageAction action, const std::string& label)
	{
		switch (action)
		{
		case PageAction::TitleNew:
			OpenSlots(SlotMode::NewGame, label);
			break;
		case PageAction::TitleContinue:
		{
			bool any = false;
			for (int slot = 0; slot < SlotProvider::SLOT_COUNT && m_Services.Slots; ++slot)
				any = any || m_Services.Slots->Describe(slot).Occupied;
			if (any)
				OpenSlots(SlotMode::Load, label);
			break;
		}
		case PageAction::PauseSave:
			OpenSlots(SlotMode::Save, label);
			break;
		case PageAction::OpenOptions:
			Open(ScreenId::Options, label);
			break;
		case PageAction::OpenCredits:
			Open(ScreenId::Credits, label);
			break;
		case PageAction::PauseResume:
			CloseAll();
			break;
		case PageAction::QuitGame:
			if (m_Services.OnQuit)
				m_Services.OnQuit();
			break;
		default:
			break;
		}
	}

	void ScreenHost::ActSlots(int slot)
	{
		if (!m_Services.Slots || slot < 0 || slot >= SlotProvider::SLOT_COUNT)
		{
			SetStatus("Saving is not available yet");
			return;
		}
		const SlotInfo info = m_Services.Slots->Describe(slot);
		if (m_SlotsMode != SlotMode::Load && info.Occupied && m_SlotConfirm != slot)
		{
			m_SlotConfirm = slot;
			SetStatus(std::format("Slot {} is in use. Press it again to overwrite.", slot + 1));
			return;
		}
		m_SlotConfirm = -1;

		std::string message;
		bool ok = false;
		switch (m_SlotsMode)
		{
		case SlotMode::NewGame: ok = m_Services.Slots->StartNew(slot, message); break;
		case SlotMode::Load: ok = m_Services.Slots->Load(slot, message); break;
		case SlotMode::Save: ok = m_Services.Slots->Save(slot, message); break;
		}
		if (!ok)
		{
			SetStatus(message.empty() ? "That did not work" : message);
			return;
		}
		if (!message.empty())
			Say(message);
		CloseAll();
	}

	void ScreenHost::ActOptions(PageAction action)
	{
		const OptionsState before = m_Options;
		switch (action)
		{
		case PageAction::OptionVSync: m_Options.VSync = !m_Options.VSync; break;
		case PageAction::OptionFullscreen: m_Options.Fullscreen = !m_Options.Fullscreen; break;
		case PageAction::OptionFullscreenKind: m_Options.Exclusive = !m_Options.Exclusive; break;
		case PageAction::OptionHoldToPlace: m_Options.HoldToPlace = !m_Options.HoldToPlace; break;
		default: break;
		}
		if (!(before == m_Options) && m_Services.OnOptionsChanged)
			m_Services.OnOptionsChanged(m_Options);
	}

	void ScreenHost::SetSlider(int arg, float value)
	{
		if (Top() != ScreenId::Options)
			return;
		OptionsState next = m_Options;
		switch (arg)
		{
		case 0: next.Master = value; break;
		case 1: next.Music = value; break;
		case 2: next.Ambience = value; break;
		case 3: next.Sfx = value; break;
		case 4: next.Ui = value; break;
		case 5: next.CameraSpeed = value; break;
		default: return;
		}
		next = ClampOptions(next);
		if (next == m_Options)
			return;
		m_Options = next;
		if (m_Services.OnOptionsChanged)
			m_Services.OnOptionsChanged(m_Options);
	}

	void ScreenHost::ActShop(PageAction action, int arg)
	{
		switch (action)
		{
		case PageAction::Tab:
			if (m_Seller == Merchant::Shopkeeper)
			{
				m_Tab = arg == 1 ? 1 : 0;
				m_Page = 0;
				SetStatus({});
			}
			return;
		case PageAction::CycleQuantity:
			m_QuantityIndex = (m_QuantityIndex + 1) % MenuMetrics::QUANTITY_COUNT;
			return;
		case PageAction::PrevPage:
		case PageAction::NextPage:
			FlipPage(action == PageAction::NextPage ? 1 : -1, BuildMenuPage(ScreenId::Shop, MakeView()).PageCount);
			return;
		case PageAction::Row:
			break;
		default:
			return;
		}

		const int quantity = GetQuantity();
		if (m_Tab == 0 || m_Seller == Merchant::Carpenter)
		{
			const std::vector<ShopOffer> offers = ShopStock(m_Seller);
			if (arg < 0 || arg >= static_cast<int>(offers.size()))
				return;
			const ShopOffer& offer = offers[static_cast<size_t>(arg)];
			if (offer.Kind == OfferKind::Item)
			{
				const TradeResult result = ShopBuyItem(m_Inventory, offer.Item, quantity);
				SetStatus(result == TradeResult::Ok ? std::format("Bought {} {}", quantity, GetItemDef(offer.Item).Name) : std::string(ToString(result)));
			}
			else
			{
				const TradeResult result = ShopBuyPlan(m_Inventory, offer.Build);
				SetStatus(result == TradeResult::Ok ? std::format("Bought the {} plan", GetBuildDef(offer.Build).Name) : std::string(ToString(result)));
			}
			return;
		}

		if (arg < 0 || arg >= ITEM_COUNT)
			return;
		const ItemId item = static_cast<ItemId>(arg);
		const int count = (std::min)(quantity, m_Inventory.Count(item));
		const int price = ShopSellBackPrice(item);
		const TradeResult result = ShopSellItem(m_Inventory, item, count);
		SetStatus(result == TradeResult::Ok ? std::format("Sold {} {} for {} coins", count, GetItemDef(item).Name, price * count) : std::string(ToString(result)));
	}

	void ScreenHost::ActBin(PageAction action, int arg)
	{
		if (!m_Services.State)
			return;
		ShippingBin& bin = m_Services.State->Bin;
		switch (action)
		{
		case PageAction::Tab:
			m_Tab = arg == 1 ? 1 : 0;
			m_Page = 0;
			SetStatus({});
			return;
		case PageAction::CycleQuantity:
			m_QuantityIndex = (m_QuantityIndex + 1) % MenuMetrics::QUANTITY_COUNT;
			return;
		case PageAction::PrevPage:
		case PageAction::NextPage:
			FlipPage(action == PageAction::NextPage ? 1 : -1, BuildMenuPage(ScreenId::ShippingBin, MakeView()).PageCount);
			return;
		case PageAction::Row:
			break;
		default:
			return;
		}

		if (arg < 0 || arg >= ITEM_COUNT)
			return;
		const ItemId item = static_cast<ItemId>(arg);
		const int quantity = GetQuantity();
		if (m_Tab == 0)
		{
			const int count = (std::min)(quantity, m_Inventory.Count(item));
			const TradeResult result = bin.Ship(m_Inventory, item, count);
			SetStatus(result == TradeResult::Ok ? std::format("Put {} {} in the bin", count, GetItemDef(item).Name) : std::string(ToString(result)));
		}
		else
		{
			const int count = (std::min)(quantity, bin.Held(item));
			const TradeResult result = bin.Retrieve(m_Inventory, item, count);
			SetStatus(result == TradeResult::Ok ? std::format("Took {} {} back", count, GetItemDef(item).Name) : std::string(ToString(result)));
		}
	}

	void ScreenHost::ActBoard(PageAction action, int arg)
	{
		if (!m_Services.State)
			return;
		CommunityBoard& board = m_Services.State->Board;
		const int day = m_Director.GetCalendar().GetAbsoluteDay();
		switch (action)
		{
		case PageAction::PrevPage:
		case PageAction::NextPage:
		{
			const int count = OpenBoardCount(board, day);
			if (count > 1)
			{
				const int ordinal = OpenBoardOrdinal(board, day, m_BoardIndex);
				m_BoardIndex = NthOpenBoard(board, day, (ordinal + (action == PageAction::NextPage ? 1 : count - 1)) % count);
				SetStatus({});
			}
			return;
		}
		case PageAction::BoardGive:
		{
			if (arg < 0 || arg >= BOARD_REQUESTS)
				return;
			const BoardRequest& request = GetBoardDef(static_cast<Season>(m_BoardIndex)).Requests[static_cast<size_t>(arg)];
			const TradeResult result = GiveGift(m_Inventory, board, request.Villager, request.Item, day);
			SetStatus(result == TradeResult::Ok ? std::format("Gave {} one {}", GetVillagerDef(request.Villager).Name, GetItemDef(request.Item).Name) : std::string(ToString(result)));
			return;
		}
		case PageAction::FestivalStart:
			OpenDialogue(FestivalScript(), "Hear the mayor##board");
			return;
		default:
			return;
		}
	}

	void ScreenHost::FinishDialogue()
	{
		if (m_Dialogue.End == DialogueEnd::AcknowledgeFestival && m_Services.State
			&& m_Services.State->Board.AcknowledgeFestival(m_Director.GetGrid()))
			Say("The festival begins. The boards ask for more now.");
		Back();
	}

}
