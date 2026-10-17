#pragma once
#include <cstdint>
#include <string>

namespace Dingo
{

	enum class SlotMode : uint8_t { NewGame, Load, Save };

	struct SlotInfo
	{
		bool Occupied = false;
		// One or two short lines the slot button shows next to its name; about 100 characters fit.
		std::string Summary;
		// Something is on disk but no copy of it reads: it cannot be loaded, only overwritten or deleted.
		bool Damaged = false;
	};

	// What the slots screen needs from the save system, so the screen is finished before the saves exist. The layer
	// hands the host the real provider when the saves arrive; each call may refuse and says why in `message`.
	class SlotProvider
	{
	public:
		static constexpr int SLOT_COUNT = 3;

		virtual ~SlotProvider() = default;
		virtual SlotInfo Describe(int slot) const = 0;
		// Resets the game to a fresh valley that will save into `slot`.
		virtual bool StartNew(int slot, std::string& message) = 0;
		virtual bool Load(int slot, std::string& message) = 0;
		virtual bool Save(int slot, std::string& message) = 0;
		virtual bool Delete(int slot, std::string& message) = 0;
	};

	// Until saving is wired: every slot is empty, a new game keeps the valley that is running, and a save or load says so.
	class NullSlotProvider final : public SlotProvider
	{
	public:
		SlotInfo Describe(int) const override { return { false, "Empty" }; }
		bool StartNew(int, std::string& message) override { message = "New game"; return true; }
		bool Load(int, std::string& message) override { message = "Loading arrives with the save system"; return false; }
		bool Save(int, std::string& message) override { message = "Saving arrives with the save system"; return false; }
		bool Delete(int, std::string& message) override { message = "There is nothing to delete"; return false; }
	};

}
