#include "KeepMap.h"
#include "GameTuning.h"

#include <DingoEngine.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>

namespace
{
	using namespace Dingo;

	constexpr std::string_view k_Rows[] =
	{
		"              ##################                                ###############",
		"              #...s....s....s..#                                #.............#",
		"############  #..............o.#        ####        ####        #....c.A.c....#",
		"#..s....s..#  #..B.........#..s#  #######..##########..##########.............#",
		"#.o........#  #s...#...........#  #............B..............................#",
		"#s...B....s####................#  #.c.c.c.c.c.c.c.c.c.c.c.c......4..#..c..#...#",
		"#.....................##......s#  #..........................####.............#",
		"#.1.............2.....##..#....####..........................#  #..c.......c..#",
		"#..........####.....................c.c.c.c.c.c.c.c.c.c.c.c..#  #.............#",
		"#........o.#  #....#................3......................o.#  #...#..c..#...#",
		"#..........#  #s............B..################..#############  #.............#",
		"############  #.o.............s#              ####              #..c.......c..#",
		"              #................#                                #............o#",
		"              ##################                                ###############",
	};

	const std::pair<const char*, TileRect> k_RoomDefs[] =
	{
		{ "The Gatehouse", { 1, 3, 10, 8 } },
		{ "The Great Hall", { 15, 1, 16, 12 } },
		{ "The Gallery", { 35, 4, 26, 6 } },
		{ "The Chapel", { 65, 1, 13, 12 } },
	};

	const TileRect k_CorridorDefs[] =
	{
		{ 11, 6, 4, 2 },
		{ 31, 8, 4, 2 },
		{ 61, 4, 4, 2 },
	};

	// The Hall loop circles its central pillars; the Gallery pair start at opposite ends and pass side
	// by side mid-gallery; the Chapel loop crosses in front of the altar.
	const WardenRoute k_WardenRouteDefs[] =
	{
		{ 1, { { 21, 4 }, { 27, 4 }, { 27, 9 }, { 21, 9 } } },
		{ 2, { { 38, 6 }, { 51, 6 }, { 51, 7 }, { 38, 7 } } },
		{ 2, { { 57, 7 }, { 44, 7 }, { 44, 6 }, { 57, 6 } } },
		{ 3, { { 69, 3 }, { 73, 3 }, { 73, 10 }, { 69, 10 } } },
	};

	constexpr glm::ivec2 k_Steps[] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

	constexpr glm::ivec2 k_CheckpointOffsets[] = { { 0, 1 }, { 1, 0 }, { -1, 0 }, { 0, -1 }, { 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 } };

	// The eye Wardens::Reset leaves a warden with, but at full range: walls are not known here.
	SpotLight RouteStartEye(const KeepMap& map, const std::vector<glm::ivec2>& lane)
	{
		glm::vec3 ahead(0.0f, 0.0f, -1.0f);
		if (lane.size() > 1)
			ahead = glm::vec3(static_cast<float>(lane[1].x - lane[0].x), 0.0f, static_cast<float>(lane[1].y - lane[0].y));

		const float pitch = glm::radians(WARDEN_EYE_PITCH_DEG);
		SpotLight eye;
		eye.Position = map.TileCenter(lane.front()) + glm::vec3(0.0f, WARDEN_EYE_HEIGHT, 0.0f) + ahead * WARDEN_EYE_FORWARD;
		eye.Direction = glm::vec3(ahead.x * std::cos(pitch), -std::sin(pitch), ahead.z * std::cos(pitch));
		eye.Color = WARDEN_LIGHT_COLOR;
		eye.Intensity = WARDEN_EYE_INTENSITY;
		eye.Range = WARDEN_EYE_RANGE;
		eye.InnerConeAngle = WARDEN_EYE_INNER_DEG;
		eye.OuterConeAngle = WARDEN_EYE_OUTER_DEG;
		return eye;
	}

	bool MarkerTypeOf(char c, MarkerType& type)
	{
		switch (c)
		{
		case '1': case '2': case '3': case '4': type = MarkerType::Spawn; return true;
		case 'B': type = MarkerType::Brazier; return true;
		case 'A': type = MarkerType::Altar; return true;
		case 's': type = MarkerType::Sconce; return true;
		case 'c': type = MarkerType::Candle; return true;
		case 'o': type = MarkerType::Flask; return true;
		default: return false;
		}
	}
}

namespace Dingo
{

	KeepMap::KeepMap()
	{
		m_Height = static_cast<int>(std::size(k_Rows));
		for (std::string_view row : k_Rows)
			m_Width = std::max(m_Width, static_cast<int>(row.size()));

		m_Tiles.assign(static_cast<size_t>(m_Width) * m_Height, ' ');
		m_RoomOfTile.assign(m_Tiles.size(), -1);

		for (int row = 0; row < m_Height; ++row)
			for (int col = 0; col < static_cast<int>(k_Rows[row].size()); ++col)
				m_Tiles[IndexOf({ col, row })] = k_Rows[row][col];

		for (const auto& [name, rect] : k_RoomDefs)
		{
			const int index = static_cast<int>(m_Rooms.size());
			m_Rooms.push_back({ name, rect, glm::ivec2(rect.Col, rect.Row) });
			for (int row = rect.Row; row < rect.Row + rect.Height; ++row)
				for (int col = rect.Col; col < rect.Col + rect.Width; ++col)
					m_RoomOfTile[IndexOf({ col, row })] = index;
		}

		m_Corridors.assign(std::begin(k_CorridorDefs), std::end(k_CorridorDefs));

		for (int row = 0; row < m_Height; ++row)
		{
			for (int col = 0; col < m_Width; ++col)
			{
				const glm::ivec2 tile(col, row);
				const bool inCorridor = std::any_of(m_Corridors.begin(), m_Corridors.end(), [&](const TileRect& rect) { return rect.Contains(tile); });
				if (!HasFloor(tile) || RoomOf(tile) >= 0 || inCorridor)
					continue;

				for (const glm::ivec2& step : { glm::ivec2(0, 1), glm::ivec2(0, -1), glm::ivec2(1, 0), glm::ivec2(-1, 0) })
				{
					if (!InBounds(tile + step) || !HasFloor(tile + step))
						continue;
					const int room = m_RoomOfTile[IndexOf(tile + step)];
					if (room >= 0)
					{
						m_RoomOfTile[IndexOf(tile)] = room;
						break;
					}
				}
			}
		}

		for (int row = 0; row < m_Height; ++row)
		{
			for (int col = 0; col < m_Width; ++col)
			{
				const glm::ivec2 tile(col, row);
				MarkerType type;
				if (!MarkerTypeOf(At(tile), type))
					continue;

				m_Markers[static_cast<size_t>(type)].push_back({ type, tile, RoomOf(tile) });
				if (type == MarkerType::Spawn)
				{
					const int room = At(tile) - '1';
					if (room >= 0 && room < static_cast<int>(m_Rooms.size()))
						m_Rooms[room].Spawn = tile;
				}
			}
		}

		for (size_t i = 0; i < m_Rooms.size(); ++i)
		{
			if (!m_Rooms[i].Rect.Contains(m_Rooms[i].Spawn) || At(m_Rooms[i].Spawn) != static_cast<char>('1' + i))
				DE_ERROR("Candlewick: room {} ('{}') has no spawn tile inside its rectangle", i + 1, m_Rooms[i].Name);
		}

		m_WardenRoutes.assign(std::begin(k_WardenRouteDefs), std::end(k_WardenRouteDefs));
		BuildRouteLanes();
		ValidateRoutes();
		ValidateMap();
	}

	void KeepMap::BuildRouteLanes()
	{
		for (const WardenRoute& route : m_WardenRoutes)
		{
			std::vector<glm::ivec2> lane;
			for (size_t w = 0; w < route.Waypoints.size(); ++w)
			{
				const std::vector<glm::ivec2> leg = FindPath(route.Waypoints[w], route.Waypoints[(w + 1) % route.Waypoints.size()], route.Room);
				if (leg.size() > 1)
					lane.insert(lane.end(), leg.begin(), leg.end() - 1);
			}
			if (lane.empty() && !route.Waypoints.empty())
				lane.push_back(route.Waypoints.front());
			m_RouteLanes.push_back(std::move(lane));
		}
	}

	void KeepMap::ValidateRoutes() const
	{
		for (size_t i = 0; i < m_WardenRoutes.size(); ++i)
		{
			const WardenRoute& route = m_WardenRoutes[i];
			for (size_t w = 0; w < route.Waypoints.size(); ++w)
			{
				const glm::ivec2& from = route.Waypoints[w];
				const glm::ivec2& to = route.Waypoints[(w + 1) % route.Waypoints.size()];
				if (RoomOf(from) != route.Room)
					DE_ERROR("Candlewick: warden {} waypoint ({}, {}) is outside its room", i + 1, from.x, from.y);
				if (FindPath(from, to, route.Room).empty())
					DE_ERROR("Candlewick: warden {} has no path from ({}, {}) to ({}, {})", i + 1, from.x, from.y, to.x, to.y);
			}
		}
	}

	void KeepMap::ValidateMap() const
	{
		if (m_Rooms.empty())
			return;

		std::vector<bool> reached(m_Tiles.size(), false);
		std::vector<glm::ivec2> frontier;
		const glm::ivec2 start = m_Rooms.front().Spawn;
		if (IsWalkable(start))
		{
			reached[IndexOf(start)] = true;
			frontier.push_back(start);
		}
		for (size_t head = 0; head < frontier.size(); ++head)
		{
			for (const glm::ivec2& step : k_Steps)
			{
				const glm::ivec2 next = frontier[head] + step;
				if (!InBounds(next) || !IsWalkable(next) || reached[IndexOf(next)])
					continue;
				reached[IndexOf(next)] = true;
				frontier.push_back(next);
			}
		}

		const auto isReached = [&](const glm::ivec2& tile) { return InBounds(tile) && reached[IndexOf(tile)]; };
		const auto isReachedBeside = [&](const glm::ivec2& tile)
		{
			return std::any_of(std::begin(k_CheckpointOffsets), std::end(k_CheckpointOffsets), [&](const glm::ivec2& offset) { return isReached(tile + offset); });
		};

		for (const MarkerType type : { MarkerType::Brazier, MarkerType::Altar })
		{
			const char* name = type == MarkerType::Altar ? "altar" : "brazier";
			for (const KeepMarker& marker : GetMarkers(type))
			{
				if (!isReachedBeside(marker.Tile))
					DE_WARN("Candlewick: the {} at ({}, {}) cannot be reached from the first spawn", name, marker.Tile.x, marker.Tile.y);
				if (type != MarkerType::Brazier)
					continue;

				const glm::ivec2 checkpoint = FindCheckpointTile(marker.Tile);
				if (!isReached(checkpoint))
					DE_WARN("Candlewick: the checkpoint ({}, {}) of the brazier at ({}, {}) cannot be reached from the first spawn", checkpoint.x, checkpoint.y, marker.Tile.x, marker.Tile.y);
				if (GetLaneDistance(checkpoint) == 0.0f)
					DE_WARN("Candlewick: the checkpoint ({}, {}) of the brazier at ({}, {}) lies on a patrol lane", checkpoint.x, checkpoint.y, marker.Tile.x, marker.Tile.y);
				else if (IsSeenAtRouteStart(checkpoint))
					DE_WARN("Candlewick: the checkpoint ({}, {}) of the brazier at ({}, {}) is inside a warden's cone when its route starts over", checkpoint.x, checkpoint.y, marker.Tile.x, marker.Tile.y);
			}
		}
	}

	float KeepMap::GetLaneDistance(const glm::ivec2& tile) const
	{
		int nearest = std::numeric_limits<int>::max();
		for (const std::vector<glm::ivec2>& lane : m_RouteLanes)
		{
			for (const glm::ivec2& laneTile : lane)
			{
				const glm::ivec2 offset = tile - laneTile;
				nearest = std::min(nearest, offset.x * offset.x + offset.y * offset.y);
			}
		}
		return nearest == std::numeric_limits<int>::max() ? std::numeric_limits<float>::max() : std::sqrt(static_cast<float>(nearest));
	}

	bool KeepMap::IsSeenAtRouteStart(const glm::ivec2& tile) const
	{
		const glm::vec3 center = TileCenter(tile);
		for (const std::vector<glm::ivec2>& lane : m_RouteLanes)
		{
			if (lane.empty())
				continue;

			const SpotLight eye = RouteStartEye(*this, lane);
			for (const float height : { SAMPLE_FEET, SAMPLE_CHEST, SAMPLE_HEAD })
			{
				if (GetLightAttenuation(eye, center + glm::vec3(0.0f, height, 0.0f)) >= SEEN_WEIGHT)
					return true;
			}
		}
		return false;
	}

	bool KeepMap::InBounds(const glm::ivec2& tile) const
	{
		return tile.x >= 0 && tile.y >= 0 && tile.x < m_Width && tile.y < m_Height;
	}

	bool KeepMap::IsPatrolFloor(const glm::ivec2& tile, int room) const
	{
		return IsWalkable(tile) && At(tile) != 'c' && (room < 0 || RoomOf(tile) == room);
	}

	std::vector<glm::ivec2> KeepMap::FindPath(const glm::ivec2& from, const glm::ivec2& to, int room) const
	{
		return FindPathToNearest(from, { to }, room);
	}

	std::optional<glm::ivec2> KeepMap::FindNearestRoomTile(const glm::ivec2& tile, int room) const
	{
		std::optional<glm::ivec2> nearest;
		int nearestDistance = 0;
		for (int row = 0; row < m_Height; ++row)
		{
			for (int col = 0; col < m_Width; ++col)
			{
				const glm::ivec2 candidate(col, row);
				if (!IsPatrolFloor(candidate, room))
					continue;

				const glm::ivec2 offset = candidate - tile;
				const int distance = offset.x * offset.x + offset.y * offset.y;
				if (!nearest || distance < nearestDistance)
				{
					nearest = candidate;
					nearestDistance = distance;
				}
			}
		}
		return nearest;
	}

	std::vector<glm::ivec2> KeepMap::FindPathToNearest(const glm::ivec2& from, const std::vector<glm::ivec2>& goals, int room) const
	{
		if (!IsPatrolFloor(from, room))
			return {};

		std::vector<bool> isGoal(m_Tiles.size(), false);
		for (const glm::ivec2& goal : goals)
		{
			if (IsPatrolFloor(goal, room))
				isGoal[IndexOf(goal)] = true;
		}

		std::vector<int> parent(m_Tiles.size(), -1);
		std::vector<bool> visited(m_Tiles.size(), false);
		std::vector<glm::ivec2> frontier = { from };
		visited[IndexOf(from)] = true;

		for (size_t head = 0; head < frontier.size(); ++head)
		{
			const glm::ivec2 tile = frontier[head];
			if (isGoal[IndexOf(tile)])
			{
				std::vector<glm::ivec2> path;
				for (int index = static_cast<int>(IndexOf(tile)); index >= 0; index = parent[index])
					path.emplace_back(index % m_Width, index / m_Width);
				std::reverse(path.begin(), path.end());
				return path;
			}

			for (const glm::ivec2& step : k_Steps)
			{
				const glm::ivec2 next = tile + step;
				if (!IsPatrolFloor(next, room) || visited[IndexOf(next)])
					continue;
				visited[IndexOf(next)] = true;
				parent[IndexOf(next)] = static_cast<int>(IndexOf(tile));
				frontier.push_back(next);
			}
		}
		return {};
	}

	glm::ivec2 KeepMap::FindCheckpointTile(const glm::ivec2& brazier) const
	{
		const int room = RoomOf(brazier);
		std::optional<glm::ivec2> best;
		bool bestSeen = true;
		float bestClearance = 0.0f;
		for (const glm::ivec2& offset : k_CheckpointOffsets)
		{
			const glm::ivec2 tile = brazier + offset;
			if (!IsPatrolFloor(tile, room))
				continue;

			const bool seen = IsSeenAtRouteStart(tile);
			const float clearance = GetLaneDistance(tile);
			if (!best || (bestSeen && !seen) || (seen == bestSeen && clearance > bestClearance))
			{
				best = tile;
				bestSeen = seen;
				bestClearance = clearance;
			}
		}
		return best ? *best : FindNearestRoomTile(brazier, room).value_or(brazier);
	}

	char KeepMap::At(const glm::ivec2& tile) const
	{
		return InBounds(tile) ? m_Tiles[IndexOf(tile)] : ' ';
	}

	bool KeepMap::HasFloor(const glm::ivec2& tile) const
	{
		const char c = At(tile);
		return c != ' ' && c != '#';
	}

	bool KeepMap::IsWalkable(const glm::ivec2& tile) const
	{
		return HasFloor(tile) && At(tile) != 'B' && At(tile) != 'A';
	}

	glm::vec3 KeepMap::TileCenter(const glm::ivec2& tile) const
	{
		return glm::vec3(static_cast<float>(tile.x) + 0.5f, 0.0f, static_cast<float>(tile.y) + 0.5f) * TILE_SIZE;
	}

	glm::ivec2 KeepMap::TileOf(const glm::vec3& worldPosition) const
	{
		return glm::ivec2(static_cast<int>(std::floor(worldPosition.x / TILE_SIZE)), static_cast<int>(std::floor(worldPosition.z / TILE_SIZE)));
	}

	int KeepMap::RoomOf(const glm::ivec2& tile) const
	{
		return InBounds(tile) ? m_RoomOfTile[IndexOf(tile)] : -1;
	}

}
