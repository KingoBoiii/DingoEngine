#include "Defs/VillagerDefs.h"

namespace Dingo
{

	namespace
	{
		constexpr ItemId N = ItemId::Count;

		constexpr std::array<VillagerDef, VILLAGER_COUNT> k_Villagers = { {
			{ VillagerId::Shopkeeper, "Mira",   "Shopkeeper", Landmark::Shop,       { HarvestItem(CropId::Strawberry), HarvestItem(CropId::Tomato), ItemId::Honey, N } },
			{ VillagerId::Carpenter,  "Bram",   "Carpenter",  Landmark::Carpenter,  { ItemId::Wood, HarvestItem(CropId::Pumpkin), ItemId::Milk, N } },
			{ VillagerId::Mayor,      "Odette", "Mayor",      Landmark::MayorHouse, { HarvestItem(CropId::Cabbage), HarvestItem(CropId::Leek), ItemId::Egg, N } },
			{ VillagerId::Child,      "Pip",    "Child",      Landmark::Cottage,    { HarvestItem(CropId::Sunflower), HarvestItem(CropId::Strawberry), HarvestItem(CropId::Melon), ItemId::Honey } },
		} };
	}

	std::span<const VillagerDef> GetVillagerDefs()
	{
		return k_Villagers;
	}

	const VillagerDef& GetVillagerDef(VillagerId id)
	{
		return k_Villagers[static_cast<size_t>(id)];
	}

}
