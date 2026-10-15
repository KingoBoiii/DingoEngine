#include "Placing/Interactions.h"

namespace Dingo
{

	std::optional<Landmark> LandmarkAt(const TileGrid& grid, int x, int y)
	{
		if (!grid.InBounds(x, y))
			return std::nullopt;

		const Tile& tile = grid.At(x, y);
		if (tile.Occupant != k_NoOccupant)
		{
			const Occupant* occupant = grid.FindOccupant(tile.Occupant);
			if (occupant && occupant->Kind == OccupantKind::Landmark && occupant->DefId < LANDMARK_COUNT)
				return static_cast<Landmark>(occupant->DefId);
		}

		for (const LandmarkSpec& spec : k_Landmarks)
		{
			if (spec.Footprint.Contains(x, y) || (spec.HasDoor && spec.DoorX == x && spec.DoorY == y))
				return spec.Id;
		}
		return std::nullopt;
	}

}
