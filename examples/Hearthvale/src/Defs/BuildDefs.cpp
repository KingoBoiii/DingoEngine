#include "Defs/BuildDefs.h"
#include "GameTuning.h"

#include <array>

namespace Dingo
{

	namespace
	{
		constexpr int R = BUILD_REFUND_PERMILLE;

		constexpr std::array<BuildDef, BUILD_COUNT> k_Builds = { {
			{ BuildId::WoodFence,        "Wood Fence",        BuildCategory::Fence,    1, 1, { 0,   2,  0, 0 }, R, 0,    false, true,  0 },
			{ BuildId::StoneFence,       "Stone Fence",       BuildCategory::Fence,    1, 1, { 0,   0,  2, 0 }, R, 150,  false, true,  0 },
			{ BuildId::Gate,             "Gate",              BuildCategory::Fence,    1, 1, { 10,  4,  0, 0 }, R, 0,    false, false, 0 },
			{ BuildId::DirtPath,         "Dirt Path",         BuildCategory::Path,     1, 1, { 0,   0,  0, 1 }, R, 0,    true,  false, 0 },
			{ BuildId::StonePath,        "Stone Path",        BuildCategory::Path,     1, 1, { 0,   0,  1, 0 }, R, 100,  true,  false, 0 },
			{ BuildId::Scarecrow,        "Scarecrow",         BuildCategory::Farm,     1, 1, { 50,  4,  0, 4 }, R, 0,    false, true,  0 },
			{ BuildId::Sprinkler,        "Sprinkler",         BuildCategory::Farm,     1, 1, { 200, 0,  6, 0 }, R, 250,  false, true,  1 },
			{ BuildId::QualitySprinkler, "Quality Sprinkler", BuildCategory::Farm,     1, 1, { 600, 0, 12, 0 }, R, 600,  false, true,  2 },
			{ BuildId::Chest,            "Chest",             BuildCategory::Storage,  1, 1, { 0,   10, 0, 0 }, R, 0,    false, true,  0 },
			{ BuildId::LampPost,         "Lamp Post",         BuildCategory::Light,    1, 1, { 40,  4,  2, 0 }, R, 0,    false, true,  0 },
			{ BuildId::Bench,            "Bench",             BuildCategory::Decor,    2, 1, { 0,   8,  0, 0 }, R, 100,  false, true,  0 },
			{ BuildId::FlowerBed,        "Flower Bed",        BuildCategory::Decor,    1, 1, { 20,  2,  2, 0 }, R, 150,  false, false, 0 },
			{ BuildId::Well,             "Well",              BuildCategory::Farm,     1, 1, { 100, 0, 20, 0 }, R, 200,  false, true,  0 },
			{ BuildId::Beehive,          "Beehive",           BuildCategory::Farm,     1, 1, { 150, 10, 0, 0 }, R, 400,  false, true,  0 },
			{ BuildId::Coop,             "Coop",              BuildCategory::Building, 3, 2, { 400, 30, 10, 0 }, R, 500, false, true,  0 },
			{ BuildId::Barn,             "Barn",              BuildCategory::Building, 4, 3, { 900, 60, 30, 0 }, R, 1200, false, true, 0 },
			{ BuildId::Greenhouse,       "Greenhouse",        BuildCategory::Building, 4, 4, { 1500, 40, 40, 0 }, R, 2000, false, false, 0 },
			{ BuildId::Mailbox,          "Mailbox",           BuildCategory::Decor,    1, 1, { 30,  4,  0, 0 }, R, 0,    false, true,  0 },
			{ BuildId::Sign,             "Sign",              BuildCategory::Decor,    1, 1, { 0,   3,  0, 0 }, R, 0,    false, true,  0 },
			{ BuildId::FestivalLantern,  "Festival Lantern",  BuildCategory::Light,    1, 1, { 60,  2,  0, 6 }, R, 300,  false, true,  0 },
		} };
	}

	std::span<const BuildDef> GetBuildDefs()
	{
		return k_Builds;
	}

	const BuildDef& GetBuildDef(BuildId id)
	{
		return k_Builds[static_cast<size_t>(id)];
	}

	Footprint RotatedFootprint(const BuildDef& def, int rotation)
	{
		return (rotation & 1) ? Footprint{ def.Height, def.Width } : Footprint{ def.Width, def.Height };
	}

}
