#include "Checks/Checks.h"
#include "Defs/BoardDefs.h"
#include "Defs/BuildDefs.h"
#include "Defs/CropDefs.h"
#include "Defs/DecorDefs.h"
#include "Defs/ItemDefs.h"
#include "Defs/VillagerDefs.h"

#include <set>
#include <string>

namespace Dingo
{

	namespace
	{
		bool ValidItem(ItemId id) { return static_cast<int>(id) < ITEM_COUNT; }

		void CheckCrops(CheckReport& report)
		{
			int perSeason[SEASON_COUNT] = {};
			int regrowing = 0;
			bool tableOrder = GetCropDefs().size() == static_cast<size_t>(CROP_COUNT);
			bool prices = true;
			bool growth = true;
			bool stages = true;
			for (const CropDef& crop : GetCropDefs())
			{
				++perSeason[static_cast<int>(crop.GrowSeason)];
				regrowing += crop.RegrowDays > 0 ? 1 : 0;
				tableOrder = tableOrder && &GetCropDef(crop.Id) == &crop;
				prices = prices && crop.SellPrice > crop.SeedPrice && crop.SeedPrice > 0 && crop.HarvestYield > 0;
				growth = growth && crop.GrowthDays > 0 && crop.GrowthDays <= DAYS_PER_SEASON && crop.RegrowDays >= 0 && crop.GrowthDays + crop.RegrowDays <= DAYS_PER_SEASON;
				stages = stages && CropStageFor(crop, 0) == 0 && CropStageFor(crop, crop.GrowthDays) == 3 && CropStageFor(crop, crop.GrowthDays - 1) < 3;
				for (int d = 1; d <= crop.GrowthDays; ++d)
					stages = stages && CropStageFor(crop, d) >= CropStageFor(crop, d - 1);
			}
			report.Check(CROP_COUNT == 12 && tableOrder, "Defs: 12 crops, indexed by CropId");
			report.Check(perSeason[0] == 3 && perSeason[1] == 3 && perSeason[2] == 3 && perSeason[3] == 3, "Defs: three crops a season");
			report.Check(regrowing == 2, "Defs: two regrowing crops");
			report.Check(prices, "Defs: every crop sells for more than its seed and costs something");
			report.Check(growth, "Defs: every crop ripens within a season");
			report.Check(stages, "Defs: a crop is ripe on its growth day and not a day before, and stages never go back");
		}

		void CheckItems(CheckReport& report)
		{
			bool named = true;
			bool idsMatch = true;
			bool links = true;
			bool sellable = true;
			for (int i = 0; i < ITEM_COUNT; ++i)
			{
				const ItemDef& item = GetItemDef(static_cast<ItemId>(i));
				named = named && item.Name && item.Name[0] != '\0' && item.StackMax >= 1;
				idsMatch = idsMatch && static_cast<int>(item.Id) == i;
				sellable = sellable && item.SellPrice >= 0 && item.BuyPrice >= 0;
			}
			for (const CropDef& crop : GetCropDefs())
			{
				const ItemDef& seed = GetItemDef(SeedItem(crop.Id));
				const ItemDef& harvest = GetItemDef(HarvestItem(crop.Id));
				links = links && seed.Kind == ItemKind::Seed && seed.Crop == crop.Id && seed.BuyPrice == crop.SeedPrice
					&& harvest.Kind == ItemKind::Harvest && harvest.Crop == crop.Id && harvest.SellPrice == crop.SellPrice;
			}
			report.Check(named && idsMatch, "Defs: every item has its own id, a name and a stack size");
			report.Check(links, "Defs: seed and harvest items carry their crop's prices");
			report.Check(sellable && GetItemDef(ItemId::Egg).SellPrice > 0 && GetItemDef(ItemId::Milk).SellPrice > 0 && GetItemDef(ItemId::Wood).SellPrice > 0,
				"Defs: item prices are not negative and products sell");
		}

		void CheckBuildables(CheckReport& report)
		{
			std::set<std::string> names;
			bool sane = GetBuildDefs().size() == static_cast<size_t>(BUILD_COUNT);
			bool costs = true;
			bool sprinklers = GetBuildDef(BuildId::Sprinkler).WaterRadius == 1 && GetBuildDef(BuildId::QualitySprinkler).WaterRadius == 2;
			for (const BuildDef& def : GetBuildDefs())
			{
				names.insert(def.Name);
				sane = sane && &GetBuildDef(def.Id) == &def && def.Width >= 1 && def.Height >= 1 && def.RefundPermille >= 0 && def.RefundPermille <= 1000 && def.PlanPrice >= 0;
				costs = costs && (def.Cost.Coins + def.Cost.Wood + def.Cost.Stone + def.Cost.Fibre) > 0;
			}
			report.Check(BUILD_COUNT == 20 && names.size() == 20 && sane, "Defs: 20 buildables with unique names, indexed by BuildId");
			report.Check(costs, "Defs: every buildable costs something");
			report.Check(sprinklers, "Defs: the sprinkler waters 3x3 and the quality sprinkler 5x5");

			const BuildDef& barn = GetBuildDef(BuildId::Barn);
			const Footprint turned = RotatedFootprint(barn, 1);
			const Footprint half = RotatedFootprint(barn, 2);
			report.Check(turned.W == barn.Height && turned.H == barn.Width && half.W == barn.Width && half.H == barn.Height, "Defs: a quarter turn swaps the footprint, a half turn keeps it");
		}

		void CheckPeople(CheckReport& report)
		{
			bool villagers = GetVillagerDefs().size() == static_cast<size_t>(VILLAGER_COUNT);
			for (const VillagerDef& villager : GetVillagerDefs())
			{
				int liked = 0;
				for (const ItemId item : villager.Liked)
					if (ValidItem(item))
						++liked;
				villagers = villagers && &GetVillagerDef(villager.Id) == &villager && villager.Name[0] != '\0' && liked >= 3
					&& GetLandmarkSpec(villager.Home).HasDoor;
			}
			report.Check(VILLAGER_COUNT == 4 && villagers, "Defs: 4 villagers, each with a home landmark that has a door and 3+ liked gifts");

			bool boards = true;
			int requests = 0;
			for (int s = 0; s < SEASON_COUNT; ++s)
			{
				const BoardDef& board = GetBoardDef(static_cast<Season>(s));
				boards = boards && board.BoardSeason == static_cast<Season>(s);
				for (const BoardRequest& request : board.Requests)
				{
					++requests;
					boards = boards && request.Text[0] != '\0' && request.Count > 0 && request.RewardCoins > 0;
					switch (request.Kind)
					{
						case RequestKind::Ship:      boards = boards && ValidItem(request.Item) && GetItemDef(request.Item).SellPrice > 0; break;
						case RequestKind::Build:     boards = boards && static_cast<int>(request.Build) < BUILD_COUNT; break;
						case RequestKind::Gift:      boards = boards && ValidItem(request.Item) && static_cast<int>(request.Villager) < VILLAGER_COUNT; break;
						case RequestKind::EarnCoins: break;
					}
				}
			}
			report.Check(requests == 16 && boards, "Defs: four boards of four valid requests, one board a season");

			bool decor = GetDecorDefs().size() == static_cast<size_t>(DECOR_KIND_COUNT);
			for (const DecorDef& def : GetDecorDefs())
				decor = decor && &GetDecorDef(def.Kind) == &def && (def.Cosmetic || (ValidItem(def.Tool) && ValidItem(def.Yield) && def.YieldCount > 0));
			report.Check(decor, "Defs: every non-cosmetic decoration names the tool that clears it and its yield");
		}
	}

	void RunDefChecks(CheckReport& report)
	{
		SuiteScope scope(report, "Defs");
		CheckCrops(report);
		CheckItems(report);
		CheckBuildables(report);
		CheckPeople(report);
	}

}
