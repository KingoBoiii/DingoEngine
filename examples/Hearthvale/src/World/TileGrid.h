#pragma once
#include "Defs/CropDefs.h"

#include <cstdint>
#include <vector>

namespace Dingo
{

	enum class Ground : uint8_t { Grass, Tilled, Watered, Path, Water, Blocked };

	enum class OccupantKind : uint8_t { Decoration, Building, Landmark };

	// Whether a player-placed object may stand on this ground. Generation ignores it for rim decoration.
	constexpr bool CanOccupy(Ground ground)
	{
		return ground == Ground::Grass || ground == Ground::Tilled || ground == Ground::Watered || ground == Ground::Path;
	}

	inline constexpr uint32_t k_NoOccupant = 0;

	struct CropRecord
	{
		uint8_t Def = k_NoCrop;
		uint8_t Stage = 0;
		uint8_t DaysWatered = 0;
		int32_t PlantedDay = 0;

		bool Present() const { return Def != k_NoCrop; }
	};

	struct Tile
	{
		Ground GroundState = Ground::Grass;
		uint32_t Occupant = k_NoOccupant;
		CropRecord Crop;
	};

	// Origin is the minimum corner; W and H are the footprint after rotation.
	struct Occupant
	{
		uint32_t Id = k_NoOccupant;
		OccupantKind Kind = OccupantKind::Decoration;
		uint16_t DefId = 0;
		int16_t X = 0;
		int16_t Y = 0;
		uint8_t W = 1;
		uint8_t H = 1;
		uint8_t Rotation = 0;
		uint8_t Variant = 0;
	};

	class TileGrid
	{
	public:
		TileGrid(int width, int height);

		int GetWidth() const { return m_Width; }
		int GetHeight() const { return m_Height; }
		bool InBounds(int x, int y) const { return x >= 0 && y >= 0 && x < m_Width && y < m_Height; }

		const Tile& At(int x, int y) const { return m_Tiles[Index(x, y)]; }
		Tile& At(int x, int y) { return m_Tiles[Index(x, y)]; }

		Ground GetGround(int x, int y) const { return At(x, y).GroundState; }
		void SetGround(int x, int y, Ground ground) { At(x, y).GroundState = ground; }

		// Placement rule for players: in bounds, ground that can be occupied, no occupant and no crop on any tile.
		bool IsAreaFree(int x, int y, int w, int h) const;

		// Returns the new id, or k_NoOccupant when IsAreaFree fails.
		uint32_t AddOccupant(OccupantKind kind, uint16_t defId, int x, int y, int w, int h, int rotation, int variant);

		// Generation only: ignores the ground, still refuses out-of-bounds and occupied tiles.
		uint32_t AddOccupantUnchecked(OccupantKind kind, uint16_t defId, int x, int y, int w, int h, int rotation, int variant);

		bool RemoveOccupant(uint32_t id);
		const Occupant* FindOccupant(uint32_t id) const;
		const std::vector<Occupant>& GetOccupants() const { return m_Occupants; }

		int CountGround(Ground ground) const;
		int CountOccupants(OccupantKind kind) const;

		// FNV-1a over explicit little-endian fields, never over struct memory, so padding and platform do not matter.
		uint64_t Hash() const;

	private:
		size_t Index(int x, int y) const { return static_cast<size_t>(y) * static_cast<size_t>(m_Width) + static_cast<size_t>(x); }
		bool IsAreaEmpty(int x, int y, int w, int h) const;
		uint32_t Insert(OccupantKind kind, uint16_t defId, int x, int y, int w, int h, int rotation, int variant);

	private:
		int m_Width;
		int m_Height;
		std::vector<Tile> m_Tiles;
		std::vector<Occupant> m_Occupants;
		uint32_t m_NextOccupantId = 1;
	};

}
