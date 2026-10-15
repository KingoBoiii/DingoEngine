#include "World/ValleyWorld.h"
#include "GameTuning.h"
#include "Look/ArtRegistry.h"

#include <algorithm>

namespace Dingo
{

	static_assert(static_cast<int>(ENGINE_STATIC_CELL / TILE_SIZE) % CELL_TILES == 0,
		"a game cell must nest in one engine static cell, or one edit can rebake two");

	namespace
	{
		bool SameOccupant(const Occupant& a, const Occupant& b)
		{
			return a.Kind == b.Kind && a.DefId == b.DefId && a.X == b.X && a.Y == b.Y && a.W == b.W && a.H == b.H &&
				a.Rotation == b.Rotation && a.Variant == b.Variant;
		}
	}

	ValleyWorld::ValleyWorld(Scene& scene, const TileGrid& grid, const ValleyWorldOptions& options)
		: m_Scene(scene), m_Grid(grid), m_Options(options), m_Dirty(grid.GetWidth(), grid.GetHeight(), CELL_TILES),
		m_CellStates(static_cast<size_t>(m_Dirty.GetCellCount())),
		m_Tiles(static_cast<size_t>(grid.GetWidth()) * static_cast<size_t>(grid.GetHeight()))
	{
		m_Dirty.MarkAll();
	}

	void ValleyWorld::Sync()
	{
		m_Last = {};
		if (m_DayRollPending)
		{
			m_DayRollPending = false;
			m_Dirty.MarkFarmCells(m_Grid);
			for (int cell = 0; cell < m_Dirty.GetCellCount(); ++cell)
			{
				if (m_CellStates[static_cast<size_t>(cell)].FarmEntities > 0)
					m_Dirty.MarkCell(cell);
			}
		}
		if (m_Dirty.Empty())
			return;

		m_Last.Cells = m_Dirty.Drain();
		std::vector<int> slotOf(static_cast<size_t>(m_Dirty.GetCellCount()), -1);
		for (size_t i = 0; i < m_Last.Cells.size(); ++i)
			slotOf[static_cast<size_t>(m_Last.Cells[i])] = static_cast<int>(i);

		std::vector<std::vector<const Occupant*>> wanted(m_Last.Cells.size());
		for (const Occupant& occupant : m_Grid.GetOccupants())
		{
			const int slot = slotOf[static_cast<size_t>(m_Dirty.CellOfOccupant(occupant))];
			if (slot >= 0)
				wanted[static_cast<size_t>(slot)].push_back(&occupant);
		}

		for (size_t i = 0; i < m_Last.Cells.size(); ++i)
			RebuildCell(m_Last.Cells[i], wanted[i]);
		m_CellsRebuiltTotal += m_Last.Cells.size();
	}

	void ValleyWorld::RebuildCell(int cell, const std::vector<const Occupant*>& occupants)
	{
		CellState& state = m_CellStates[static_cast<size_t>(cell)];
		const TileRect rect = m_Dirty.CellRect(cell);
		SyncGround(state, rect);

		state.FarmEntities = 0;
		for (int y = rect.Y; y < rect.Bottom(); ++y)
			for (int x = rect.X; x < rect.Right(); ++x)
				SyncTile(x, y, state);

		std::vector<uint32_t> ids;
		ids.reserve(occupants.size());
		for (const Occupant* occupant : occupants)
		{
			ids.push_back(occupant->Id);
			const auto it = m_Occupants.find(occupant->Id);
			if (it != m_Occupants.end())
			{
				if (SameOccupant(it->second.Snapshot, *occupant))
					continue;
				Destroy(it->second.Handle);
			}
			m_Occupants[occupant->Id] = OccupantVisual{ SpawnOccupant(*occupant), *occupant };
		}
		for (const uint32_t old : state.Occupants)
		{
			if (std::binary_search(ids.begin(), ids.end(), old))
				continue;
			const auto it = m_Occupants.find(old);
			if (it == m_Occupants.end())
				continue;
			Destroy(it->second.Handle);
			m_Occupants.erase(it);
		}
		state.Occupants = std::move(ids);
	}

	void ValleyWorld::SyncGround(CellState& state, const TileRect& rect)
	{
		MergeGroundRects(m_Grid, rect, m_RectScratch);
		if (m_RectScratch == state.Rects)
			return;

		for (Entity& entity : state.RectEntities)
			Destroy(entity);
		state.RectEntities.clear();
		for (const GroundRect& ground : m_RectScratch)
		{
			const float top = GroundTop(ground.Look);
			Entity entity = Art::Spawn(m_Scene, Art::ForGround(ground.Look), "Ground",
				glm::vec3((static_cast<float>(ground.X) + 0.5f * static_cast<float>(ground.W)) * TILE_SIZE, 0.5f * top,
					(static_cast<float>(ground.Y) + 0.5f * static_cast<float>(ground.H)) * TILE_SIZE));
			if (!entity)
				continue;
			entity.GetComponent<Transform3DComponent>().Scale = glm::vec3(static_cast<float>(ground.W) * TILE_SIZE, top, static_cast<float>(ground.H) * TILE_SIZE);
			Finish(entity, true, ShadowCasting::Off);
			state.RectEntities.push_back(entity);
		}
		state.Rects = m_RectScratch;
	}

	void ValleyWorld::SyncTile(int x, int y, CellState& state)
	{
		const Tile& tile = m_Grid.At(x, y);
		TileVisual& visual = m_Tiles[TileIndex(x, y)];
		const float cx = (static_cast<float>(x) + 0.5f) * TILE_SIZE;
		const float cz = (static_cast<float>(y) + 0.5f) * TILE_SIZE;
		const bool soil = IsSoil(tile.GroundState);

		if (soil)
		{
			const bool watered = tile.GroundState == Ground::Watered;
			if (!visual.Soil)
			{
				visual.Soil = Art::SpawnOnGround(m_Scene, watered ? Art::SOIL_WATERED : Art::SOIL, "Soil", cx, cz, GROUND_TOP);
				visual.Watered = watered;
				Finish(visual.Soil, true, ShadowCasting::Off);
			}
			else if (visual.Watered != watered)
			{
				// Colour only, from two fixed entries: the scene compares it bitwise, so this rebakes the cell quietly.
				if (const ArtEntry* entry = Art::Find(watered ? Art::SOIL_WATERED : Art::SOIL))
				{
					visual.Soil.GetComponent<MeshRendererComponent>().Color = entry->Color;
					visual.Watered = watered;
					++m_Last.Recoloured;
				}
			}
			++state.FarmEntities;
		}
		else if (visual.Soil)
		{
			Destroy(visual.Soil);
		}

		const CropRecord& crop = tile.Crop;
		if (crop.Present())
		{
			if (visual.Crop && (visual.CropDef != crop.Def || visual.CropStage != crop.Stage))
				Destroy(visual.Crop);
			if (!visual.Crop)
			{
				visual.Crop = Art::SpawnOnGround(m_Scene, Art::ForCrop(static_cast<CropId>(crop.Def), crop.Stage), "Crop", cx, cz, soil ? SOIL_TOP : GROUND_TOP);
				visual.CropDef = crop.Def;
				visual.CropStage = crop.Stage;
				Finish(visual.Crop, true, ShadowCasting::On);
			}
			++state.FarmEntities;
		}
		else if (visual.Crop)
		{
			Destroy(visual.Crop);
		}
	}

	Entity ValleyWorld::SpawnOccupant(const Occupant& occupant)
	{
		std::string_view id;
		const char* name = "Decoration";
		float scale = 1.0f;
		bool isStatic = true;
		switch (occupant.Kind)
		{
			case OccupantKind::Decoration:
				id = Art::ForDecor(static_cast<DecorKind>(occupant.DefId));
				scale = DECOR_VARIANT_SCALE[(std::min)(static_cast<int>(occupant.Variant), DECOR_VARIANTS - 1)];
				isStatic = false;
				break;
			case OccupantKind::Building:
				id = Art::ForBuild(static_cast<BuildId>(occupant.DefId));
				name = "Building";
				break;
			case OccupantKind::Landmark:
				id = Art::ForLandmark(static_cast<Landmark>(occupant.DefId));
				name = "Landmark";
				break;
		}

		const float x = (static_cast<float>(occupant.X) + 0.5f * static_cast<float>(occupant.W)) * TILE_SIZE;
		const float z = (static_cast<float>(occupant.Y) + 0.5f * static_cast<float>(occupant.H)) * TILE_SIZE;
		const float ground = GroundTop(GroundLookOf(m_Grid.GetGround(occupant.X + occupant.W / 2, occupant.Y + occupant.H / 2)));
		Entity entity = Art::SpawnOnGround(m_Scene, id, name, x, z, ground, occupant.Rotation, scale);
		Finish(entity, isStatic, ShadowCasting::On);
		return entity;
	}

	void ValleyWorld::Finish(Entity entity, bool isStatic, ShadowCasting shadows)
	{
		if (!entity)
			return;
		const bool fixedInPlace = isStatic && m_Options.Static;
		Art::ForEachMesh(entity, [&](MeshRendererComponent& mesh) {
			mesh.Static = fixedInPlace;
			mesh.Shadows = shadows;
		});
		++m_Last.Created;
	}

	void ValleyWorld::Destroy(Entity& entity)
	{
		if (entity)
		{
			m_Scene.DestroyEntity(entity);
			++m_Last.Destroyed;
		}
		entity = {};
	}

}
