#pragma once
#include "GameTuning.h"

#include <array>
#include <cstdint>

namespace Dingo
{

	struct TileRect
	{
		int X = 0;
		int Y = 0;
		int W = 0;
		int H = 0;

		constexpr int Right() const { return X + W; }
		constexpr int Bottom() const { return Y + H; }
		constexpr bool Contains(int x, int y) const { return x >= X && x < X + W && y >= Y && y < Y + H; }
	};

	enum class Landmark : uint8_t { FarmHouse, ShippingBin, Shop, Carpenter, MayorHouse, Cottage, NoticeBoard, Count };

	struct LandmarkSpec
	{
		Landmark Id;
		const char* Name;
		TileRect Footprint;
		bool HasDoor;
		int DoorX;
		int DoorY;
	};

	inline constexpr int LANDMARK_COUNT = static_cast<int>(Landmark::Count);

	inline constexpr TileRect FARM_CLEARING   { 10, 28, 40, 40 };
	inline constexpr TileRect FARM_PLOT       { 24, 42, 12, 12 };
	inline constexpr TileRect VILLAGE_ZONE    { 52, 28, 36, 54 };
	inline constexpr TileRect VILLAGE_SQUARE  { 64, 40, 16, 16 };

	// The main road: two rows from the plot's east edge to the square.
	inline constexpr TileRect MAIN_ROAD       { 36, 47, 28, 2 };

	inline constexpr int POND_CENTER_X = 76;
	inline constexpr int POND_CENTER_Y = 74;
	inline constexpr int POND_RADIUS_X = 6;
	inline constexpr int POND_RADIUS_Y = 4;
	inline constexpr TileRect POND_BOUNDS     { POND_CENTER_X - POND_RADIUS_X - 1, POND_CENTER_Y - POND_RADIUS_Y - 1, 2 * POND_RADIUS_X + 3, 2 * POND_RADIUS_Y + 3 };
	inline constexpr int POND_SHORE_X = 79;
	inline constexpr int POND_SHORE_Y = 68;

	inline constexpr std::array<LandmarkSpec, LANDMARK_COUNT> k_Landmarks = { {
		{ Landmark::FarmHouse,   "Farmhouse",    { 26, 34, 4, 4 }, true,  28, 38 },
		{ Landmark::ShippingBin, "Shipping bin", { 32, 40, 2, 1 }, true,  32, 41 },
		{ Landmark::Shop,        "Shop",         { 66, 34, 4, 4 }, true,  68, 38 },
		{ Landmark::Carpenter,   "Carpenter",    { 74, 34, 4, 4 }, true,  76, 38 },
		{ Landmark::MayorHouse,  "Mayor's house",{ 66, 58, 4, 4 }, true,  68, 57 },
		{ Landmark::Cottage,     "Cottage",      { 74, 58, 4, 4 }, true,  76, 57 },
		{ Landmark::NoticeBoard, "Notice board", { 71, 41, 2, 1 }, true,  71, 42 },
	} };

	constexpr const LandmarkSpec& GetLandmarkSpec(Landmark id) { return k_Landmarks[static_cast<size_t>(id)]; }

}
