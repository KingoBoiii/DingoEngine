#pragma once
#include "Farm/Animals.h"
#include "Farm/Board.h"
#include "Farm/Economy.h"
#include "Farm/Friendship.h"
#include "Placing/Inventory.h"
#include "ValleyDirector.h"

#include <cstdint>

namespace Dingo
{

	// The player-side state the saves and the determinism checks cover next to the grid: what is carried, the bin, the board, friendships and the herd.
	struct GameState
	{
		Inventory Pack;
		ShippingBin Bin;
		CommunityBoard Board;
		Friendship Bonds;
		Herd Animals;
	};

	struct GameStateDigest
	{
		uint64_t Grid = 0;
		uint64_t Pack = 0;
		uint64_t Bin = 0;
		uint64_t Board = 0;
		uint64_t Bonds = 0;
		uint64_t Animals = 0;
		uint64_t Time = 0;
		uint64_t All = 0;
	};

	GameStateDigest HashGameState(const ValleyDirector& director, const GameState& state);

}
