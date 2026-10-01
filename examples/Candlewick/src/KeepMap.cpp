#include "KeepMap.h"
#include "GameTuning.h"

#include <DingoEngine.h>

#include <algorithm>
#include <cmath>
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
	}

	bool KeepMap::InBounds(const glm::ivec2& tile) const
	{
		return tile.x >= 0 && tile.y >= 0 && tile.x < m_Width && tile.y < m_Height;
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
