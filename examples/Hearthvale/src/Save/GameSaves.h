#pragma once
#include "Save/SaveStore.h"
#include "Ui/SlotProvider.h"

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace Dingo
{

	// The three save slots behind the slots screen (HD7). The game saves at the day roll only: each roll takes a
	// snapshot of the new morning and writes it into the active slot, and "Save game" writes that same snapshot, so a
	// save always holds the start of a day. Starting or loading a game makes its slot the active one.
	class GameSaves final : public SlotProvider
	{
	public:
		GameSaves(ValleyDirector& director, GameState& state, SaveStore store);
		GameSaves(const GameSaves&) = delete;
		GameSaves& operator=(const GameSaves&) = delete;

		static std::string SlotName(int slot);
		// "1" to "3" or a slot's file name, as --load takes it; -1 when it names no slot.
		static int ParseSlot(std::string_view text);

		SlotInfo Describe(int slot) const override;
		bool StartNew(int slot, std::string& message) override;
		bool Load(int slot, std::string& message) override;
		bool Save(int slot, std::string& message) override;
		bool Delete(int slot, std::string& message) override;

		// Runs after a new or loaded game replaced the running one, so the layer can rebuild what it derives from it.
		void SetOnGameReplaced(std::function<void()> fn) { m_OnGameReplaced = std::move(fn); }
		// The seed of a new game; unset, each one rolls its own.
		void SetNewGameSeed(std::optional<uint32_t> seed) { m_NewGameSeed = seed; }

		// Takes the running game as the snapshot "Save game" writes, as at a day roll, without writing it.
		void TakeSnapshot();
		const SaveData& GetSnapshot() const { return m_Snapshot; }

		void AddPlayTime(float seconds) { m_PlaySeconds += static_cast<double>(seconds); }
		double GetPlaySeconds() const { return m_PlaySeconds; }

		int GetActiveSlot() const { return m_Active; }
		const SaveStore& GetStore() const { return m_Store; }
		int GetAutosaveCount() const { return m_Autosaves; }

	private:
		void OnDayRolled();
		bool WriteSnapshot(int slot);
		bool ValidSlot(int slot) const { return slot >= 0 && slot < SLOT_COUNT; }

	private:
		ValleyDirector& m_Director;
		GameState& m_State;
		SaveStore m_Store;
		SaveData m_Snapshot;
		double m_PlaySeconds = 0.0;
		int m_Active = -1;
		int m_Autosaves = 0;
		std::optional<uint32_t> m_NewGameSeed;
		std::function<void()> m_OnGameReplaced;
		mutable std::array<std::optional<SlotInfo>, SLOT_COUNT> m_Described;
	};

}
