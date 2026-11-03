#include "Ui/ScreenHost.h"

#include <format>

namespace Dingo
{

	ScreenHost::ScreenHost(ValleyDirector& director, Inventory& inventory, BuildController& controller)
		: m_Director(director), m_Inventory(inventory), m_Controller(controller)
	{}

	void ScreenHost::Update(PlayerIntent& intent, bool blocked)
	{
		m_Changed = false;

		if (intent.ToggleInventory)
		{
			intent.ToggleInventory = false;
			if (IsOpen())
			{
				if (Top() == ScreenId::Inventory || Top() == ScreenId::BuildPalette)
					CloseAll();
			}
			else if (!blocked)
			{
				Open(ScreenId::Inventory);
			}
		}
		else if (intent.ToggleBuild)
		{
			if (IsOpen())
			{
				intent.ToggleBuild = false;
				if (Top() == ScreenId::BuildPalette)
					Back();
				else
					SwitchScreen(false);
			}
			else if (!blocked && m_Controller.GetMode() == BuildMode::Tools)
			{
				intent.ToggleBuild = false;
				Open(ScreenId::BuildPalette);
			}
		}

		if (IsOpen())
		{
			if (intent.Pause)
				Back();
			else if (intent.HotbarNext || intent.HotbarPrevious)
				SwitchScreen(false);
			else if (intent.HotbarSlot >= 0 && Top() == ScreenId::Inventory)
				EquipHotbarSlot(intent.HotbarSlot);
		}
		else if (intent.Pause && !blocked && !m_Changed)
		{
			Open(ScreenId::Pause);
		}

		if (IsOpen() || m_Changed)
			intent.ClearPulses();
	}

	bool ScreenHost::Open(ScreenId id, std::string openerLabel)
	{
		if (!m_Stack.Open(id, std::move(openerLabel)))
			return false;
		m_Changed = true;
		++m_StackSerial;
		ResetScreenState();
		AfterStackChange();
		return true;
	}

	void ScreenHost::Back()
	{
		if (m_Changed || !IsOpen() || Top() == ScreenId::Title)
			return;
		if (Top() == ScreenId::Inventory && m_Picked >= 0)
		{
			m_Picked = -1;
			m_Changed = true;
			return;
		}
		NoteClosing(false);
		m_Stack.Close();
		m_Changed = true;
		++m_StackSerial;
		m_FocusRequest = m_Stack.TakeReturnFocus();
		AfterStackChange();
	}

	void ScreenHost::CloseAll()
	{
		if (!IsOpen())
			return;
		NoteClosing(true);
		m_Stack.CloseAll();
		m_Changed = true;
		++m_StackSerial;
		m_FocusRequest.clear();
		AfterStackChange();
	}

	void ScreenHost::SwitchScreen(bool viaTab)
	{
		if (m_Changed || !IsOpen())
			return;
		const ScreenId target = Top() == ScreenId::Inventory ? ScreenId::BuildPalette : ScreenId::Inventory;
		if (Top() != ScreenId::Inventory && Top() != ScreenId::BuildPalette)
			return;
		if (!m_Stack.Replace(target))
			return;
		m_Changed = true;
		++m_StackSerial;
		m_Picked = -1;
		m_FocusRequest = viaTab ? (target == ScreenId::Inventory ? TAB_INVENTORY_LABEL : TAB_BUILD_LABEL) : std::string();
	}

	void ScreenHost::ActivateSlot(int index)
	{
		if (Top() != ScreenId::Inventory || index < 0 || index >= INVENTORY_SLOTS)
			return;
		if (m_Picked < 0)
		{
			if (!m_Inventory.GetSlot(index).Empty())
				m_Picked = index;
			return;
		}
		if (m_Picked == index)
		{
			m_Picked = -1;
			return;
		}
		m_Inventory.MoveSlot(m_Picked, index);
		m_Picked = -1;
		SyncHeld();
		m_FocusRequest = SlotLabel(index, m_Inventory.GetSlot(index));
	}

	void ScreenHost::EquipHotbarSlot(int slot)
	{
		if (slot < 0 || slot >= HOTBAR_SLOTS)
			return;
		m_Inventory.SelectSlot(slot);
		SyncHeld();
	}

	bool ScreenHost::ChooseBuildable(BuildId id)
	{
		if (Top() != ScreenId::BuildPalette)
			return false;
		if (!m_Inventory.KnowsPlan(id))
		{
			Say(ToString(PlaceResult::PlanLocked));
			return false;
		}
		m_Controller.SetMode(BuildMode::Build);
		m_Controller.SelectBuildable(id);
		Say(std::format("Building: {}", GetBuildDef(id).Name));
		CloseAll();
		return true;
	}

	void ScreenHost::UseTools()
	{
		if (Top() != ScreenId::BuildPalette)
			return;
		m_Controller.SetMode(BuildMode::Tools);
		Say("Tool mode");
		CloseAll();
	}

	std::string ScreenHost::TakeFocusRequest()
	{
		std::string request = std::move(m_FocusRequest);
		m_FocusRequest.clear();
		return request;
	}

	void ScreenHost::Say(std::string text)
	{
		m_Toast = std::move(text);
		++m_ToastSerial;
	}

	void ScreenHost::AfterStackChange()
	{
		m_Picked = -1;
		if (IsOpen() && !m_HoldsPause)
		{
			m_WasPaused = m_Director.IsTimePaused();
			m_Director.SetTimePaused(true);
			m_HoldsPause = true;
		}
		else if (!IsOpen() && m_HoldsPause)
		{
			m_Director.SetTimePaused(m_WasPaused);
			m_HoldsPause = false;
		}
	}

	void ScreenHost::ResetScreenState()
	{
		m_Tab = 0;
		m_Page = 0;
		m_Status.clear();
		m_SlotConfirm = -1;
		m_SlotDeleteConfirm = -1;
		m_DialoguePage = 0;
	}

	void ScreenHost::NoteClosing(bool all)
	{
		const bool closesOptions = all ? m_Stack.Contains(ScreenId::Options) : Top() == ScreenId::Options;
		if (closesOptions && m_Services.OnOptionsClosed)
			m_Services.OnOptionsClosed(m_Options);
	}

	void ScreenHost::SyncHeld()
	{
		const ItemStack& held = m_Inventory.GetSelected();
		if (!held.Empty() && GetItemDef(held.Item).Kind == ItemKind::Seed)
			m_Controller.SelectSeed(GetItemDef(held.Item).Crop);
	}

}
