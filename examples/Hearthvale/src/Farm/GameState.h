#pragma once
#include "Farm/Board.h"
#include "Farm/Economy.h"
#include "Placing/Inventory.h"
#include "ValleyDirector.h"

#include <cstdint>

namespace Dingo
{

	// The player-side state the saves and the determinism checks cover next to the grid: what is carried, the bin and the board.
	struct GameState
	{
		Inventory Pack;
		ShippingBin Bin;
		CommunityBoard Board;
	};

	struct GameStateDigest
	{
		uint64_t Grid = 0;
		uint64_t Pack = 0;
		uint64_t Bin = 0;
		uint64_t Board = 0;
		uint64_t Time = 0;
		uint64_t All = 0;
	};

	GameStateDigest HashGameState(const ValleyDirector& director, const GameState& state);

}
