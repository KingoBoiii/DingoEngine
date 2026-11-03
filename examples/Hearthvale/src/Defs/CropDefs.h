#pragma once
#include "Sim/SimTypes.h"

#include <cstdint>
#include <span>

namespace Dingo
{

	enum class CropId : uint8_t
	{
		Parsnip, Radish, Strawberry,
		Tomato, Melon, Sunflower,
		Pumpkin, Cabbage, Beetroot,
		Kale, Leek, Turnip,
		Count
	};

	inline constexpr uint8_t k_NoCrop = 0xFF;
	inline constexpr int CROP_COUNT = static_cast<int>(CropId::Count);
	inline constexpr int CROP_STAGES = 4;

	struct CropDef
	{
		CropId Id;
		const char* Name;
		Season GrowSeason;
		int GrowthDays;     // watered days from planting to ripe
		int RegrowDays;     // 0 = single harvest; otherwise watered days from a harvest back to ripe
		int SellPrice;
		int SeedPrice;
		int HarvestYield;
	};

	std::span<const CropDef> GetCropDefs();
	const CropDef& GetCropDef(CropId id);

	// 0 = seed, 1 = sprout, 2 = young, 3 = ripe.
	int CropStageFor(const CropDef& def, int daysWatered);

}
