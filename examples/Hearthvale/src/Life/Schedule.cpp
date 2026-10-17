#include "Life/Schedule.h"
#include "GameTuning.h"
#include "Life/PlayerMath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <queue>

namespace Dingo
{

	namespace
	{
		using S = Spot;
		using E = ScheduleEntry;

		constexpr E k_MiraFair[] = { { 360, S::Inside }, { 480, S::Doorstep }, { 720, S::SquareCentre }, { 780, S::Doorstep }, { 1020, S::BoardFront }, { 1140, S::Inside } };
		constexpr E k_MiraWet[] = { { 360, S::Inside }, { 540, S::Doorstep }, { 1020, S::Inside } };
		constexpr E k_BramFair[] = { { 360, S::Inside }, { 450, S::Doorstep }, { 660, S::RoadWest }, { 750, S::Doorstep }, { 1000, S::SquareEast }, { 1110, S::Inside } };
		constexpr E k_BramWet[] = { { 360, S::Inside }, { 480, S::Doorstep }, { 1020, S::Inside } };
		constexpr E k_OdetteFair[] = { { 360, S::Inside }, { 480, S::BoardFront }, { 660, S::SquareCentre }, { 780, S::Doorstep }, { 960, S::BoardFront }, { 1080, S::Inside } };
		constexpr E k_OdetteWet[] = { { 360, S::Inside }, { 600, S::Doorstep }, { 960, S::Inside } };
		constexpr E k_PipFair[] = { { 360, S::Inside }, { 510, S::SquareWest }, { 660, S::PondShore }, { 840, S::SquareEast }, { 1020, S::Doorstep }, { 1140, S::Inside } };
		constexpr E k_PipWet[] = { { 360, S::Inside }, { 600, S::Doorstep }, { 840, S::Inside } };
		constexpr E k_PipSnow[] = { { 360, S::Inside }, { 540, S::SquareCentre }, { 840, S::Doorstep }, { 960, S::Inside } };

		glm::ivec2 NominalTile(Spot spot)
		{
			switch (spot)
			{
			case Spot::SquareWest: return { 66, 46 };
			case Spot::SquareCentre: return { 71, 48 };
			case Spot::SquareEast: return { 77, 46 };
			case Spot::BoardFront: return { 72, 42 };
			case Spot::RoadWest: return { 56, 48 };
			case Spot::PondShore: return { POND_SHORE_X, POND_SHORE_Y };
			default: return { 0, 0 };
			}
		}

		int Outward(const LandmarkSpec& spec)
		{
			return spec.DoorY >= spec.Footprint.Bottom() ? 1 : -1;
		}

		glm::vec2 CentreOf(const glm::ivec2& tile)
		{
			return { (static_cast<float>(tile.x) + 0.5f) * TILE_SIZE, (static_cast<float>(tile.y) + 0.5f) * TILE_SIZE };
		}

		glm::ivec2 NearestWalkable(const TileGrid& grid, const glm::ivec2& wanted)
		{
			if (PlayerMath::IsWalkable(grid, wanted.x, wanted.y))
				return wanted;
			for (int radius = 1; radius <= 8; ++radius)
			{
				glm::ivec2 best(-1, -1);
				int bestDistance = 1 << 30;
				for (int y = wanted.y - radius; y <= wanted.y + radius; ++y)
				{
					for (int x = wanted.x - radius; x <= wanted.x + radius; ++x)
					{
						if ((std::max)(std::abs(x - wanted.x), std::abs(y - wanted.y)) != radius || !PlayerMath::IsWalkable(grid, x, y))
							continue;
						const int distance = (x - wanted.x) * (x - wanted.x) + (y - wanted.y) * (y - wanted.y);
						if (distance < bestDistance)
						{
							bestDistance = distance;
							best = { x, y };
						}
					}
				}
				if (best.x >= 0)
					return best;
			}
			return { -1, -1 };
		}
	}

	ScheduleVariant VariantFor(Weather weather)
	{
		switch (weather)
		{
		case Weather::Rain: return ScheduleVariant::Wet;
		case Weather::Snow: return ScheduleVariant::Snow;
		default: return ScheduleVariant::Fair;
		}
	}

	std::span<const ScheduleEntry> GetSchedule(VillagerId villager, ScheduleVariant variant)
	{
		switch (villager)
		{
		case VillagerId::Shopkeeper: return variant == ScheduleVariant::Fair ? std::span<const E>(k_MiraFair) : std::span<const E>(k_MiraWet);
		case VillagerId::Carpenter: return variant == ScheduleVariant::Fair ? std::span<const E>(k_BramFair) : std::span<const E>(k_BramWet);
		case VillagerId::Mayor: return variant == ScheduleVariant::Fair ? std::span<const E>(k_OdetteFair) : std::span<const E>(k_OdetteWet);
		default:
			if (variant == ScheduleVariant::Fair)
				return k_PipFair;
			return variant == ScheduleVariant::Wet ? std::span<const E>(k_PipWet) : std::span<const E>(k_PipSnow);
		}
	}

	glm::ivec2 ResolveSpotTile(const TileGrid& grid, VillagerId villager, Spot spot)
	{
		const LandmarkSpec& home = GetLandmarkSpec(GetVillagerDef(villager).Home);
		glm::ivec2 nominal;
		if (spot == Spot::Inside)
			nominal = { home.DoorX, home.DoorY };
		else if (spot == Spot::Doorstep)
			nominal = { home.DoorX, home.DoorY + Outward(home) };
		else
			nominal = NominalTile(spot);
		return NearestWalkable(grid, nominal);
	}

	std::vector<glm::ivec2> FindRoute(const TileGrid& grid, const glm::ivec2& from, const glm::ivec2& to)
	{
		if (!PlayerMath::IsWalkable(grid, from.x, from.y) || !PlayerMath::IsWalkable(grid, to.x, to.y))
			return {};

		const int width = grid.GetWidth();
		std::vector<int> distance(static_cast<size_t>(width) * static_cast<size_t>(grid.GetHeight()), -1);
		auto index = [width](const glm::ivec2& tile) { return static_cast<size_t>(tile.y) * static_cast<size_t>(width) + static_cast<size_t>(tile.x); };
		const glm::ivec2 steps[4] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

		std::queue<glm::ivec2> open;
		distance[index(to)] = 0;
		open.push(to);
		while (!open.empty() && distance[index(from)] < 0)
		{
			const glm::ivec2 current = open.front();
			open.pop();
			for (const glm::ivec2& step : steps)
			{
				const glm::ivec2 next = current + step;
				if (!PlayerMath::IsWalkable(grid, next.x, next.y) || distance[index(next)] >= 0)
					continue;
				distance[index(next)] = distance[index(current)] + 1;
				open.push(next);
			}
		}
		if (distance[index(from)] < 0)
			return {};

		std::vector<glm::ivec2> route{ from };
		glm::ivec2 heading(0, 0);
		glm::ivec2 current = from;
		while (current != to)
		{
			const int want = distance[index(current)] - 1;
			glm::ivec2 chosen(0, 0);
			bool found = false;
			if (heading != glm::ivec2(0, 0))
			{
				const glm::ivec2 ahead = current + heading;
				if (PlayerMath::IsWalkable(grid, ahead.x, ahead.y) && distance[index(ahead)] == want)
				{
					chosen = heading;
					found = true;
				}
			}
			for (size_t i = 0; i < 4 && !found; ++i)
			{
				const glm::ivec2 next = current + steps[i];
				if (PlayerMath::IsWalkable(grid, next.x, next.y) && distance[index(next)] == want)
				{
					chosen = steps[i];
					found = true;
				}
			}
			heading = chosen;
			current += chosen;
			route.push_back(current);
		}
		return route;
	}

	DayPlan BuildDayPlan(const TileGrid& grid, VillagerId villager, ScheduleVariant variant)
	{
		DayPlan plan;
		plan.Who = villager;
		plan.Variant = variant;

		glm::ivec2 here(-1, -1);
		float yaw = 0.0f;
		for (const ScheduleEntry& entry : GetSchedule(villager, variant))
		{
			PlanStop stop;
			stop.Minute = entry.Minute;
			stop.Where = entry.Where;
			const glm::ivec2 wanted = ResolveSpotTile(grid, villager, entry.Where);

			if (here.x < 0)
			{
				stop.Tile = wanted;
				stop.Route = { CentreOf(wanted) };
				stop.Reachable = wanted.x >= 0;
			}
			else
			{
				std::vector<glm::ivec2> route = wanted.x >= 0 ? FindRoute(grid, here, wanted) : std::vector<glm::ivec2>();
				if (route.empty())
				{
					stop.Reachable = false;
					stop.Tile = here;
					stop.Route = { CentreOf(here) };
				}
				else
				{
					stop.Tile = wanted;
					for (const glm::ivec2& tile : route)
						stop.Route.push_back(CentreOf(tile));
				}
			}

			if (stop.Route.size() >= 2)
			{
				const glm::vec2 last = stop.Route[stop.Route.size() - 1] - stop.Route[stop.Route.size() - 2];
				yaw = PlayerMath::YawOf(last);
			}
			stop.Yaw = yaw;
			here = stop.Tile;
			plan.Stops.push_back(std::move(stop));
		}
		return plan;
	}

	float WalkMinutes(int tiles)
	{
		return static_cast<float>(tiles) / VILLAGER_WALK_TILES_PER_MINUTE;
	}

	VillagerPlace PlaceOnPlan(const DayPlan& plan, float minute)
	{
		VillagerPlace place;
		if (plan.Stops.empty())
			return place;

		size_t active = 0;
		for (size_t i = 0; i < plan.Stops.size(); ++i)
		{
			if (static_cast<float>(plan.Stops[i].Minute) <= minute)
				active = i;
		}

		const PlanStop& stop = plan.Stops[active];
		const float walked = (std::max)(minute - static_cast<float>(stop.Minute), 0.0f) * VILLAGER_WALK_TILES_PER_MINUTE;
		const size_t segments = stop.Route.size() - 1;
		place.Yaw = stop.Yaw;

		if (walked >= static_cast<float>(segments))
		{
			place.Position = stop.Route.back();
			place.Indoors = stop.Where == Spot::Inside;
			return place;
		}

		const size_t from = static_cast<size_t>(walked);
		const float fraction = walked - static_cast<float>(from);
		const glm::vec2 a = stop.Route[from];
		const glm::vec2 b = stop.Route[from + 1];
		place.Position = a + (b - a) * fraction;
		place.Yaw = PlayerMath::YawOf(b - a);
		place.Indoors = false;
		place.Moving = true;
		return place;
	}

}
