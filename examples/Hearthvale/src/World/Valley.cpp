#include "World/Valley.h"
#include "Defs/DecorDefs.h"
#include "GameTuning.h"
#include "Sim/Rng.h"
#include "World/ValleyLayout.h"

#include <algorithm>
#include <array>
#include <span>
#include <vector>

namespace Dingo
{

	namespace
	{
		enum Stream : uint64_t { StreamRim = 1, StreamPond = 2, StreamDecor = 3 };

		struct DecorWeight
		{
			DecorKind Kind;
			int Weight;
		};

		constexpr std::array<DecorWeight, 5> k_RimMix = { { { DecorKind::Tree, 35 }, { DecorKind::Pine, 40 }, { DecorKind::Bush, 10 }, { DecorKind::Rock, 10 }, { DecorKind::Stump, 5 } } };
		constexpr std::array<DecorWeight, 6> k_OvergrownMix = { { { DecorKind::Weed, 35 }, { DecorKind::Rock, 12 }, { DecorKind::Tree, 12 }, { DecorKind::Stump, 11 }, { DecorKind::Bush, 15 }, { DecorKind::Grass, 15 } } };
		constexpr std::array<DecorWeight, 6> k_MeadowMix = { { { DecorKind::Grass, 40 }, { DecorKind::Flower, 25 }, { DecorKind::Bush, 10 }, { DecorKind::Tree, 15 }, { DecorKind::Rock, 5 }, { DecorKind::Stump, 5 } } };
		constexpr std::array<DecorWeight, 3> k_VillageMix = { { { DecorKind::Grass, 55 }, { DecorKind::Flower, 35 }, { DecorKind::Bush, 10 } } };

		DecorKind PickKind(Rng& rng, std::span<const DecorWeight> mix)
		{
			int total = 0;
			for (const DecorWeight& entry : mix)
				total += entry.Weight;
			int roll = rng.NextInt(0, total - 1);
			for (const DecorWeight& entry : mix)
			{
				if (roll < entry.Weight)
					return entry.Kind;
				roll -= entry.Weight;
			}
			return mix.back().Kind;
		}

		std::vector<int> RimDepths(Rng& rng, int length)
		{
			std::vector<int> depths(static_cast<size_t>(length));
			int depth = rng.NextInt(RIM_DEPTH_MIN, RIM_DEPTH_MAX);
			for (int& value : depths)
			{
				depth = std::clamp(depth + rng.NextInt(-1, 1), RIM_DEPTH_MIN, RIM_DEPTH_MAX);
				value = depth;
			}
			return depths;
		}

		void CarveRim(TileGrid& grid, Rng& rng)
		{
			const int width = grid.GetWidth();
			const int height = grid.GetHeight();
			const std::vector<int> west = RimDepths(rng, height);
			const std::vector<int> east = RimDepths(rng, height);
			const std::vector<int> north = RimDepths(rng, width);
			const std::vector<int> south = RimDepths(rng, width);
			for (int y = 0; y < height; ++y)
				for (int x = 0; x < width; ++x)
					if (x < west[static_cast<size_t>(y)] || x >= width - east[static_cast<size_t>(y)]
						|| y < north[static_cast<size_t>(x)] || y >= height - south[static_cast<size_t>(x)])
						grid.SetGround(x, y, Ground::Blocked);
		}

		void FillPath(TileGrid& grid, const TileRect& rect)
		{
			for (int y = rect.Y; y < rect.Bottom(); ++y)
				for (int x = rect.X; x < rect.Right(); ++x)
					grid.SetGround(x, y, Ground::Path);
		}

		void LayPaths(TileGrid& grid)
		{
			FillPath(grid, VILLAGE_SQUARE);
			FillPath(grid, MAIN_ROAD);
			FillPath(grid, { 28, 38, 1, 4 });
			FillPath(grid, { 28, 41, 9, 1 });
			FillPath(grid, { 36, 41, 1, 6 });
			FillPath(grid, { 68, 38, 1, 2 });
			FillPath(grid, { 76, 38, 1, 2 });
			FillPath(grid, { 68, 56, 1, 2 });
			FillPath(grid, { 76, 56, 1, 2 });
			FillPath(grid, { POND_SHORE_X, VILLAGE_SQUARE.Bottom(), 1, POND_SHORE_Y - VILLAGE_SQUARE.Bottom() + 1 });
		}

		void CarvePond(TileGrid& grid, Rng& rng)
		{
			const int rx2 = POND_RADIUS_X * POND_RADIUS_X;
			const int ry2 = POND_RADIUS_Y * POND_RADIUS_Y;
			for (int y = POND_BOUNDS.Y; y < POND_BOUNDS.Bottom(); ++y)
				for (int x = POND_BOUNDS.X; x < POND_BOUNDS.Right(); ++x)
				{
					const int dx = x - POND_CENTER_X;
					const int dy = y - POND_CENTER_Y;
					const int jitter = rng.NextInt(-40, 40);
					if (dx * dx * ry2 + dy * dy * rx2 <= rx2 * ry2 + jitter && grid.GetGround(x, y) == Ground::Grass)
						grid.SetGround(x, y, Ground::Water);
				}
		}

		void PlaceLandmarks(TileGrid& grid)
		{
			for (const LandmarkSpec& spec : k_Landmarks)
				grid.AddOccupantUnchecked(OccupantKind::Landmark, static_cast<uint16_t>(spec.Id),
					spec.Footprint.X, spec.Footprint.Y, spec.Footprint.W, spec.Footprint.H, 0, 0);
		}

		void ScatterDecoration(TileGrid& grid, Rng& rng)
		{
			for (int y = 0; y < grid.GetHeight(); ++y)
				for (int x = 0; x < grid.GetWidth(); ++x)
				{
					const Tile& tile = grid.At(x, y);
					if (tile.Occupant != k_NoOccupant || FARM_PLOT.Contains(x, y))
						continue;
					if (tile.GroundState != Ground::Grass && tile.GroundState != Ground::Blocked)
						continue;

					int permille = DECOR_MEADOW_PERMILLE;
					std::span<const DecorWeight> mix = k_MeadowMix;
					if (tile.GroundState == Ground::Blocked)
					{
						permille = DECOR_RIM_PERMILLE;
						mix = k_RimMix;
					}
					else if (FARM_CLEARING.Contains(x, y))
					{
						permille = DECOR_OVERGROWN_PERMILLE;
						mix = k_OvergrownMix;
					}
					else if (VILLAGE_ZONE.Contains(x, y))
					{
						permille = DECOR_VILLAGE_PERMILLE;
						mix = k_VillageMix;
					}

					if (!rng.Chance(permille))
						continue;

					const DecorKind kind = PickKind(rng, mix);
					const int rotation = rng.NextInt(0, 3);
					const int variant = rng.NextInt(0, DECOR_VARIANTS - 1);
					grid.AddOccupantUnchecked(OccupantKind::Decoration, static_cast<uint16_t>(kind), x, y, 1, 1, rotation, variant);
				}
		}
	}

	void GenerateValley(TileGrid& grid, uint32_t seed)
	{
		Rng rimRng(HashMix(seed, StreamRim));
		Rng pondRng(HashMix(seed, StreamPond));
		Rng decorRng(HashMix(seed, StreamDecor));

		CarveRim(grid, rimRng);
		LayPaths(grid);
		CarvePond(grid, pondRng);
		PlaceLandmarks(grid);
		ScatterDecoration(grid, decorRng);
	}

}
