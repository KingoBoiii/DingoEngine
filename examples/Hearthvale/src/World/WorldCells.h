#pragma once
#include "World/TileGrid.h"
#include "World/ValleyLayout.h"

#include <cstdint>
#include <vector>

namespace Dingo
{

	// The ground drawn for a tile. Tilled and watered soil is drawn on top of Land, so tilling or
	// watering never changes the merged ground of a cell.
	enum class GroundLook : uint8_t { Land, Path, Water, Rim, Count };

	constexpr GroundLook GroundLookOf(Ground ground)
	{
		switch (ground)
		{
			case Ground::Path:    return GroundLook::Path;
			case Ground::Water:   return GroundLook::Water;
			case Ground::Blocked: return GroundLook::Rim;
			default:              return GroundLook::Land;
		}
	}

	constexpr bool IsSoil(Ground ground) { return ground == Ground::Tilled || ground == Ground::Watered; }

	float GroundTop(GroundLook look);

	struct GroundRect
	{
		int X = 0;
		int Y = 0;
		int W = 0;
		int H = 0;
		GroundLook Look = GroundLook::Land;

		bool operator==(const GroundRect&) const = default;
	};

	// Greedy row-major merge of the cell's tiles into rectangles of one look; every tile is in exactly one.
	void MergeGroundRects(const TileGrid& grid, const TileRect& cell, std::vector<GroundRect>& out);

	// The game's 16 x 16-tile cells (the merge and dirty unit) and a queue that holds each at most once.
	class DirtyCells
	{
	public:
		DirtyCells(int tilesX, int tilesY, int cellTiles);

		int GetCellCount() const { return m_CellsX * m_CellsY; }
		int GetCellsX() const { return m_CellsX; }
		int CellOfTile(int x, int y) const { return (y / m_CellTiles) * m_CellsX + x / m_CellTiles; }
		// An occupant belongs to the cell of its footprint's centre tile, as its mesh's bounds centre does.
		int CellOfOccupant(const Occupant& occupant) const { return CellOfTile(occupant.X + occupant.W / 2, occupant.Y + occupant.H / 2); }
		TileRect CellRect(int cell) const;

		void MarkTile(int x, int y);
		void MarkArea(int x, int y, int w, int h);
		void MarkCell(int cell);
		void MarkAll();
		// Cells holding a crop or tilled soil: what a day roll can change.
		void MarkFarmCells(const TileGrid& grid);

		bool IsQueued(int cell) const { return m_Queued[static_cast<size_t>(cell)] != 0; }
		bool Empty() const { return m_Queue.empty(); }
		// The queued cells in ascending order, each once; the queue is empty afterwards.
		std::vector<int> Drain();

	private:
		int m_TilesX;
		int m_TilesY;
		int m_CellTiles;
		int m_CellsX;
		int m_CellsY;
		std::vector<uint8_t> m_Queued;
		std::vector<int> m_Queue;
	};

}
