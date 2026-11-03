#pragma once
#include "Defs/BuildDefs.h"
#include "Defs/ItemDefs.h"
#include "Placing/Inventory.h"
#include "Sim/SimTypes.h"
#include "World/TileGrid.h"
#include "World/ValleyLayout.h"

#include <cstdint>

namespace Dingo
{

	enum class FarmResult : uint8_t
	{
		Ok,
		OutOfBounds,
		BadGround,      // not soil that can be tilled or planted
		Occupied,       // something other than cosmetic decoration or a greenhouse stands here
		NotTilled,
		AlreadyDone,    // already tilled, or already watered
		HasCrop,
		NoCrop,
		NotRipe,
		WrongSeason,
		NoSeed,
		InventoryFull   // a harvest that does not fit: the crop stays
	};

	const char* ToString(FarmResult result);

	struct FarmOutcome
	{
		FarmResult Result = FarmResult::Ok;
		TileRect Dirty;                 // every tile the edit touched; empty when nothing changed
		CropId Crop = CropId::Count;
		ItemId Yield = ItemId::Count;
		int YieldCount = 0;
	};

	// A greenhouse footprint keeps its crops out of the season and snow rules.
	bool IsGreenhouseTile(const TileGrid& grid, int x, int y);

	bool IsRipe(const CropRecord& crop);

	// Whether `crop` may be planted and kept on a tile in `season`.
	bool CanGrowIn(const CropDef& def, Season season, bool greenhouse);

	// Grass becomes tilled soil, and cosmetic decoration on it goes for free. A greenhouse floor can be tilled too.
	// In the rain, outdoor soil is tilled already watered.
	FarmOutcome TillTile(TileGrid& grid, int x, int y, Weather weather);

	// Tilled soil becomes watered; the colour is all that changes.
	FarmOutcome WaterTile(TileGrid& grid, int x, int y);

	// Takes one seed and plants it in empty tilled soil, if the crop grows in `season` or the tile is in a greenhouse.
	FarmOutcome PlantSeed(TileGrid& grid, Inventory& inventory, int day, Season season, int x, int y, CropId crop);

	// A ripe crop gives its yield. A regrowing crop stays and is ripe again after its RegrowDays watered days. A yield
	// that does not fit the inventory changes nothing (InventoryFull).
	FarmOutcome HarvestTile(TileGrid& grid, Inventory& inventory, int x, int y);

	// First seed (starting at `preferred`, in crop order) held in `inventory` that grows in `season`; CropId::Count if none.
	CropId PickSeed(const Inventory& inventory, CropId preferred, Season season);

	struct GrowReport
	{
		int Grown = 0;
		int Dry = 0;            // growing crops that got no water
		int Snowed = 0;         // crops outside a greenhouse held back by snow
		int Withered = 0;
	};

	// The day roll's crop step for the day that is ending (`endingDay`, with `weather` that day). A crop grows one
	// day when its tile is watered, or it is outdoors in the rain; snow stops outdoor growth. Then every crop out
	// of season on the next day withers, except in a greenhouse. Does not dry the soil.
	GrowReport GrowCrops(TileGrid& grid, int endingDay, Weather weather);

	// Watered becomes tilled again; returns how many tiles dried.
	int DrySoil(TileGrid& grid);

	// Dawn: every tilled tile within a sprinkler's radius (Chebyshev) becomes watered; returns how many.
	int WaterWithSprinklers(TileGrid& grid);

	// Rain at dawn: every outdoor tilled tile becomes watered; returns how many.
	int WaterOutdoorSoil(TileGrid& grid);

}
