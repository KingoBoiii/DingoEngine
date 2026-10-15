#pragma once
#include "Defs/BuildDefs.h"
#include "Defs/ItemDefs.h"
#include "Placing/Inventory.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Dingo
{

	enum class TradeResult : uint8_t
	{
		Ok,
		BadCount,
		NotForSale,     // the shop does not stock it, or it has no plan price
		NotSellable,    // no ship price
		CannotAfford,
		NotEnough,      // fewer held than asked
		InventoryFull,  // the purchase or the returned goods do not fit
		AlreadyKnown    // the plan was bought already
	};

	const char* ToString(TradeResult result);

	enum class Merchant : uint8_t { Shopkeeper, Carpenter };
	enum class OfferKind : uint8_t { Item, Plan };

	struct ShopOffer
	{
		Merchant Seller = Merchant::Shopkeeper;
		OfferKind Kind = OfferKind::Item;
		ItemId Item = ItemId::Count;    // Item offers
		BuildId Build = BuildId::Count; // Plan offers
		int Price = 0;
	};

	// The shopkeeper sells every seed plus wood and stone at their buy prices; the carpenter sells the plan of every
	// buildable with a PlanPrice. Fixed order, so a screen and a check read the same list.
	std::vector<ShopOffer> ShopStock(Merchant seller);

	// Coins per item the shop pays at once for goods; 0 when it will not buy. The shipping bin pays full price,
	// but only at the day roll.
	int ShopSellBackPrice(ItemId item);

	// All or nothing: nothing is charged or moved unless the whole order goes through.
	TradeResult ShopBuyItem(Inventory& inventory, ItemId item, int count);
	TradeResult ShopBuyPlan(Inventory& inventory, BuildId build);
	TradeResult ShopSellItem(Inventory& inventory, ItemId item, int count);

	// What one night's shipping paid.
	struct ShippingPayout
	{
		int Coins = 0;
		std::array<int, ITEM_COUNT> Items{};
	};

	// Goods put in the bin wait there; they are paid for, and the bin emptied, only by Settle at the day roll.
	class ShippingBin
	{
	public:
		TradeResult Ship(Inventory& inventory, ItemId item, int count);
		// Takes goods back out before the payout; needs room for them.
		TradeResult Retrieve(Inventory& inventory, ItemId item, int count);

		int Held(ItemId item) const;
		int PendingValue() const;
		bool IsEmpty() const;

		// Pays PendingValue into the inventory, empties the bin and returns what was paid.
		ShippingPayout Settle(Inventory& inventory);

		uint64_t Hash() const;

	private:
		std::array<int, ITEM_COUNT> m_Held{};
	};

}
