#include "World/WorldCells.h"
#include "GameTuning.h"

#include <algorithm>

namespace Dingo
{

	float GroundTop(GroundLook look)
	{
		switch (look)
		{
			case GroundLook::Water: return WATER_TOP;
			case GroundLook::Rim:   return RIM_TOP;
			default:                return GROUND_TOP;
		}
	}

	void MergeGroundRects(const TileGrid& grid, const TileRect& cell, std::vector<GroundRect>& out)
	{
		out.clear();
		const int w = cell.W;
		const int h = cell.H;
		std::vector<uint8_t> taken(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
		const auto look = [&](int i, int j) { return GroundLookOf(grid.GetGround(cell.X + i, cell.Y + j)); };
		const auto free = [&](int i, int j) { return taken[static_cast<size_t>(j) * static_cast<size_t>(w) + static_cast<size_t>(i)] == 0; };

		for (int j = 0; j < h; ++j)
		{
			for (int i = 0; i < w; ++i)
			{
				if (!free(i, j))
					continue;
				const GroundLook kind = look(i, j);
				int rw = 1;
				while (i + rw < w && free(i + rw, j) && look(i + rw, j) == kind)
					++rw;
				int rh = 1;
				for (; j + rh < h; ++rh)
				{
					bool row = true;
					for (int k = 0; k < rw && row; ++k)
						row = free(i + k, j + rh) && look(i + k, j + rh) == kind;
					if (!row)
						break;
				}
				for (int b = 0; b < rh; ++b)
					for (int a = 0; a < rw; ++a)
						taken[static_cast<size_t>(j + b) * static_cast<size_t>(w) + static_cast<size_t>(i + a)] = 1;
				out.push_back({ cell.X + i, cell.Y + j, rw, rh, kind });
			}
		}
	}

	DirtyCells::DirtyCells(int tilesX, int tilesY, int cellTiles)
		: m_TilesX(tilesX), m_TilesY(tilesY), m_CellTiles(cellTiles),
		m_CellsX((tilesX + cellTiles - 1) / cellTiles), m_CellsY((tilesY + cellTiles - 1) / cellTiles),
		m_Queued(static_cast<size_t>(m_CellsX) * static_cast<size_t>(m_CellsY), 0)
	{}

	TileRect DirtyCells::CellRect(int cell) const
	{
		const int x = (cell % m_CellsX) * m_CellTiles;
		const int y = (cell / m_CellsX) * m_CellTiles;
		return { x, y, (std::min)(m_CellTiles, m_TilesX - x), (std::min)(m_CellTiles, m_TilesY - y) };
	}

	void DirtyCells::MarkCell(int cell)
	{
		if (cell < 0 || cell >= GetCellCount() || IsQueued(cell))
			return;
		m_Queued[static_cast<size_t>(cell)] = 1;
		m_Queue.push_back(cell);
	}

	void DirtyCells::MarkTile(int x, int y)
	{
		if (x < 0 || y < 0 || x >= m_TilesX || y >= m_TilesY)
			return;
		MarkCell(CellOfTile(x, y));
	}

	void DirtyCells::MarkArea(int x, int y, int w, int h)
	{
		const int x0 = (std::max)(x, 0);
		const int y0 = (std::max)(y, 0);
		const int x1 = (std::min)(x + w, m_TilesX) - 1;
		const int y1 = (std::min)(y + h, m_TilesY) - 1;
		if (x1 < x0 || y1 < y0)
			return;
		for (int cy = y0 / m_CellTiles; cy <= y1 / m_CellTiles; ++cy)
			for (int cx = x0 / m_CellTiles; cx <= x1 / m_CellTiles; ++cx)
				MarkCell(cy * m_CellsX + cx);
	}

	void DirtyCells::MarkAll()
	{
		for (int cell = 0; cell < GetCellCount(); ++cell)
			MarkCell(cell);
	}

	void DirtyCells::MarkFarmCells(const TileGrid& grid)
	{
		for (int y = 0; y < grid.GetHeight(); ++y)
		{
			for (int x = 0; x < grid.GetWidth(); ++x)
			{
				const Tile& tile = grid.At(x, y);
				if (tile.Crop.Present() || IsSoil(tile.GroundState))
					MarkTile(x, y);
			}
		}
	}

	std::vector<int> DirtyCells::Drain()
	{
		std::vector<int> cells = std::move(m_Queue);
		m_Queue.clear();
		std::sort(cells.begin(), cells.end());
		for (const int cell : cells)
			m_Queued[static_cast<size_t>(cell)] = 0;
		return cells;
	}

}
