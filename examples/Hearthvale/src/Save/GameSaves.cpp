#include "Save/GameSaves.h"

#include <DingoEngine.h>

#include <charconv>
#include <format>
#include <random>

namespace Dingo
{

	GameSaves::GameSaves(ValleyDirector& director, GameState& state, SaveStore store)
		: m_Director(director), m_State(state), m_Store(std::move(store))
	{
		m_Director.OnDayRoll(DayRollStep::Autosave, [this](ValleyDirector&, DayRollReason) { OnDayRolled(); });
		TakeSnapshot();
	}

	std::string GameSaves::SlotName(int slot)
	{
		return std::format("slot{}", slot + 1);
	}

	int GameSaves::ParseSlot(std::string_view text)
	{
		int number = 0;
		const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), number);
		if (ec == std::errc() && end == text.data() + text.size() && number >= 1 && number <= SLOT_COUNT)
			return number - 1;
		for (int slot = 0; slot < SLOT_COUNT; ++slot)
		{
			if (text == SlotName(slot))
				return slot;
		}
		return -1;
	}

	void GameSaves::TakeSnapshot()
	{
		m_Snapshot = WriteSave(m_Director, m_State, SaveMeta{ m_PlaySeconds });
	}

	void GameSaves::OnDayRolled()
	{
		TakeSnapshot();
		if (m_Active < 0)
			return;
		if (WriteSnapshot(m_Active))
			++m_Autosaves;
		else
			DE_WARN("Hearthvale: the autosave into slot {} failed", m_Active + 1);
	}

	bool GameSaves::WriteSnapshot(int slot)
	{
		m_Described[static_cast<size_t>(slot)].reset();
		return m_Store.Write(SlotName(slot), m_Snapshot);
	}

	SlotInfo GameSaves::Describe(int slot) const
	{
		if (!ValidSlot(slot))
			return {};
		std::optional<SlotInfo>& cached = m_Described[static_cast<size_t>(slot)];
		if (cached)
			return *cached;

		LoadedGame game;
		const SlotRead read = m_Store.Read(SlotName(slot), game);
		SlotInfo info;
		switch (read.State)
		{
		case SlotState::Empty:
			info.Summary = "Empty";
			break;
		case SlotState::Ok:
			info.Occupied = true;
			info.Summary = DescribeGame(game) + (read.FromBackup ? " (from its backup)" : "");
			break;
		case SlotState::Damaged:
			info.Damaged = true;
			info.Summary = "Damaged: no copy of this save can be read";
			break;
		}
		cached = info;
		return info;
	}

	bool GameSaves::StartNew(int slot, std::string& message)
	{
		if (!ValidSlot(slot))
			return false;
		const uint32_t seed = m_NewGameSeed ? *m_NewGameSeed : std::random_device{}();
		ApplyLoadedGame(MakeNewGame(seed), m_Director, m_State);
		m_PlaySeconds = 0.0;
		m_Active = slot;
		TakeSnapshot();
		const bool written = WriteSnapshot(slot);
		if (m_OnGameReplaced)
			m_OnGameReplaced();
		message = written ? std::format("New game in slot {}", slot + 1) : std::format("New game; slot {} could not be written", slot + 1);
		return true;
	}

	bool GameSaves::Load(int slot, std::string& message)
	{
		if (!ValidSlot(slot))
			return false;
		LoadedGame game;
		const SlotRead read = m_Store.Read(SlotName(slot), game);
		m_Described[static_cast<size_t>(slot)].reset();
		if (read.State == SlotState::Empty)
		{
			message = std::format("Slot {} is empty", slot + 1);
			return false;
		}
		if (read.State == SlotState::Damaged)
		{
			message = std::format("Slot {} is damaged and cannot be loaded", slot + 1);
			return false;
		}

		m_PlaySeconds = game.Meta.PlaySeconds;
		ApplyLoadedGame(std::move(game), m_Director, m_State);
		m_Active = slot;
		TakeSnapshot();
		if (m_OnGameReplaced)
			m_OnGameReplaced();
		message = read.FromBackup ? std::format("Loaded slot {} from its backup", slot + 1) : std::format("Loaded slot {}", slot + 1);
		return true;
	}

	bool GameSaves::Save(int slot, std::string& message)
	{
		if (!ValidSlot(slot))
			return false;
		if (!WriteSnapshot(slot))
		{
			message = std::format("Slot {} could not be written", slot + 1);
			return false;
		}
		m_Active = slot;
		message = std::format("Saved this morning's game to slot {}", slot + 1);
		return true;
	}

	bool GameSaves::Delete(int slot, std::string& message)
	{
		if (!ValidSlot(slot))
			return false;
		m_Described[static_cast<size_t>(slot)].reset();
		if (!m_Store.Delete(SlotName(slot)))
		{
			message = std::format("Slot {} could not be deleted", slot + 1);
			return false;
		}
		if (m_Active == slot)
			m_Active = -1;
		message = std::format("Slot {} deleted", slot + 1);
		return true;
	}

}
