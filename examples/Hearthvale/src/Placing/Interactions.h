#pragma once
#include "World/TileGrid.h"
#include "World/ValleyLayout.h"

#include <optional>

namespace Dingo
{

	// The landmark whose building or door stands on tile (x, y): a landmark occupant first, else the footprint or door
	// of a landmark's spec.
	std::optional<Landmark> LandmarkAt(const TileGrid& grid, int x, int y);

}
