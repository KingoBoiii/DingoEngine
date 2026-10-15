#include "World/TileGrid.h"
#include "Sim/Fnv.h"

#include <algorithm>

namespace Dingo
{

	TileGrid::TileGrid(int width, int height)
		: m_Width(width), m_Height(height), m_Tiles(static_cast<size_t>(width) * static_cast<size_t>(height))
	{}

	bool TileGrid::IsAreaEmpty(int x, int y, int w, int h) const
	{
		if (w <= 0 || h <= 0 || !InBounds(x, y) || !InBounds(x + w - 1, y + h - 1))
			return false;
		for (int j = y; j < y + h; ++j)
			for (int i = x; i < x + w; ++i)
				if (At(i, j).Occupant != k_NoOccupant)
					return false;
		return true;
	}

	bool TileGrid::IsAreaFree(int x, int y, int w, int h) const
	{
		if (!IsAreaEmpty(x, y, w, h))
			return false;
		for (int j = y; j < y + h; ++j)
			for (int i = x; i < x + w; ++i)
			{
				const Tile& tile = At(i, j);
				if (!CanOccupy(tile.GroundState) || tile.Crop.Present())
					return false;
			}
		return true;
	}

	uint32_t TileGrid::Insert(OccupantKind kind, uint16_t defId, int x, int y, int w, int h, int rotation, int variant)
	{
		Occupant occupant;
		occupant.Id = m_NextOccupantId++;
		occupant.Kind = kind;
		occupant.DefId = defId;
		occupant.X = static_cast<int16_t>(x);
		occupant.Y = static_cast<int16_t>(y);
		occupant.W = static_cast<uint8_t>(w);
		occupant.H = static_cast<uint8_t>(h);
		occupant.Rotation = static_cast<uint8_t>(rotation & 3);
		occupant.Variant = static_cast<uint8_t>(variant);
		m_Occupants.push_back(occupant);
		for (int j = y; j < y + h; ++j)
			for (int i = x; i < x + w; ++i)
				At(i, j).Occupant = occupant.Id;
		return occupant.Id;
	}

	uint32_t TileGrid::AddOccupant(OccupantKind kind, uint16_t defId, int x, int y, int w, int h, int rotation, int variant)
	{
		if (!IsAreaFree(x, y, w, h))
			return k_NoOccupant;
		return Insert(kind, defId, x, y, w, h, rotation, variant);
	}

	uint32_t TileGrid::AddOccupantUnchecked(OccupantKind kind, uint16_t defId, int x, int y, int w, int h, int rotation, int variant)
	{
		if (!IsAreaEmpty(x, y, w, h))
			return k_NoOccupant;
		return Insert(kind, defId, x, y, w, h, rotation, variant);
	}

	const Occupant* TileGrid::FindOccupant(uint32_t id) const
	{
		const auto it = std::lower_bound(m_Occupants.begin(), m_Occupants.end(), id,
			[](const Occupant& occupant, uint32_t value) { return occupant.Id < value; });
		return (it != m_Occupants.end() && it->Id == id) ? &*it : nullptr;
	}

	bool TileGrid::RemoveOccupant(uint32_t id)
	{
		const auto it = std::lower_bound(m_Occupants.begin(), m_Occupants.end(), id,
			[](const Occupant& occupant, uint32_t value) { return occupant.Id < value; });
		if (it == m_Occupants.end() || it->Id != id)
			return false;
		for (int j = it->Y; j < it->Y + it->H; ++j)
			for (int i = it->X; i < it->X + it->W; ++i)
				At(i, j).Occupant = k_NoOccupant;
		m_Occupants.erase(it);
		return true;
	}

	int TileGrid::CountGround(Ground ground) const
	{
		return static_cast<int>(std::count_if(m_Tiles.begin(), m_Tiles.end(), [ground](const Tile& tile) { return tile.GroundState == ground; }));
	}

	int TileGrid::CountOccupants(OccupantKind kind) const
	{
		return static_cast<int>(std::count_if(m_Occupants.begin(), m_Occupants.end(), [kind](const Occupant& occupant) { return occupant.Kind == kind; }));
	}

	uint64_t TileGrid::Hash() const
	{
		Fnv1a hash;
		hash.U32(static_cast<uint32_t>(m_Width));
		hash.U32(static_cast<uint32_t>(m_Height));
		for (const Tile& tile : m_Tiles)
		{
			hash.Byte(static_cast<uint8_t>(tile.GroundState));
			hash.U32(tile.Occupant);
			hash.Byte(tile.Crop.Def);
			hash.Byte(tile.Crop.Stage);
			hash.Byte(tile.Crop.DaysWatered);
			hash.U32(static_cast<uint32_t>(tile.Crop.PlantedDay));
		}
		for (size_t i = 0; i < m_Tiles.size(); ++i)
		{
			if (m_Tiles[i].Built != 0)
			{
				hash.U32(static_cast<uint32_t>(i));
				hash.Byte(m_Tiles[i].Built);
			}
		}
		hash.U32(static_cast<uint32_t>(m_Occupants.size()));
		for (const Occupant& occupant : m_Occupants)
		{
			hash.U32(occupant.Id);
			hash.Byte(static_cast<uint8_t>(occupant.Kind));
			hash.U16(occupant.DefId);
			hash.U16(static_cast<uint16_t>(occupant.X));
			hash.U16(static_cast<uint16_t>(occupant.Y));
			hash.Byte(occupant.W);
			hash.Byte(occupant.H);
			hash.Byte(occupant.Rotation);
			hash.Byte(occupant.Variant);
		}
		return hash.Value();
	}

}
