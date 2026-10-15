#pragma once
#include "Defs/BuildDefs.h"
#include "Defs/ItemDefs.h"
#include "GameTuning.h"

#include <array>
#include <cstdint>
#include <span>

namespace Dingo
{

	struct ItemStack
	{
		ItemId Item = ItemId::Count;
		int Count = 0;

		constexpr bool Empty() const { return Count <= 0; }
		constexpr bool operator==(const ItemStack&) const = default;
	};

	// Everything the farmer holds: INVENTORY_SLOTS stack slots (the first HOTBAR_SLOTS are the hotbar, the rest the
	// 6 x 4 backpack), the selected hotbar slot, coins and the building plans learned at the carpenter. Plain data, no
	// renderer, so --check and the save code use it directly. Nothing that lands here may exceed a stack or the slot
	// count: Add reports how much fitted, and callers that must not lose a pickup ask CanAdd first.
	class Inventory
	{
	public:
		Inventory();

		// Total held across all slots.
		int Count(ItemId item) const;
		// How many more of `item` fit: the gaps in its stacks plus whole stacks in the empty slots.
		int Room(ItemId item) const;
		bool CanAdd(ItemId item, int count) const;
		// Whether all of `stacks` fit together; they may compete for the same free slots.
		bool CanAddAll(std::span<const ItemStack> stacks) const;
		// Fills partial stacks of `item` first, then empty slots. Tools and seeds prefer the hotbar and everything else
		// the backpack, each spilling into the other when full. Returns how many were added.
		int Add(ItemId item, int count);
		// Takes nothing and returns false when fewer than `count` are held. Empties the last slots first.
		bool Remove(ItemId item, int count);

		int FreeSlots() const;
		const ItemStack& GetSlot(int index) const;
		// Merges the stack at `from` into the one at `to` when they hold the same item, else swaps the two.
		void MoveSlot(int from, int to);
		// The first slot holding `item`, or -1.
		int FindSlot(ItemId item) const;

		int GetSelectedSlot() const { return m_Selected; }
		void SelectSlot(int slot);
		// Steps to the next hotbar slot that holds something, wrapping; stays put when the hotbar is empty.
		void CycleSelection(int direction);
		const ItemStack& GetSelected() const { return m_Slots[static_cast<size_t>(m_Selected)]; }

		int GetCoins() const { return m_Coins; }
		void AddCoins(int coins) { m_Coins += coins; }
		bool SpendCoins(int coins);

		bool KnowsPlan(BuildId id) const;
		void LearnPlan(BuildId id);
		void LearnAllPlans();

		// Coins, tools and seeds of GameTuning.h's starting loadout, on top of what is held.
		void GiveStarterLoadout();

		uint64_t Hash() const;

	private:
		friend struct SaveCodec;

	private:
		std::array<ItemStack, INVENTORY_SLOTS> m_Slots{};
		std::array<bool, BUILD_COUNT> m_Plans{};
		int m_Selected = 0;
		int m_Coins = 0;
	};

}
