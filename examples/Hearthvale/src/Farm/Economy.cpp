#include "Farm/Economy.h"
#include "GameTuning.h"
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
	}

	const char* ToString(TradeResult result)
	{
		switch (result)
		{
			case TradeResult::Ok:            return "Ok";
			case TradeResult::BadCount:      return "Bad amount";
			case TradeResult::NotForSale:    return "Not for sale";
			case TradeResult::NotSellable:   return "Cannot be sold";
			case TradeResult::CannotAfford:  return "Not enough coins";
			case TradeResult::NotEnough:     return "Not enough items";
			case TradeResult::InventoryFull: return "Inventory full";
			case TradeResult::AlreadyKnown:  return "Plan already known";
		}
		return "";
	}

	std::vector<ShopOffer> ShopStock(Merchant seller)
	{
		std::vector<ShopOffer> stock;
		if (seller == Merchant::Shopkeeper)
		{
			for (int i = 0; i < ITEM_COUNT; ++i)
			{
				const ItemDef& def = GetItemDef(static_cast<ItemId>(i));
				if (def.BuyPrice > 0)
					stock.push_back(ShopOffer{ Merchant::Shopkeeper, OfferKind::Item, def.Id, BuildId::Count, def.BuyPrice });
			}
		}
		else
		{
			for (const BuildDef& def : GetBuildDefs())
			{
				if (def.PlanPrice > 0)
					stock.push_back(ShopOffer{ Merchant::Carpenter, OfferKind::Plan, ItemId::Count, def.Id, def.PlanPrice });
			}
		}
		return stock;
	}

	int ShopSellBackPrice(ItemId item)
	{
		if (!IsItem(item))
			return 0;
		const int ship = GetItemDef(item).SellPrice;
		return ship > 0 ? (std::max)(1, ship * SHOP_SELL_BACK_PERMILLE / 1000) : 0;
	}

	TradeResult ShopBuyItem(Inventory& inventory, ItemId item, int count)
	{
		if (count <= 0)
			return TradeResult::BadCount;
		if (!IsItem(item) || GetItemDef(item).BuyPrice <= 0)
			return TradeResult::NotForSale;
		const int total = GetItemDef(item).BuyPrice * count;
		if (inventory.GetCoins() < total)
			return TradeResult::CannotAfford;
		if (!inventory.CanAdd(item, count))
			return TradeResult::InventoryFull;
		inventory.SpendCoins(total);
		inventory.Add(item, count);
		return TradeResult::Ok;
	}

	TradeResult ShopBuyPlan(Inventory& inventory, BuildId build)
	{
		if (build >= BuildId::Count || GetBuildDef(build).PlanPrice <= 0)
			return TradeResult::NotForSale;
		if (inventory.KnowsPlan(build))
			return TradeResult::AlreadyKnown;
		if (!inventory.SpendCoins(GetBuildDef(build).PlanPrice))
			return TradeResult::CannotAfford;
		inventory.LearnPlan(build);
		return TradeResult::Ok;
	}

	TradeResult ShopSellItem(Inventory& inventory, ItemId item, int count)
	{
		if (count <= 0)
			return TradeResult::BadCount;
		const int price = ShopSellBackPrice(item);
		if (price <= 0)
			return TradeResult::NotSellable;
		if (!inventory.Remove(item, count))
			return TradeResult::NotEnough;
		inventory.AddCoins(price * count);
		return TradeResult::Ok;
	}

	TradeResult ShippingBin::Ship(Inventory& inventory, ItemId item, int count)
	{
		if (count <= 0)
			return TradeResult::BadCount;
		if (!IsItem(item) || GetItemDef(item).SellPrice <= 0)
			return TradeResult::NotSellable;
		if (!inventory.Remove(item, count))
			return TradeResult::NotEnough;
		m_Held[static_cast<size_t>(item)] += count;
		return TradeResult::Ok;
	}

	TradeResult ShippingBin::Retrieve(Inventory& inventory, ItemId item, int count)
	{
		if (count <= 0)
			return TradeResult::BadCount;
		if (!IsItem(item) || m_Held[static_cast<size_t>(item)] < count)
			return TradeResult::NotEnough;
		if (!inventory.CanAdd(item, count))
			return TradeResult::InventoryFull;
		inventory.Add(item, count);
		m_Held[static_cast<size_t>(item)] -= count;
		return TradeResult::Ok;
	}

	int ShippingBin::Held(ItemId item) const
	{
		return IsItem(item) ? m_Held[static_cast<size_t>(item)] : 0;
	}

	int ShippingBin::PendingValue() const
	{
		int total = 0;
		for (int i = 0; i < ITEM_COUNT; ++i)
			total += m_Held[static_cast<size_t>(i)] * GetItemDef(static_cast<ItemId>(i)).SellPrice;
		return total;
	}

	bool ShippingBin::IsEmpty() const
	{
		return std::all_of(m_Held.begin(), m_Held.end(), [](int count) { return count == 0; });
	}

	ShippingPayout ShippingBin::Settle(Inventory& inventory)
	{
		ShippingPayout payout;
		payout.Coins = PendingValue();
		payout.Items = m_Held;
		inventory.AddCoins(payout.Coins);
		m_Held.fill(0);
		return payout;
	}

	uint64_t ShippingBin::Hash() const
	{
		Fnv1a hash;
		for (int i = 0; i < ITEM_COUNT; ++i)
		{
			if (m_Held[static_cast<size_t>(i)] != 0)
			{
				hash.U16(static_cast<uint16_t>(i));
				hash.I32(m_Held[static_cast<size_t>(i)]);
			}
		}
		return hash.Value();
	}

}
