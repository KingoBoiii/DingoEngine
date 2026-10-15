#pragma once
#include <cstdint>
#include <span>

namespace Dingo
{

	enum class BuildId : uint8_t
	{
		WoodFence, StoneFence, Gate, DirtPath, StonePath,
		Scarecrow, Sprinkler, QualitySprinkler, Chest, LampPost,
		Bench, FlowerBed, Well, Beehive, Coop,
		Barn, Greenhouse, Mailbox, Sign, FestivalLantern,
		Count
	};

	inline constexpr int BUILD_COUNT = static_cast<int>(BuildId::Count);

	enum class BuildCategory : uint8_t { Fence, Path, Farm, Light, Decor, Storage, Building };

	struct BuildCost
	{
		int Coins = 0;
		int Wood = 0;
		int Stone = 0;
		int Fibre = 0;
	};

	struct BuildDef
	{
		BuildId Id;
		const char* Name;
		BuildCategory Category;
		int Width;
		int Height;
		BuildCost Cost;
		int RefundPermille;
		int PlanPrice;          // coins at the carpenter; 0 = known from the start
		bool GroundOnly;        // sets the ground to Path instead of adding an occupant
		bool Blocking;          // walkers cannot pass
		int WaterRadius;        // sprinklers: water every tile within this Chebyshev radius at dawn
	};

	std::span<const BuildDef> GetBuildDefs();
	const BuildDef& GetBuildDef(BuildId id);

	// Footprint size after `rotation` quarter turns.
	struct Footprint
	{
		int W;
		int H;
	};
	Footprint RotatedFootprint(const BuildDef& def, int rotation);

}
