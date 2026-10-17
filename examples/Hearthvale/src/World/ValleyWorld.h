#pragma once
#include <DingoEngine.h>

#include "World/TileGrid.h"
#include "World/WorldCells.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Dingo
{

	struct ValleyWorldOptions
	{
		bool Static = true;     // false under --no-static
	};

	struct WorldSyncStats
	{
		std::vector<int> Cells;  // game cells rebuilt by the latest Sync, ascending
		int Created = 0;
		int Destroyed = 0;
		int Recoloured = 0;      // soil entities whose colour changed in place
	};

	// Derives the valley's entities from the grid, which stays the truth. Edits mark tiles; Sync rebuilds each
	// marked 16 x 16 cell once and touches only what changed, so an unchanged cell writes identical draw state
	// and the engine does not rebake it. Ground, soil, crops, buildables and landmarks are Static; decoration is
	// dynamic so its thousands of copies instance. GPU side: never constructed headless.
	class ValleyWorld
	{
	public:
		ValleyWorld(Scene& scene, const TileGrid& grid, const ValleyWorldOptions& options);
		ValleyWorld(const ValleyWorld&) = delete;
		ValleyWorld& operator=(const ValleyWorld&) = delete;

		void MarkTile(int x, int y) { m_Dirty.MarkTile(x, y); }
		void MarkArea(int x, int y, int w, int h) { m_Dirty.MarkArea(x, y, w, h); }
		void MarkAll() { m_Dirty.MarkAll(); }
		// Drops every occupant visual and rebuilds all cells; for a replaced game, whose occupant ids restart at 1.
		void Reset();
		// Queues every cell a day roll can change (crops and soil); the director's day roll calls it.
		void OnDayRoll() { m_DayRollPending = true; }

		// Once a frame, after the simulation and before the scene renders.
		void Sync();

		const WorldSyncStats& GetLastSync() const { return m_Last; }
		uint64_t GetCellsRebuiltTotal() const { return m_CellsRebuiltTotal; }
		const DirtyCells& GetCells() const { return m_Dirty; }
		bool IsStatic() const { return m_Options.Static; }
		bool HasOccupantVisual(uint32_t occupantId) const;
		size_t GetOccupantVisualCount() const { return m_Occupants.size(); }

	private:
		struct CellState
		{
			std::vector<GroundRect> Rects;
			std::vector<Entity> RectEntities;
			std::vector<uint32_t> Occupants;   // ascending ids
			int FarmEntities = 0;
		};

		struct TileVisual
		{
			Entity Soil;
			Entity Crop;
			bool Watered = false;
			uint8_t CropDef = k_NoCrop;
			uint8_t CropStage = 0;
		};

		struct OccupantVisual
		{
			Entity Handle;
			Occupant Snapshot;
		};

		void RebuildCell(int cell, const std::vector<const Occupant*>& occupants);
		void SyncGround(CellState& state, const TileRect& rect);
		void SyncTile(int x, int y, CellState& state);
		Entity SpawnOccupant(const Occupant& occupant);
		void Finish(Entity entity, bool isStatic, ShadowCasting shadows);
		void Destroy(Entity& entity);
		size_t TileIndex(int x, int y) const { return static_cast<size_t>(y) * static_cast<size_t>(m_Grid.GetWidth()) + static_cast<size_t>(x); }

	private:
		Scene& m_Scene;
		const TileGrid& m_Grid;
		ValleyWorldOptions m_Options;
		DirtyCells m_Dirty;
		std::vector<CellState> m_CellStates;
		std::vector<TileVisual> m_Tiles;
		std::unordered_map<uint32_t, OccupantVisual> m_Occupants;
		std::vector<GroundRect> m_RectScratch;
		WorldSyncStats m_Last;
		uint64_t m_CellsRebuiltTotal = 0;
		bool m_DayRollPending = false;
	};

}
