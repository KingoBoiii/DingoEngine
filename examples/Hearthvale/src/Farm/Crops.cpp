#include "Farm/Crops.h"
#include "Farm/Calendar.h"
#include "Defs/DecorDefs.h"

#include <algorithm>

namespace Dingo
{

	namespace
	{
		TileRect Tile1(int x, int y)
		{
			return TileRect{ x, y, 1, 1 };
		}

		bool IsSoilGround(Ground ground)
		{
			return ground == Ground::Tilled || ground == Ground::Watered;
		}
	}

	const char* ToString(FarmResult result)
	{
		switch (result)
		{
			case FarmResult::Ok:          return "Ok";
			case FarmResult::OutOfBounds: return "Out of bounds";
			case FarmResult::BadGround:   return "Nothing grows here";
			case FarmResult::Occupied:    return "Something is in the way";
			case FarmResult::NotTilled:   return "The soil needs tilling first";
			case FarmResult::AlreadyDone: return "Already done";
			case FarmResult::HasCrop:     return "Something is growing here";
			case FarmResult::NoCrop:      return "No crop here";
			case FarmResult::NotRipe:     return "Not ripe yet";
			case FarmResult::WrongSeason: return "Wrong season for this seed";
			case FarmResult::NoSeed:      return "No seeds";
			case FarmResult::InventoryFull: return "Inventory full";
		}
		return "";
	}

	bool IsGreenhouseTile(const TileGrid& grid, int x, int y)
	{
		if (!grid.InBounds(x, y))
			return false;
		const Occupant* occupant = grid.FindOccupant(grid.At(x, y).Occupant);
		return occupant && occupant->Kind == OccupantKind::Building && occupant->DefId == static_cast<uint16_t>(BuildId::Greenhouse);
	}

	bool IsRipe(const CropRecord& crop)
	{
		return crop.Present() && crop.DaysWatered >= GetCropDef(static_cast<CropId>(crop.Def)).GrowthDays;
	}

	bool CanGrowIn(const CropDef& def, Season season, bool greenhouse)
	{
		return greenhouse || def.GrowSeason == season;
	}

	FarmOutcome TillTile(TileGrid& grid, int x, int y, Weather weather)
	{
		FarmOutcome outcome;
		if (!grid.InBounds(x, y))
		{
			outcome.Result = FarmResult::OutOfBounds;
			return outcome;
		}

		Tile& tile = grid.At(x, y);
		if (IsSoilGround(tile.GroundState))
		{
			outcome.Result = FarmResult::AlreadyDone;
			return outcome;
		}
		if (tile.GroundState != Ground::Grass)
		{
			outcome.Result = FarmResult::BadGround;
			return outcome;
		}

		const bool greenhouse = IsGreenhouseTile(grid, x, y);
		outcome.Dirty = Tile1(x, y);
		if (tile.Occupant != k_NoOccupant && !greenhouse)
		{
			const Occupant* occupant = grid.FindOccupant(tile.Occupant);
			const bool cosmetic = occupant && occupant->Kind == OccupantKind::Decoration && occupant->DefId < DECOR_KIND_COUNT
				&& GetDecorDef(static_cast<DecorKind>(occupant->DefId)).Cosmetic;
			if (!cosmetic)
			{
				outcome.Result = FarmResult::Occupied;
				outcome.Dirty = TileRect{};
				return outcome;
			}
			outcome.Dirty = TileRect{ occupant->X, occupant->Y, occupant->W, occupant->H };
			grid.RemoveOccupant(tile.Occupant);
		}

		tile.GroundState = (weather == Weather::Rain && !greenhouse) ? Ground::Watered : Ground::Tilled;
		return outcome;
	}

	FarmOutcome WaterTile(TileGrid& grid, int x, int y)
	{
		FarmOutcome outcome;
		if (!grid.InBounds(x, y))
		{
			outcome.Result = FarmResult::OutOfBounds;
			return outcome;
		}

		Tile& tile = grid.At(x, y);
		if (tile.GroundState == Ground::Watered)
		{
			outcome.Result = FarmResult::AlreadyDone;
			return outcome;
		}
		if (tile.GroundState != Ground::Tilled)
		{
			outcome.Result = FarmResult::NotTilled;
			return outcome;
		}
		tile.GroundState = Ground::Watered;
		outcome.Dirty = Tile1(x, y);
		return outcome;
	}

	FarmOutcome PlantSeed(TileGrid& grid, Inventory& inventory, int day, Season season, int x, int y, CropId crop)
	{
		FarmOutcome outcome;
		outcome.Crop = crop;
		if (!grid.InBounds(x, y))
		{
			outcome.Result = FarmResult::OutOfBounds;
			return outcome;
		}
		if (crop >= CropId::Count)
		{
			outcome.Result = FarmResult::NoSeed;
			return outcome;
		}

		Tile& tile = grid.At(x, y);
		if (!IsSoilGround(tile.GroundState))
		{
			outcome.Result = FarmResult::NotTilled;
			return outcome;
		}
		if (tile.Crop.Present())
		{
			outcome.Result = FarmResult::HasCrop;
			return outcome;
		}
		const bool greenhouse = IsGreenhouseTile(grid, x, y);
		if (tile.Occupant != k_NoOccupant && !greenhouse)
		{
			outcome.Result = FarmResult::Occupied;
			return outcome;
		}
		if (!CanGrowIn(GetCropDef(crop), season, greenhouse))
		{
			outcome.Result = FarmResult::WrongSeason;
			return outcome;
		}
		if (!inventory.Remove(SeedItem(crop), 1))
		{
			outcome.Result = FarmResult::NoSeed;
			return outcome;
		}

		tile.Crop = CropRecord{ static_cast<uint8_t>(crop), 0, 0, day };
		outcome.Dirty = Tile1(x, y);
		return outcome;
	}

	FarmOutcome HarvestTile(TileGrid& grid, Inventory& inventory, int x, int y)
	{
		FarmOutcome outcome;
		if (!grid.InBounds(x, y))
		{
			outcome.Result = FarmResult::OutOfBounds;
			return outcome;
		}

		Tile& tile = grid.At(x, y);
		if (!tile.Crop.Present())
		{
			outcome.Result = FarmResult::NoCrop;
			return outcome;
		}
		if (!IsRipe(tile.Crop))
		{
			outcome.Result = FarmResult::NotRipe;
			return outcome;
		}

		const CropDef& def = GetCropDef(static_cast<CropId>(tile.Crop.Def));
		outcome.Crop = def.Id;
		outcome.Yield = HarvestItem(def.Id);
		outcome.YieldCount = def.HarvestYield;
		if (!inventory.CanAdd(outcome.Yield, outcome.YieldCount))
		{
			outcome.Result = FarmResult::InventoryFull;
			return outcome;
		}
		inventory.Add(outcome.Yield, outcome.YieldCount);
		outcome.Dirty = Tile1(x, y);

		if (def.RegrowDays > 0)
		{
			tile.Crop.DaysWatered = static_cast<uint8_t>(def.GrowthDays - def.RegrowDays);
			tile.Crop.Stage = static_cast<uint8_t>(CropStageFor(def, tile.Crop.DaysWatered));
		}
		else
		{
			tile.Crop = CropRecord{};
		}
		return outcome;
	}

	CropId PickSeed(const Inventory& inventory, CropId preferred, Season season)
	{
		const int first = preferred < CropId::Count ? static_cast<int>(preferred) : 0;
		for (int i = 0; i < CROP_COUNT; ++i)
		{
			const CropId crop = static_cast<CropId>((first + i) % CROP_COUNT);
			if (inventory.Count(SeedItem(crop)) > 0 && GetCropDef(crop).GrowSeason == season)
				return crop;
		}
		return CropId::Count;
	}

	GrowReport GrowCrops(TileGrid& grid, int endingDay, Weather weather)
	{
		GrowReport report;
		const Season nextSeason = Calendar::SeasonOfDay(endingDay + 1);

		for (int y = 0; y < grid.GetHeight(); ++y)
		{
			for (int x = 0; x < grid.GetWidth(); ++x)
			{
				Tile& tile = grid.At(x, y);
				if (!tile.Crop.Present())
					continue;

				const CropDef& def = GetCropDef(static_cast<CropId>(tile.Crop.Def));
				const bool greenhouse = IsGreenhouseTile(grid, x, y);

				const bool watered = tile.GroundState == Ground::Watered || (weather == Weather::Rain && !greenhouse);
				if (weather == Weather::Snow && !greenhouse)
				{
					++report.Snowed;
				}
				else if (!watered)
				{
					++report.Dry;
				}
				else if (tile.Crop.DaysWatered < def.GrowthDays)
				{
					++tile.Crop.DaysWatered;
					tile.Crop.Stage = static_cast<uint8_t>(CropStageFor(def, tile.Crop.DaysWatered));
					++report.Grown;
				}

				if (!CanGrowIn(def, nextSeason, greenhouse))
				{
					tile.Crop = CropRecord{};
					++report.Withered;
				}
			}
		}
		return report;
	}

	int DrySoil(TileGrid& grid)
	{
		int dried = 0;
		for (int y = 0; y < grid.GetHeight(); ++y)
		{
			for (int x = 0; x < grid.GetWidth(); ++x)
			{
				Tile& tile = grid.At(x, y);
				if (tile.GroundState == Ground::Watered)
				{
					tile.GroundState = Ground::Tilled;
					++dried;
				}
			}
		}
		return dried;
	}

	int WaterWithSprinklers(TileGrid& grid)
	{
		int watered = 0;
		for (const Occupant& occupant : grid.GetOccupants())
		{
			if (occupant.Kind != OccupantKind::Building || occupant.DefId >= BUILD_COUNT)
				continue;
			const int radius = GetBuildDef(static_cast<BuildId>(occupant.DefId)).WaterRadius;
			if (radius <= 0)
				continue;

			const int x0 = (std::max)(0, occupant.X - radius);
			const int y0 = (std::max)(0, occupant.Y - radius);
			const int x1 = (std::min)(grid.GetWidth() - 1, occupant.X + occupant.W - 1 + radius);
			const int y1 = (std::min)(grid.GetHeight() - 1, occupant.Y + occupant.H - 1 + radius);
			for (int y = y0; y <= y1; ++y)
			{
				for (int x = x0; x <= x1; ++x)
				{
					Tile& tile = grid.At(x, y);
					if (tile.GroundState == Ground::Tilled)
					{
						tile.GroundState = Ground::Watered;
						++watered;
					}
				}
			}
		}
		return watered;
	}

	int WaterOutdoorSoil(TileGrid& grid)
	{
		int watered = 0;
		for (int y = 0; y < grid.GetHeight(); ++y)
		{
			for (int x = 0; x < grid.GetWidth(); ++x)
			{
				Tile& tile = grid.At(x, y);
				if (tile.GroundState == Ground::Tilled && !IsGreenhouseTile(grid, x, y))
				{
					tile.GroundState = Ground::Watered;
					++watered;
				}
			}
		}
		return watered;
	}

}
