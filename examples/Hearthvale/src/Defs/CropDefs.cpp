#include "Defs/CropDefs.h"

#include <array>

namespace Dingo
{

	namespace
	{
		constexpr std::array<CropDef, CROP_COUNT> k_Crops = { {
			{ CropId::Parsnip,    "Parsnip",    Season::Spring, 4,  0, 35,  20, 1 },
			{ CropId::Radish,     "Radish",     Season::Spring, 6,  0, 60,  30, 1 },
			{ CropId::Strawberry, "Strawberry", Season::Spring, 8,  4, 55,  40, 1 },
			{ CropId::Tomato,     "Tomato",     Season::Summer, 8,  4, 45,  30, 1 },
			{ CropId::Melon,      "Melon",      Season::Summer, 10, 0, 150, 80, 1 },
			{ CropId::Sunflower,  "Sunflower",  Season::Summer, 8,  0, 90,  50, 1 },
			{ CropId::Pumpkin,    "Pumpkin",    Season::Fall,   10, 0, 160, 90, 1 },
			{ CropId::Cabbage,    "Cabbage",    Season::Fall,   6,  0, 70,  40, 1 },
			{ CropId::Beetroot,   "Beetroot",   Season::Fall,   5,  0, 55,  30, 1 },
			{ CropId::Kale,       "Kale",       Season::Winter, 5,  0, 60,  30, 1 },
			{ CropId::Leek,       "Leek",       Season::Winter, 6,  0, 75,  40, 1 },
			{ CropId::Turnip,     "Turnip",     Season::Winter, 4,  0, 40,  20, 1 },
		} };
	}

	std::span<const CropDef> GetCropDefs()
	{
		return k_Crops;
	}

	const CropDef& GetCropDef(CropId id)
	{
		return k_Crops[static_cast<size_t>(id)];
	}

	int CropStageFor(const CropDef& def, int daysWatered)
	{
		if (daysWatered >= def.GrowthDays)
			return 3;
		if (daysWatered <= 0)
			return 0;
		const int span = def.GrowthDays;
		const int stage = 1 + (daysWatered * 2) / span;
		return stage > 2 ? 2 : stage;
	}

}
