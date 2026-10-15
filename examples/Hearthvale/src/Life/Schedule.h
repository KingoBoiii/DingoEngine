#pragma once
#include "Defs/VillagerDefs.h"
#include "Sim/SimTypes.h"
#include "World/TileGrid.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace Dingo
{

	// The places a villager's day visits. Each has a nominal tile; the nearest walkable one stands in when a player's
	// building or fence covers it.
	enum class Spot : uint8_t { Inside, Doorstep, SquareWest, SquareCentre, SquareEast, BoardFront, RoadWest, PondShore, Count };

	enum class ScheduleVariant : uint8_t { Fair, Wet, Snow, Count };

	ScheduleVariant VariantFor(Weather weather);

	// At `Minute` (since midnight) the villager sets off for `Where`.
	struct ScheduleEntry
	{
		int Minute;
		Spot Where;
	};

	// Sorted by minute; the first entry is Inside at the day's start and the last one is Inside again.
	std::span<const ScheduleEntry> GetSchedule(VillagerId villager, ScheduleVariant variant);

	// The tile a villager waits at for a spot, or (-1, -1) when nothing walkable lies near it.
	glm::ivec2 ResolveSpotTile(const TileGrid& grid, VillagerId villager, Spot spot);

	// Shortest 4-neighbour walk over walkable tiles, both ends included; empty when `to` cannot be reached.
	std::vector<glm::ivec2> FindRoute(const TileGrid& grid, const glm::ivec2& from, const glm::ivec2& to);

	struct PlanStop
	{
		int Minute = 0;
		Spot Where = Spot::Inside;
		glm::ivec2 Tile{ 0 };
		bool Reachable = true;
		// World (x, z) tile centres from the previous stop's tile to this one; one point when the villager stays.
		std::vector<glm::vec2> Route;
		float Yaw = 0.0f;
	};

	// One villager's day in one weather variant, resolved against the current grid. Rebuilt when the grid changes.
	struct DayPlan
	{
		VillagerId Who = VillagerId::Shopkeeper;
		ScheduleVariant Variant = ScheduleVariant::Fair;
		std::vector<PlanStop> Stops;
	};

	DayPlan BuildDayPlan(const TileGrid& grid, VillagerId villager, ScheduleVariant variant);

	struct VillagerPlace
	{
		glm::vec2 Position{ 0.0f };
		float Yaw = 0.0f;
		bool Indoors = true;
		bool Moving = false;
	};

	// Where the villager is at `minute` since midnight. Stateless: the same plan and time give the same place, so a
	// loaded game or a skipped hour lands on the schedule without replaying anything.
	VillagerPlace PlaceOnPlan(const DayPlan& plan, float minute);

	// Minutes a walk of `tiles` tiles takes.
	float WalkMinutes(int tiles);

}
