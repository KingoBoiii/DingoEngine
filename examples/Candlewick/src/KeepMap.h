#pragma once

#include <glm/glm.hpp>

#include <array>
#include <string>
#include <vector>

namespace Dingo
{

	struct TileRect
	{
		int Col = 0;
		int Row = 0;
		int Width = 0;
		int Height = 0;

		bool Contains(const glm::ivec2& tile) const
		{
			return tile.x >= Col && tile.x < Col + Width && tile.y >= Row && tile.y < Row + Height;
		}
	};

	enum class MarkerType
	{
		Spawn,
		Brazier,
		Altar,
		Sconce,
		Candle,
		Flask,
		Count
	};

	struct KeepRoom
	{
		std::string Name;
		TileRect Rect;
		glm::ivec2 Spawn{ 0 };
	};

	struct KeepMarker
	{
		MarkerType Type = MarkerType::Spawn;
		glm::ivec2 Tile{ 0 };
		int Room = -1;
	};

	// The whole keep as one grid. Tile (col, row) spans [col, col + 1) x [row, row + 1) tiles on the
	// X and Z axes, so the top row is the northernmost (-Z) and the map reads as seen on screen.
	class KeepMap
	{
	public:
		KeepMap();

		int GetWidth() const { return m_Width; }
		int GetHeight() const { return m_Height; }

		char At(const glm::ivec2& tile) const;
		bool IsWall(const glm::ivec2& tile) const { return At(tile) == '#'; }
		bool HasFloor(const glm::ivec2& tile) const;
		bool IsWalkable(const glm::ivec2& tile) const;

		glm::vec3 TileCenter(const glm::ivec2& tile) const;
		glm::ivec2 TileOf(const glm::vec3& worldPosition) const;

		// -1 in corridors and off the map. An alcove belongs to the room it opens off.
		int RoomOf(const glm::ivec2& tile) const;

		const std::vector<KeepRoom>& GetRooms() const { return m_Rooms; }
		const std::vector<TileRect>& GetCorridors() const { return m_Corridors; }
		const std::vector<KeepMarker>& GetMarkers(MarkerType type) const { return m_Markers[static_cast<size_t>(type)]; }

	private:
		bool InBounds(const glm::ivec2& tile) const;
		size_t IndexOf(const glm::ivec2& tile) const { return static_cast<size_t>(tile.y) * m_Width + tile.x; }

	private:
		int m_Width = 0;
		int m_Height = 0;
		std::vector<char> m_Tiles;
		std::vector<int> m_RoomOfTile;

		std::vector<KeepRoom> m_Rooms;
		std::vector<TileRect> m_Corridors;
		std::array<std::vector<KeepMarker>, static_cast<size_t>(MarkerType::Count)> m_Markers;
	};

}
