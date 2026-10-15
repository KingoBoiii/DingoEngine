#pragma once
#include "Life/PlayerIntent.h"
#include "Placing/BuildController.h"
#include "Placing/Inventory.h"
#include "Ui/ScreenLayouts.h"
#include "Ui/ScreenStack.h"
#include "ValleyDirector.h"

#include <string>

namespace Dingo
{

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

		void Say(std::string text);
		const std::string& GetToast() const { return m_Toast; }
		uint32_t GetToastSerial() const { return m_ToastSerial; }

	private:
		void AfterStackChange();
		void SyncHeld();

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
	};

}
