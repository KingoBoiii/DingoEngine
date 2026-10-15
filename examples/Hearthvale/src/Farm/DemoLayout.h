#pragma once
#include "Placing/Inventory.h"
#include "World/TileGrid.h"

namespace Dingo
{

	struct DemoLayoutResult
	{
		int Buildings = 0;
		int BuildKinds = 0;
		int Crops = 0;
		int CropKinds = 0;
	};

	// A late-game farm for looking at and measuring: the plot planted with every crop at every stage of growth and
	// fenced round with a gate, sprinklers and a scarecrow on its edges, lamps, a short path, and one of every other
	// buildable standing in the clearing. The pack learns every plan; its stock and coins are left alone. Crops are
	// written straight into the tiles, so a kind that is out of season may wither at the next day roll.
	DemoLayoutResult ApplyDemoLayout(TileGrid& grid, Inventory& pack, int day);

}
