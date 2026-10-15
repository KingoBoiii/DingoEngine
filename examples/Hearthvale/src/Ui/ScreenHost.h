#pragma once
#include "Farm/GameState.h"
#include "Life/PlayerIntent.h"
#include "Placing/BuildController.h"
#include "Placing/Inventory.h"
#include "Ui/MenuPages.h"
#include "Ui/ScreenLayouts.h"
#include "Ui/ScreenStack.h"
#include "ValleyDirector.h"
#include "World/ValleyLayout.h"

#include <functional>
#include <string>

namespace Dingo
{

	// What the menus reach outside the host. Every member is optional: a screen whose service is missing shows what it can.
	struct MenuServices
	{
		// The shipping bin and the community board.
		GameState* State = nullptr;
		SlotProvider* Slots = nullptr;
		// Called after every change to an option, with the new state; the layer applies it.
		std::function<void(const OptionsState&)> OnOptionsChanged;
		// Called when the options screen closes, so the layer can write them to disk.
		std::function<void(const OptionsState&)> OnOptionsClosed;
		std::function<void()> OnQuit;
		// Called when a menu button is pressed, before its action runs.
		std::function<void()> OnClick;
	};

	// What the screens do, with no renderer: opening and closing, which input the farmer loses while one is open,
	// the pause of the sim clock (HD5), the inventory's pick-up and put-down, and what the build palette selects. The
	// screens that draw it are GPU-side and only read this and call the actions below. A later screen joins by
	// adding a ScreenId and a case in Update.
	class ScreenHost
	{
	public:
		ScreenHost(ValleyDirector& director, Inventory& inventory, BuildController& controller);
		ScreenHost(const ScreenHost&) = delete;
		ScreenHost& operator=(const ScreenHost&) = delete;

		// Once a frame, after the intent is polled. Opens and closes screens from the toggle pulses and, while a screen
		// is open or was just closed, clears every pulse so nothing reaches the farmer. `blocked` is a sleep fade.
		void Update(PlayerIntent& intent, bool blocked);

		bool IsOpen() const { return !m_Stack.IsEmpty(); }
		bool IsOpen(ScreenId id) const { return m_Stack.Contains(id); }
		ScreenId Top() const { return m_Stack.Top(); }
		const ScreenStack& GetStack() const { return m_Stack; }

		bool Open(ScreenId id, std::string openerLabel = {});
		// The back input: cancels a held stack first, else closes the top screen. At most one change a frame.
		void Back();
		void CloseAll();
		// Swaps the open inventory and build palette. `viaTab` asks for the focus to land on the new screen's tab.
		void SwitchScreen(bool viaTab);

		// The inventory's A: picks up the stack, or puts the held one down (merging or swapping).
		void ActivateSlot(int index);
		int GetPicked() const { return m_Picked; }
		// Digits 1-9 while the inventory is open.
		void EquipHotbarSlot(int slot);

		// False, with a toast, when the plan is not bought.
		bool ChooseBuildable(BuildId id);
		void UseTools();

		// The label a screen should give the focus to this frame, once; empty when none.
		std::string TakeFocusRequest();

		void SetServices(MenuServices services) { m_Services = std::move(services); }
		void SetVillagerLines(VillagerLineFn lines) { m_VillagerLines = std::move(lines); }
		void SetVillagerGift(VillagerGiftFn gift) { m_VillagerGift = std::move(gift); }
		void SetOptions(const OptionsState& options) { m_Options = ClampOptions(options); }
		const OptionsState& GetOptions() const { return m_Options; }

		bool OpenTitle() { return Open(ScreenId::Title); }
		bool OpenSlots(SlotMode mode, std::string openerLabel = {});
		bool OpenShop(Merchant seller, std::string openerLabel = {});
		bool OpenBin(std::string openerLabel = {});
		bool OpenBoard(std::string openerLabel = {});
		bool OpenDialogue(DialogueScript script, std::string openerLabel = {});
		bool OpenVillager(VillagerId villager, std::string openerLabel = {});
		// What interacting with a landmark opens; false for the farmhouse (which is bed) or when its screen is already up.
		bool OpenLandmark(Landmark landmark);

		// The page the top screen draws, from the live state. Also clamps the paging the lists keep to what exists.
		PageSpec BuildPage();
		// A button the player picked. `label` is the widget's label, which the host uses to keep the focus in place.
		void Act(PageAction action, int arg, const std::string& label);
		// A slider the player moved to `value`; arg is the cell's Arg.
		void SetSlider(int arg, float value);
		const std::string& GetStatus() const { return m_Status; }
		int GetQuantity() const;

		void Say(std::string text);
		const std::string& GetToast() const { return m_Toast; }
		uint32_t GetToastSerial() const { return m_ToastSerial; }

	private:
		void AfterStackChange();
		void SyncHeld();
		void ResetScreenState();
		void NoteClosing(bool all);
		void SetStatus(std::string text) { m_Status = std::move(text); }
		MenuView MakeView() const;
		std::string FocusSameSlot(const PageSpec& before, const PageSpec& after, const std::string& label) const;
		void ActTitleOrPause(PageAction action, const std::string& label);
		void ActSlots(int slot);
		void ActSlotDelete(int slot);
		void ActShop(PageAction action, int arg);
		void ActBin(PageAction action, int arg);
		void ActBoard(PageAction action, int arg);
		void ActOptions(PageAction action);
		void FlipPage(int direction, int pageCount);
		void FinishDialogue();

	private:
		ValleyDirector& m_Director;
		Inventory& m_Inventory;
		BuildController& m_Controller;
		ScreenStack m_Stack;
		int m_Picked = -1;
		bool m_HoldsPause = false;
		bool m_WasPaused = false;
		bool m_Changed = false;
		std::string m_FocusRequest;
		std::string m_Toast;
		uint32_t m_ToastSerial = 0;

		MenuServices m_Services;
		VillagerLineFn m_VillagerLines;
		VillagerGiftFn m_VillagerGift;
		OptionsState m_Options;
		std::string m_Status;
		uint32_t m_StackSerial = 0;
		SlotMode m_SlotsMode = SlotMode::NewGame;
		int m_SlotConfirm = -1;
		int m_SlotDeleteConfirm = -1;
		Merchant m_Seller = Merchant::Shopkeeper;
		int m_Tab = 0;
		int m_QuantityIndex = 0;
		int m_Page = 0;
		int m_BoardIndex = 0;
		DialogueScript m_Dialogue;
		int m_DialoguePage = 0;
	};

}
