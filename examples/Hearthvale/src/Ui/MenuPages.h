#pragma once
#include "Defs/VillagerDefs.h"
#include "Farm/Board.h"
#include "Farm/Economy.h"
#include "Ui/OptionsModel.h"
#include "Ui/PageLayout.h"
#include "Ui/SlotProvider.h"

#include <functional>
#include <string>
#include <vector>

namespace Dingo
{

	namespace MenuMetrics
	{
		inline constexpr int LIST_ROWS = 6;
		inline constexpr int CREDIT_ROWS = 6;
		inline constexpr int QUANTITIES[] = { 1, 5, 25 };
		inline constexpr int QUANTITY_COUNT = 3;

		inline constexpr float LIST_W = 620.0f;
		inline constexpr float ITEM_H = 40.0f;
		inline constexpr float ITEM_NAME_W = 250.0f;
		inline constexpr float ITEM_PRICE_W = 140.0f;
		inline constexpr float ITEM_HAVE_W = 150.0f;
		inline constexpr float STATUS_H = 44.0f;

		inline constexpr float SAVE_SLOT_W = 170.0f;
		inline constexpr float SAVE_SLOT_H = 64.0f;
		inline constexpr float SLOTS_W = 560.0f;

		inline constexpr float OPTION_LABEL_W = 150.0f;
		inline constexpr float OPTION_CONTROL_W = 300.0f;
		inline constexpr float OPTIONS_W = OPTION_LABEL_W + OPTION_CONTROL_W + PageMetrics::VALUE_W + 2.0f * UiMetrics::GAP;

		inline constexpr float CREDITS_W = 764.0f;

		inline constexpr float BOARD_W = 780.0f;
		inline constexpr float BOARD_ROW_H = 52.0f;
		inline constexpr float BOARD_TEXT_W = 360.0f;

		inline constexpr float DIALOGUE_W = 640.0f;
		inline constexpr float DIALOGUE_SIZE = 20.0f;
		inline constexpr int DIALOGUE_LINES = 4;
		inline constexpr float DIALOGUE_H = 112.0f;
	}

	enum class DialogueEnd : uint8_t { None, AcknowledgeFestival };

	// What a villager says. The pages are cut from `Text` to fit the box, so a line may be as long as the writer likes.
	struct DialogueScript
	{
		std::string Speaker;
		std::string Text;
		DialogueEnd End = DialogueEnd::None;
		// A VillagerId the last page offers to give a gift to; -1 for a speech with no gift button.
		int GiftTo = -1;
	};

	// The hook the villagers' own lines plug into: asked each time the player talks to one.
	using VillagerLineFn = std::function<DialogueScript(VillagerId)>;
	// The reply to the held item given to a villager, from the Give gift button.
	using VillagerGiftFn = std::function<DialogueScript(VillagerId)>;

	DialogueScript PlaceholderVillagerLine(VillagerId villager);
	DialogueScript FestivalScript();
	std::vector<std::string> DialoguePages(const DialogueScript& script);

	// Everything a page reads, so a page builder is a pure function of this struct.
	struct MenuView
	{
		const Inventory* Pack = nullptr;
		const ShippingBin* Bin = nullptr;
		const CommunityBoard* Board = nullptr;
		int Day = 0;
		std::string Status;

		std::vector<SlotInfo> Slots;
		SlotMode SlotsMode = SlotMode::NewGame;
		int SlotConfirm = -1;

		OptionsState Options;

		Merchant Seller = Merchant::Shopkeeper;
		int Tab = 0;
		int Quantity = 0;
		int Page = 0;
		int BoardIndex = 0;

		DialogueScript Dialogue;
		int DialoguePage = 0;
	};

	// The screens of this file as data. `PageIndex` and `PageCount` of the spec say where a paged list ended up, so the
	// caller can clamp a page the list outgrew.
	PageSpec BuildMenuPage(ScreenId id, const MenuView& view);

	bool AnySlotOccupied(const std::vector<SlotInfo>& slots);
	int OpenBoardCount(const CommunityBoard& board, int day);
	// The index of the n-th open board, -1 past the end.
	int NthOpenBoard(const CommunityBoard& board, int day, int n);
	int OpenBoardOrdinal(const CommunityBoard& board, int day, int boardIndex);

	// Items a tab lists, in item-id order.
	std::vector<ItemId> SellableItems(const Inventory& pack);
	std::vector<ItemId> ShippableItems(const Inventory& pack);
	std::vector<ItemId> BinItems(const ShippingBin& bin);

}
