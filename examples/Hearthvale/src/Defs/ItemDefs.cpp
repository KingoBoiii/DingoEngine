#include "Defs/ItemDefs.h"

#include <array>
#include <string>

namespace Dingo
{

	namespace
	{
		struct ItemTable
		{
			std::array<ItemDef, ITEM_COUNT> Items{};
			std::array<std::string, CROP_COUNT * 2> Names;

			ItemTable()
			{
				const auto set = [this](ItemId id, ItemKind kind, const char* name, int stack, int sell, int buy)
				{
					Items[static_cast<size_t>(id)] = ItemDef{ id, kind, name, stack, sell, buy, CropId::Count };
				};

				set(ItemId::Hoe,         ItemKind::Tool, "Hoe",          1, 0, 0);
				set(ItemId::WateringCan, ItemKind::Tool, "Watering Can", 1, 0, 0);
				set(ItemId::Axe,         ItemKind::Tool, "Axe",          1, 0, 0);
				set(ItemId::Pickaxe,     ItemKind::Tool, "Pickaxe",      1, 0, 0);
				set(ItemId::Scythe,      ItemKind::Tool, "Scythe",       1, 0, 0);

				set(ItemId::Wood,  ItemKind::Resource, "Wood",  99, 2, 5);
				set(ItemId::Stone, ItemKind::Resource, "Stone", 99, 3, 8);
				set(ItemId::Fibre, ItemKind::Resource, "Fibre", 99, 1, 0);

				set(ItemId::Egg,   ItemKind::Product, "Egg",   99, 30,  0);
				set(ItemId::Milk,  ItemKind::Product, "Milk",  99, 60,  0);
				set(ItemId::Honey, ItemKind::Product, "Honey", 99, 100, 0);

				for (const CropDef& crop : GetCropDefs())
				{
					const size_t index = static_cast<size_t>(crop.Id);
					Names[index] = std::string(crop.Name) + " Seeds";
					Names[CROP_COUNT + index] = crop.Name;

					const ItemId seed = SeedItem(crop.Id);
					Items[static_cast<size_t>(seed)] = ItemDef{ seed, ItemKind::Seed, Names[index].c_str(), 99, 0, crop.SeedPrice, crop.Id };
					const ItemId harvest = HarvestItem(crop.Id);
					Items[static_cast<size_t>(harvest)] = ItemDef{ harvest, ItemKind::Harvest, Names[CROP_COUNT + index].c_str(), 99, crop.SellPrice, 0, crop.Id };
				}
			}
		};

		const ItemTable& Table()
		{
			static const ItemTable s_Table;
			return s_Table;
		}
	}

	const ItemDef& GetItemDef(ItemId id)
	{
		return Table().Items[static_cast<size_t>(id)];
	}

}
