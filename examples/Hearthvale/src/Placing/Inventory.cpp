#include "Placing/Inventory.h"
#include "Sim/Fnv.h"

#include <algorithm>

namespace Dingo
{

	namespace
	{
		bool IsItem(ItemId item)
		{
			return static_cast<int>(item) >= 0 && static_cast<int>(item) < ITEM_COUNT;
		}

		bool PrefersHotbar(ItemId item)
		{
			const ItemKind kind = GetItemDef(item).Kind;
			return kind == ItemKind::Tool || kind == ItemKind::Seed;
		}

		// Slot indices in the order Add visits them.
		std::array<int, INVENTORY_SLOTS> FillOrder(bool hotbarFirst)
		{
			std::array<int, INVENTORY_SLOTS> order{};
			int next = 0;
			if (hotbarFirst)
			{
				for (int i = 0; i < INVENTORY_SLOTS; ++i)
					order[static_cast<size_t>(next++)] = i;
			}
			else
			{
				for (int i = HOTBAR_SLOTS; i < INVENTORY_SLOTS; ++i)
					order[static_cast<size_t>(next++)] = i;
				for (int i = 0; i < HOTBAR_SLOTS; ++i)
					order[static_cast<size_t>(next++)] = i;
			}
			return order;
		}
	}

	Inventory::Inventory()
	{
		for (const BuildDef& def : GetBuildDefs())
			m_Plans[static_cast<size_t>(def.Id)] = def.PlanPrice == 0;
	}

	int Inventory::Count(ItemId item) const
	{
		int total = 0;
		for (const ItemStack& slot : m_Slots)
		{
			if (!slot.Empty() && slot.Item == item)
				total += slot.Count;
		}
		return total;
	}

	int Inventory::Room(ItemId item) const
	{
		if (!IsItem(item))
			return 0;
		const int stackMax = GetItemDef(item).StackMax;
		int room = 0;
		for (const ItemStack& slot : m_Slots)
		{
			if (slot.Empty())
				room += stackMax;
			else if (slot.Item == item)
				room += (std::max)(0, stackMax - slot.Count);
		}
		return room;
	}

	bool Inventory::CanAdd(ItemId item, int count) const
	{
		return count <= 0 || Room(item) >= count;
	}

	bool Inventory::CanAddAll(std::span<const ItemStack> stacks) const
	{
		Inventory trial = *this;
		for (const ItemStack& stack : stacks)
		{
			if (stack.Count > 0 && trial.Add(stack.Item, stack.Count) != stack.Count)
				return false;
		}
		return true;
	}

	int Inventory::Add(ItemId item, int count)
	{
		if (!IsItem(item) || count <= 0)
			return 0;
		const int stackMax = GetItemDef(item).StackMax;
		const std::array<int, INVENTORY_SLOTS> order = FillOrder(PrefersHotbar(item));
		int remaining = count;
		for (const int index : order)
		{
			ItemStack& slot = m_Slots[static_cast<size_t>(index)];
			if (remaining == 0)
				break;
			if (!slot.Empty() && slot.Item == item && slot.Count < stackMax)
			{
				const int put = (std::min)(remaining, stackMax - slot.Count);
				slot.Count += put;
				remaining -= put;
			}
		}
		for (const int index : order)
		{
			ItemStack& slot = m_Slots[static_cast<size_t>(index)];
			if (remaining == 0)
				break;
			if (slot.Empty())
			{
				const int put = (std::min)(remaining, stackMax);
				slot = ItemStack{ item, put };
				remaining -= put;
			}
		}
		return count - remaining;
	}

	bool Inventory::Remove(ItemId item, int count)
	{
		if (!IsItem(item) || count < 0 || Count(item) < count)
			return false;
		int remaining = count;
		for (auto it = m_Slots.rbegin(); it != m_Slots.rend() && remaining > 0; ++it)
		{
			if (it->Empty() || it->Item != item)
				continue;
			const int take = (std::min)(remaining, it->Count);
			it->Count -= take;
			remaining -= take;
			if (it->Count == 0)
				*it = ItemStack{};
		}
		return true;
	}

	int Inventory::FreeSlots() const
	{
		return static_cast<int>(std::count_if(m_Slots.begin(), m_Slots.end(), [](const ItemStack& slot) { return slot.Empty(); }));
	}

	const ItemStack& Inventory::GetSlot(int index) const
	{
		static const ItemStack s_Empty;
		return index >= 0 && index < INVENTORY_SLOTS ? m_Slots[static_cast<size_t>(index)] : s_Empty;
	}

	void Inventory::MoveSlot(int from, int to)
	{
		if (from == to || from < 0 || to < 0 || from >= INVENTORY_SLOTS || to >= INVENTORY_SLOTS)
			return;
		ItemStack& source = m_Slots[static_cast<size_t>(from)];
		ItemStack& target = m_Slots[static_cast<size_t>(to)];
		if (!source.Empty() && !target.Empty() && source.Item == target.Item)
		{
			const int put = (std::min)(source.Count, GetItemDef(source.Item).StackMax - target.Count);
			if (put > 0)
			{
				target.Count += put;
				source.Count -= put;
				if (source.Count == 0)
					source = ItemStack{};
			}
			return;
		}
		std::swap(source, target);
	}

	int Inventory::FindSlot(ItemId item) const
	{
		for (int i = 0; i < INVENTORY_SLOTS; ++i)
		{
			if (!m_Slots[static_cast<size_t>(i)].Empty() && m_Slots[static_cast<size_t>(i)].Item == item)
				return i;
		}
		return -1;
	}

	void Inventory::SelectSlot(int slot)
	{
		if (slot >= 0 && slot < HOTBAR_SLOTS)
			m_Selected = slot;
	}

	void Inventory::CycleSelection(int direction)
	{
		if (direction == 0)
			return;
		const int step = direction > 0 ? 1 : -1;
		int slot = m_Selected;
		for (int i = 0; i < HOTBAR_SLOTS; ++i)
		{
			slot = ((slot + step) % HOTBAR_SLOTS + HOTBAR_SLOTS) % HOTBAR_SLOTS;
			if (!m_Slots[static_cast<size_t>(slot)].Empty())
			{
				m_Selected = slot;
				return;
			}
		}
	}

	bool Inventory::SpendCoins(int coins)
	{
		if (coins < 0 || m_Coins < coins)
			return false;
		m_Coins -= coins;
		return true;
	}

	bool Inventory::KnowsPlan(BuildId id) const
	{
		return static_cast<size_t>(id) < m_Plans.size() && m_Plans[static_cast<size_t>(id)];
	}

	void Inventory::LearnPlan(BuildId id)
	{
		if (static_cast<size_t>(id) < m_Plans.size())
			m_Plans[static_cast<size_t>(id)] = true;
	}

	void Inventory::LearnAllPlans()
	{
		m_Plans.fill(true);
	}

	void Inventory::GiveStarterLoadout()
	{
		AddCoins(STARTER_COINS);
		for (const StarterStack& stack : STARTER_LOADOUT)
			Add(stack.Item, stack.Count);
		if (const int axe = FindSlot(ItemId::Axe); axe >= 0)
			SelectSlot(axe);
	}

	uint64_t Inventory::Hash() const
	{
		Fnv1a hash;
		hash.I32(m_Coins);
		hash.I32(m_Selected);
		for (const ItemStack& slot : m_Slots)
		{
			hash.U16(slot.Empty() ? static_cast<uint16_t>(k_NoItem) : static_cast<uint16_t>(slot.Item));
			hash.I32(slot.Empty() ? 0 : slot.Count);
		}
		for (const bool known : m_Plans)
			hash.Byte(known ? 1 : 0);
		return hash.Value();
	}

}
