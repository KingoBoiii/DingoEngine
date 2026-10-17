#pragma once
#include "World/TileGrid.h"

#include <cstdint>

namespace Dingo
{

	// Fills a fresh VALLEY_SIZE grid: rim, farm clearing, village with its paths, pond, landmarks, decoration.
	// The result depends only on `seed`.
	void GenerateValley(TileGrid& grid, uint32_t seed);

}
