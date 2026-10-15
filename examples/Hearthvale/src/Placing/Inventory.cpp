#include "Placing/Inventory.h"

namespace Dingo
{

	int Inventory::Count(ItemId item) const
	{
		const size_t index = static_cast<size_t>(item);
		return index < m_Counts.size() ? m_Counts[index] : 0;
	}

	void Inventory::Add(ItemId item, int count)
	{
		const size_t index = static_cast<size_t>(item);
		if (index < m_Counts.size() && count > 0)
			m_Counts[index] += count;
	}

	bool Inventory::Remove(ItemId item, int count)
	{
		const size_t index = static_cast<size_t>(item);
		if (index >= m_Counts.size() || count < 0 || m_Counts[index] < count)
			return false;
		m_Counts[index] -= count;
		return true;
	}

	bool Inventory::SpendCoins(int coins)
	{
		if (coins < 0 || m_Coins < coins)
			return false;
		m_Coins -= coins;
		return true;
	}

}
