#include "Checks/Checks.h"
#include "Defs/DecorDefs.h"
#include "GameTuning.h"
#include "World/TileGrid.h"
#include "World/Valley.h"
#include "World/ValleyLayout.h"

#include <array>
#include <format>
#include <queue>
#include <vector>

namespace Dingo
{

	namespace
	{
		TileGrid MakeValley(uint32_t seed)
		{
			TileGrid grid(VALLEY_SIZE, VALLEY_SIZE);
			GenerateValley(grid, seed);
			return grid;
		}

		bool DoorsReachable(const TileGrid& grid)
		{
			const LandmarkSpec& start = GetLandmarkSpec(Landmark::FarmHouse);
			std::vector<uint8_t> seen(static_cast<size_t>(grid.GetWidth()) * static_cast<size_t>(grid.GetHeight()), 0);
			std::queue<std::pair<int, int>> open;
			const auto push = [&](int x, int y)
			{
				if (!grid.InBounds(x, y) || grid.GetGround(x, y) != Ground::Path)
					return;
				uint8_t& flag = seen[static_cast<size_t>(y) * static_cast<size_t>(grid.GetWidth()) + static_cast<size_t>(x)];
				if (flag)
					return;
				flag = 1;
				open.push({ x, y });
			};
			push(start.DoorX, start.DoorY);
			while (!open.empty())
			{
				const auto [x, y] = open.front();
				open.pop();
				push(x + 1, y);
				push(x - 1, y);
				push(x, y + 1);
				push(x, y - 1);
			}

			const auto reached = [&](int x, int y)
			{
				return seen[static_cast<size_t>(y) * static_cast<size_t>(grid.GetWidth()) + static_cast<size_t>(x)] != 0;
			};
			for (const LandmarkSpec& spec : k_Landmarks)
				if (spec.HasDoor && !reached(spec.DoorX, spec.DoorY))
					return false;
			return reached(POND_SHORE_X, POND_SHORE_Y);
		}

		void CheckValley(CheckReport& report, uint32_t seed)
		{
			const TileGrid grid = MakeValley(seed);
			const std::string tag = std::format("seed {}", seed);

			bool plotClear = true;
			for (int y = FARM_PLOT.Y; y < FARM_PLOT.Bottom(); ++y)
				for (int x = FARM_PLOT.X; x < FARM_PLOT.Right(); ++x)
				{
					const Tile& tile = grid.At(x, y);
					plotClear = plotClear && tile.GroundState == Ground::Grass && tile.Occupant == k_NoOccupant && !tile.Crop.Present();
				}
			report.Check(plotClear, std::format("Generation: the 12x12 farm plot is bare grass ({})", tag));

			bool decorOk = true;
			bool decorOffPaths = true;
			bool decorOffWater = true;
			int decorCount = 0;
			int clearingDecor = 0;
			for (const Occupant& occupant : grid.GetOccupants())
			{
				if (occupant.Kind != OccupantKind::Decoration)
					continue;
				++decorCount;
				const Ground ground = grid.GetGround(occupant.X, occupant.Y);
				decorOffPaths = decorOffPaths && ground != Ground::Path;
				decorOffWater = decorOffWater && ground != Ground::Water;
				decorOk = decorOk && !FARM_PLOT.Contains(occupant.X, occupant.Y);
				if (FARM_CLEARING.Contains(occupant.X, occupant.Y))
					++clearingDecor;
			}
			report.Check(decorOk, std::format("Generation: no decoration on the farm plot ({})", tag));
			report.Check(decorOffPaths, std::format("Generation: no decoration on a path ({})", tag));
			report.Check(decorOffWater, std::format("Generation: no decoration in the pond ({})", tag));
			report.Check(decorCount >= DECOR_COUNT_MIN && decorCount <= DECOR_COUNT_MAX,
				std::format("Generation: decoration count {} is within {}..{} ({})", decorCount, DECOR_COUNT_MIN, DECOR_COUNT_MAX, tag));
			report.Check(clearingDecor > 300, std::format("Generation: the 40x40 clearing is overgrown ({} objects, {})", clearingDecor, tag));

			bool squarePaved = true;
			for (int y = VILLAGE_SQUARE.Y; y < VILLAGE_SQUARE.Bottom(); ++y)
				for (int x = VILLAGE_SQUARE.X; x < VILLAGE_SQUARE.Right(); ++x)
					squarePaved = squarePaved && grid.GetGround(x, y) == Ground::Path;
			bool roadPaved = true;
			for (int y = MAIN_ROAD.Y; y < MAIN_ROAD.Bottom(); ++y)
				for (int x = MAIN_ROAD.X; x < MAIN_ROAD.Right(); ++x)
					roadPaved = roadPaved && grid.GetGround(x, y) == Ground::Path;
			report.Check(squarePaved && roadPaved, std::format("Generation: the square and the main road are paths ({})", tag));
			report.Check(DoorsReachable(grid), std::format("Generation: every door and the pond shore are reachable by path from the farmhouse ({})", tag));

			const int water = grid.CountGround(Ground::Water);
			report.Check(water >= 40 && grid.GetGround(POND_CENTER_X, POND_CENTER_Y) == Ground::Water,
				std::format("Generation: the pond exists ({} water tiles, {})", water, tag));

			bool landmarksOk = grid.CountOccupants(OccupantKind::Landmark) == LANDMARK_COUNT;
			for (const LandmarkSpec& spec : k_Landmarks)
			{
				const Tile& corner = grid.At(spec.Footprint.X, spec.Footprint.Y);
				const Occupant* occupant = grid.FindOccupant(corner.Occupant);
				landmarksOk = landmarksOk && occupant && occupant->Kind == OccupantKind::Landmark && occupant->DefId == static_cast<uint16_t>(spec.Id)
					&& CanOccupy(corner.GroundState);
			}
			report.Check(landmarksOk, std::format("Generation: every landmark is placed on usable ground ({})", tag));

			bool rimOk = true;
			for (int i = 0; i < VALLEY_SIZE; ++i)
				rimOk = rimOk && grid.GetGround(i, 0) == Ground::Blocked && grid.GetGround(i, VALLEY_SIZE - 1) == Ground::Blocked
					&& grid.GetGround(0, i) == Ground::Blocked && grid.GetGround(VALLEY_SIZE - 1, i) == Ground::Blocked;
			bool clearingFree = true;
			for (int y = FARM_CLEARING.Y; y < FARM_CLEARING.Bottom(); ++y)
				for (int x = FARM_CLEARING.X; x < FARM_CLEARING.Right(); ++x)
					clearingFree = clearingFree && grid.GetGround(x, y) != Ground::Blocked;
			report.Check(rimOk && clearingFree, std::format("Generation: wooded rim blocks the edge and spares the clearing ({})", tag));
		}

		void CheckGridRules(CheckReport& report)
		{
			TileGrid grid(8, 8);
			grid.SetGround(5, 5, Ground::Water);
			grid.SetGround(6, 0, Ground::Blocked);

			const uint32_t a = grid.AddOccupant(OccupantKind::Building, 1, 1, 1, 2, 2, 0, 0);
			report.Check(a != k_NoOccupant && grid.At(2, 2).Occupant == a, "Grid: a 2x2 building claims all four tiles");
			report.Check(grid.AddOccupant(OccupantKind::Building, 1, 2, 2, 1, 1, 0, 0) == k_NoOccupant, "Grid: placing on an occupied tile fails");
			report.Check(grid.AddOccupant(OccupantKind::Building, 1, 0, 1, 2, 1, 0, 0) == k_NoOccupant && grid.At(0, 1).Occupant == k_NoOccupant,
				"Grid: a footprint overlapping one occupied tile fails and claims nothing");
			report.Check(grid.AddOccupant(OccupantKind::Building, 1, 5, 5, 1, 1, 0, 0) == k_NoOccupant, "Grid: placing on water fails");
			report.Check(grid.AddOccupant(OccupantKind::Building, 1, 6, 0, 1, 1, 0, 0) == k_NoOccupant, "Grid: placing on the blocked rim fails");
			report.Check(grid.AddOccupant(OccupantKind::Building, 1, 7, 7, 2, 1, 0, 0) == k_NoOccupant, "Grid: placing out of bounds fails");
			report.Check(grid.AddOccupant(OccupantKind::Building, 1, -1, 3, 1, 1, 0, 0) == k_NoOccupant, "Grid: placing at a negative coordinate fails");

			grid.At(4, 4).Crop.Def = static_cast<uint8_t>(CropId::Parsnip);
			report.Check(grid.AddOccupant(OccupantKind::Building, 1, 4, 4, 1, 1, 0, 0) == k_NoOccupant, "Grid: placing on a crop fails");

			const uint64_t before = grid.Hash();
			report.Check(grid.RemoveOccupant(a) && grid.At(1, 1).Occupant == k_NoOccupant && grid.At(2, 2).Occupant == k_NoOccupant && !grid.FindOccupant(a),
				"Grid: removing frees every tile of the footprint");
			report.Check(grid.Hash() != before, "Grid: the hash changes when an occupant is removed");
			report.Check(!grid.RemoveOccupant(a), "Grid: removing twice fails");

			const uint32_t b = grid.AddOccupant(OccupantKind::Building, 1, 1, 1, 1, 1, 0, 0);
			report.Check(b != a, "Grid: occupant ids are never reused");
		}

		void CheckHash(CheckReport& report)
		{
			TileGrid empty(4, 4);
			report.Check(empty.Hash() == 0x70F76B4CC90685D5ull, std::format("Grid: the hash of an empty 4x4 grid is the platform-independent constant (got {:016X})", empty.Hash()));

			TileGrid grid = MakeValley(1);
			const uint64_t original = grid.Hash();
			const TileGrid copy = grid;
			report.Check(copy.Hash() == original, "Grid: a copy hashes the same");
			grid.SetGround(FARM_PLOT.X, FARM_PLOT.Y, Ground::Tilled);
			report.Check(grid.Hash() != original, "Grid: changing one tile's ground changes the hash");
			grid.SetGround(FARM_PLOT.X, FARM_PLOT.Y, Ground::Grass);
			report.Check(grid.Hash() == original, "Grid: restoring the tile restores the hash");
			grid.At(FARM_PLOT.X, FARM_PLOT.Y).Crop.DaysWatered = 1;
			report.Check(grid.Hash() != original, "Grid: a crop record is part of the hash");
		}
	}

	void RunGenerationChecks(CheckReport& report)
	{
		SuiteScope scope(report, "Generation");

		const uint64_t first = MakeValley(1).Hash();
		report.Check(first == MakeValley(1).Hash(), "Generation: the same seed gives the same valley hash (seed 1)");
		report.Check(MakeValley(987654321u).Hash() == MakeValley(987654321u).Hash(), "Generation: the same seed gives the same valley hash (seed 987654321)");

		const uint64_t second = MakeValley(2).Hash();
		const uint64_t third = MakeValley(3).Hash();
		report.Check(first != second && first != third && second != third, "Generation: different seeds give different valleys");

		for (const uint32_t seed : { 1u, 2u, 3u, 4242u })
			CheckValley(report, seed);

		CheckGridRules(report);
		CheckHash(report);
	}

}
