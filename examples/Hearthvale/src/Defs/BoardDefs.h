#pragma once
#include "Defs/BuildDefs.h"
#include "Defs/ItemDefs.h"
#include "Defs/VillagerDefs.h"
#include "Sim/SimTypes.h"

#include <array>
#include <cstdint>

namespace Dingo
{

	enum class RequestKind : uint8_t { Ship, Build, Gift, EarnCoins };

	inline constexpr int BOARD_REQUESTS = 4;
	inline constexpr int SEASON_BOARD_COUNT = 4;

	struct BoardRequest
	{
		RequestKind Kind;
		const char* Text;
		ItemId Item;            // Ship, Gift
		BuildId Build;          // Build
		VillagerId Villager;    // Gift
		int Count;              // items, buildings or coins
		bool InSquare;          // Build: only counts inside the village square
		int RewardCoins;
	};

	struct BoardDef
	{
		Season BoardSeason;
		std::array<BoardRequest, BOARD_REQUESTS> Requests;
	};

	const BoardDef& GetBoardDef(Season season);

}
