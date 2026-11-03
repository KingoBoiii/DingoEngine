#pragma once
#include "Defs/CropDefs.h"

#include <cstdint>
#include <span>

namespace Dingo
{

	// Layout: tools, resources, products, then one seed and one harvest item per crop, in CropId order.
	enum class ItemId : uint16_t
	{
		Hoe, WateringCan, Axe, Pickaxe, Scythe,
		Wood, Stone, Fibre,
		Egg, Milk, Honey,
		SeedFirst,
		HarvestFirst = SeedFirst + CROP_COUNT,
		Count = HarvestFirst + CROP_COUNT
	};

	inline constexpr uint16_t k_NoItem = 0xFFFF;
	inline constexpr int ITEM_COUNT = static_cast<int>(ItemId::Count);

	constexpr ItemId SeedItem(CropId crop) { return static_cast<ItemId>(static_cast<int>(ItemId::SeedFirst) + static_cast<int>(crop)); }
	constexpr ItemId HarvestItem(CropId crop) { return static_cast<ItemId>(static_cast<int>(ItemId::HarvestFirst) + static_cast<int>(crop)); }

	enum class ItemKind : uint8_t { Tool, Resource, Product, Seed, Harvest };

	struct ItemDef
	{
		ItemId Id;
		ItemKind Kind;
		const char* Name;
		int StackMax;
		int SellPrice;      // 0 = cannot be shipped
		int BuyPrice;       // 0 = not sold in the shop
		CropId Crop;        // seeds and harvests only
	};

	// Indexed by ItemId; seeds and harvests are derived from the crop table.
	const ItemDef& GetItemDef(ItemId id);

}
