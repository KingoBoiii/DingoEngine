#include "Defs/BoardDefs.h"

namespace Dingo
{

	namespace
	{
		constexpr ItemId NoItem = ItemId::Count;
		constexpr BuildId NoBuild = BuildId::Count;
		constexpr VillagerId NoVillager = VillagerId::Count;

		constexpr BoardRequest Ship(const char* text, ItemId item, int count, int reward)
		{
			return { RequestKind::Ship, text, item, NoBuild, NoVillager, count, false, reward };
		}
		constexpr BoardRequest Build(const char* text, BuildId build, int count, bool inSquare, int reward)
		{
			return { RequestKind::Build, text, NoItem, build, NoVillager, count, inSquare, reward };
		}
		constexpr BoardRequest Gift(const char* text, ItemId item, VillagerId villager, int reward)
		{
			return { RequestKind::Gift, text, item, NoBuild, villager, 1, false, reward };
		}
		constexpr BoardRequest Earn(const char* text, int coins, int reward)
		{
			return { RequestKind::EarnCoins, text, NoItem, NoBuild, NoVillager, coins, false, reward };
		}

		constexpr std::array<BoardDef, SEASON_BOARD_COUNT> k_Boards = { {
			{ Season::Spring, { {
				Ship("Ship 20 parsnips", HarvestItem(CropId::Parsnip), 20, 300),
				Build("Build 10 wood fences", BuildId::WoodFence, 10, false, 200),
				Gift("Give the child a strawberry", HarvestItem(CropId::Strawberry), VillagerId::Child, 150),
				Earn("Earn 1500 coins from shipping", 1500, 400),
			} } },
			{ Season::Summer, { {
				Ship("Ship 15 tomatoes", HarvestItem(CropId::Tomato), 15, 350),
				Build("Build 3 lamp posts in the square", BuildId::LampPost, 3, true, 300),
				Gift("Give the child a sunflower", HarvestItem(CropId::Sunflower), VillagerId::Child, 150),
				Ship("Ship 5 melons", HarvestItem(CropId::Melon), 5, 400),
			} } },
			{ Season::Fall, { {
				Ship("Ship 10 pumpkins", HarvestItem(CropId::Pumpkin), 10, 500),
				Build("Build a greenhouse", BuildId::Greenhouse, 1, false, 600),
				Gift("Give the mayor a cabbage", HarvestItem(CropId::Cabbage), VillagerId::Mayor, 150),
				Build("Build 2 benches in the square", BuildId::Bench, 2, true, 250),
			} } },
			{ Season::Winter, { {
				Ship("Ship 10 kale", HarvestItem(CropId::Kale), 10, 500),
				Build("Build 4 festival lanterns in the square", BuildId::FestivalLantern, 4, true, 500),
				Gift("Give the shopkeeper some honey", ItemId::Honey, VillagerId::Shopkeeper, 200),
				Build("Build a barn", BuildId::Barn, 1, false, 800),
			} } },
		} };
	}

	const BoardDef& GetBoardDef(Season season)
	{
		return k_Boards[static_cast<size_t>(season)];
	}

}
