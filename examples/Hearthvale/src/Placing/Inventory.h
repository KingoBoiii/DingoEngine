#pragma once
#include "Defs/ItemDefs.h"

#include <array>

namespace Dingo
{

	// Placeholder counters for P5: no slots, stacks or capacity. P7 replaces the body and keeps this surface, which
	// is all the build and clear code touches.
	class Inventory
	{
	public:
		int Count(ItemId item) const;
		void Add(ItemId item, int count);
		// Takes nothing and returns false when fewer than `count` are held.
		bool Remove(ItemId item, int count);

		int GetCoins() const { return m_Coins; }
		void AddCoins(int coins) { m_Coins += coins; }
		bool SpendCoins(int coins);

	private:
		std::array<int, ITEM_COUNT> m_Counts{};
		int m_Coins = 0;
	};

}
