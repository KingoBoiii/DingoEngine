#include "Farm/DemoLayout.h"
#include "Defs/BuildDefs.h"
#include "Defs/CropDefs.h"
#include "Defs/ItemDefs.h"
#include "Farm/Crops.h"
#include "Placing/Placement.h"
#include "World/ValleyLayout.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>

namespace Dingo
{

	namespace
	{
		constexpr int PLOT_CROP_FIRST_ROW = FARM_PLOT.Y + 1;
		constexpr int PLOT_CROP_ROWS = FARM_PLOT.H - 2;
		constexpr int PLOT_GEAR_NORTH = FARM_PLOT.Y;
		constexpr int PLOT_GEAR_SOUTH = FARM_PLOT.Bottom() - 1;
		constexpr int RING_WEST = FARM_PLOT.X - 1;
		constexpr int RING_EAST = FARM_PLOT.Right();
		constexpr int RING_NORTH = FARM_PLOT.Y - 1;
		constexpr int RING_SOUTH = FARM_PLOT.Bottom();
		constexpr int CLEARING_MARGIN = 3;

		struct Builder
		{
			TileGrid& Grid;
			Inventory Scratch;
			std::array<bool, BUILD_COUNT> Used{};
			std::vector<TileRect> Taken;
			DemoLayoutResult& Result;

			Builder(TileGrid& grid, DemoLayoutResult& result)
				: Grid(grid), Result(result)
			{
				Scratch.LearnAllPlans();
				Scratch.AddCoins(1000000);
			}

			bool Place(BuildId id, int x, int y)
			{
				const BuildCost& cost = GetBuildDef(id).Cost;
				Scratch.Add(ItemId::Wood, cost.Wood);
				Scratch.Add(ItemId::Stone, cost.Stone);
				Scratch.Add(ItemId::Fibre, cost.Fibre);
				if (PlaceBuilding(Grid, Scratch, PlaceSpec{ id, x, y, 0 }).Result != PlaceResult::Ok)
					return false;
				if (!Used[static_cast<size_t>(id)])
				{
					Used[static_cast<size_t>(id)] = true;
					++Result.BuildKinds;
				}
				++Result.Buildings;
				return true;
			}
		};

		bool NearDoor(int x, int y)
		{
			for (int i = 0; i < LANDMARK_COUNT; ++i)
			{
				const LandmarkSpec& spec = GetLandmarkSpec(static_cast<Landmark>(i));
				if (spec.HasDoor && std::abs(x - spec.DoorX) <= 1 && std::abs(y - spec.DoorY) <= 1)
					return true;
			}
			return false;
		}

		bool Overlaps(const TileRect& a, const TileRect& b)
		{
			return a.X < b.Right() && b.X < a.Right() && a.Y < b.Bottom() && b.Y < a.Bottom();
		}

		void PlantPlot(TileGrid& grid, DemoLayoutResult& result, int day)
		{
			for (int column = 0; column < FARM_PLOT.W; ++column)
			{
				const CropId crop = static_cast<CropId>(column % CROP_COUNT);
				const CropDef& def = GetCropDef(crop);
				bool planted = false;
				for (int row = 0; row < PLOT_CROP_ROWS; ++row)
				{
					const int x = FARM_PLOT.X + column;
					const int y = PLOT_CROP_FIRST_ROW + row;
					(void)TillTile(grid, x, y, Weather::Clear);
					Tile& tile = grid.At(x, y);
					if (tile.GroundState != Ground::Tilled && tile.GroundState != Ground::Watered)
						continue;
					tile.GroundState = row % 2 == 0 ? Ground::Watered : Ground::Tilled;
					const int days = row * def.GrowthDays / (PLOT_CROP_ROWS - 1);
					tile.Crop = CropRecord{ static_cast<uint8_t>(crop), static_cast<uint8_t>(CropStageFor(def, days)), static_cast<uint8_t>(days), day };
					++result.Crops;
					planted = true;
				}
				if (planted)
					++result.CropKinds;
			}
		}

		void FencePlot(Builder& builder)
		{
			const int gateSouth = FARM_PLOT.X + FARM_PLOT.W / 2;
			const int gateEast = MAIN_ROAD.Y;
			for (int x = RING_WEST; x <= RING_EAST; ++x)
			{
				for (const int y : { RING_NORTH, RING_SOUTH })
				{
					if (NearDoor(x, y))
						continue;
					builder.Place(y == RING_SOUTH && x == gateSouth ? BuildId::Gate : BuildId::WoodFence, x, y);
				}
			}
			for (int y = RING_NORTH + 1; y < RING_SOUTH; ++y)
			{
				for (const int x : { RING_WEST, RING_EAST })
				{
					if (NearDoor(x, y) || (x == RING_EAST && y == gateEast + 1))
						continue;
					builder.Place(x == RING_EAST && y == gateEast ? BuildId::Gate : BuildId::StoneFence, x, y);
				}
			}
		}

		void EquipPlot(Builder& builder)
		{
			builder.Place(BuildId::Scarecrow, FARM_PLOT.X, PLOT_GEAR_NORTH);
			builder.Place(BuildId::Sprinkler, FARM_PLOT.X + 3, PLOT_GEAR_NORTH);
			builder.Place(BuildId::QualitySprinkler, FARM_PLOT.X + 6, PLOT_GEAR_NORTH);
			builder.Place(BuildId::Sprinkler, FARM_PLOT.X + 9, PLOT_GEAR_NORTH);
			builder.Place(BuildId::Chest, FARM_PLOT.Right() - 1, PLOT_GEAR_SOUTH);
			builder.Place(BuildId::Sprinkler, FARM_PLOT.X + 4, PLOT_GEAR_SOUTH);
			builder.Place(BuildId::Sprinkler, FARM_PLOT.X + 8, PLOT_GEAR_SOUTH);
			for (const glm::ivec2& spot : { glm::ivec2(RING_WEST - 1, RING_NORTH), glm::ivec2(RING_EAST + 1, RING_NORTH),
				glm::ivec2(RING_WEST - 1, RING_SOUTH), glm::ivec2(RING_EAST + 1, RING_SOUTH) })
				builder.Place(BuildId::LampPost, spot.x, spot.y);
		}

		void LayPaths(Builder& builder)
		{
			const LandmarkSpec& home = GetLandmarkSpec(Landmark::FarmHouse);
			for (int y = home.DoorY + 1; y < RING_NORTH; ++y)
				builder.Place(BuildId::StonePath, home.DoorX, y);
			for (int x = home.DoorX + 1; x < GetLandmarkSpec(Landmark::ShippingBin).DoorX; ++x)
				builder.Place(BuildId::DirtPath, x, home.DoorY + 1);
		}

		bool FindSpot(const Builder& builder, BuildId id, int& outX, int& outY)
		{
			const BuildDef& def = GetBuildDef(id);
			const TileRect plot{ FARM_PLOT.X - CLEARING_MARGIN, FARM_PLOT.Y - CLEARING_MARGIN, FARM_PLOT.W + 2 * CLEARING_MARGIN, FARM_PLOT.H + 2 * CLEARING_MARGIN };
			for (int y = FARM_CLEARING.Y + 1; y + def.Height < FARM_CLEARING.Bottom(); ++y)
			{
				for (int x = FARM_CLEARING.X + 1; x + def.Width < FARM_CLEARING.Right(); ++x)
				{
					const TileRect footprint{ x - 1, y - 1, def.Width + 2, def.Height + 2 };
					if (Overlaps(footprint, plot) || Overlaps(footprint, MAIN_ROAD))
						continue;
					bool blocked = false;
					for (int dy = 0; dy < footprint.H && !blocked; ++dy)
					{
						for (int dx = 0; dx < footprint.W && !blocked; ++dx)
							blocked = NearDoor(footprint.X + dx, footprint.Y + dy);
					}
					for (const TileRect& taken : builder.Taken)
						blocked = blocked || Overlaps(footprint, taken);
					if (blocked || CheckPlacement(builder.Grid, builder.Scratch, PlaceSpec{ id, x, y, 0 }) != PlaceResult::Ok)
						continue;
					outX = x;
					outY = y;
					return true;
				}
			}
			return false;
		}
	}

	DemoLayoutResult ApplyDemoLayout(TileGrid& grid, Inventory& pack, int day)
	{
		DemoLayoutResult result;
		pack.LearnAllPlans();

		PlantPlot(grid, result, day);
		Builder builder(grid, result);
		FencePlot(builder);
		EquipPlot(builder);
		LayPaths(builder);

		static constexpr BuildId k_Scattered[] = { BuildId::Greenhouse, BuildId::Barn, BuildId::Coop, BuildId::Well, BuildId::Beehive, BuildId::Bench,
			BuildId::FlowerBed, BuildId::Mailbox, BuildId::Sign, BuildId::FestivalLantern, BuildId::LampPost, BuildId::LampPost, BuildId::Chest };
		for (const BuildId id : k_Scattered)
		{
			int x = 0;
			int y = 0;
			if (!FindSpot(builder, id, x, y) || !builder.Place(id, x, y))
				continue;
			const BuildDef& def = GetBuildDef(id);
			builder.Taken.push_back(TileRect{ x - 1, y - 1, def.Width + 2, def.Height + 2 });
		}
		return result;
	}

}
