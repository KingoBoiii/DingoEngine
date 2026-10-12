#pragma once
#include "DingoEngine/Graphics/StaticBatch.h"
#include "DingoEngine/Graphics/Frustum.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Dingo
{
	class GraphicsBuffer;
}

namespace Dingo::Internal
{

	// Renderer3D's ObjectRecord, the 32 B std430 struct of Renderer3D_Lit.glsl.
	struct StaticObjectRecord
	{
		glm::vec4 Emissive{ 0.0f };
		glm::vec4 Surface{ 0.0f };
	};

	// What a renderer and every batch it made share, so a batch may outlive the renderer: the baked
	// surface overrides, which sit at fixed indices of the renderer's object buffer from 1 up, and the
	// buffers of rebaked or destroyed cells waiting for the GPU to finish with them.
	struct StaticBatchShared
	{
		std::vector<StaticObjectRecord> Records = std::vector<StaticObjectRecord>(1); // record 0 is never read
		std::vector<std::pair<uint32_t, uint32_t>> FreeRanges; // start, count; sorted, never adjacent
		uint64_t Revision = 0; // bumped whenever a record is written
		std::vector<std::pair<GraphicsBuffer*, uint64_t>> Retired;
		std::vector<StaticBatchData*> Live;
		bool RendererAlive = true;

		uint32_t Allocate(uint32_t count);
		void Release(uint32_t start, uint32_t count);
	};

	struct StaticBatchEntry
	{
		StaticMesh Desc;
		uint64_t MeshId = 0;
		bool Live = false;
		bool Loose = false;  // submitted through SubmitMesh every scene
		bool Queued = false; // in StaticBatchData::Pending
		uint32_t Cell = ~0u; // its StaticBatchData::Cells entry, ~0u when loose or unplaced
	};

	// One draw of a cell: a run of its meshes with one material and shadow setting, back to back in the
	// cell's buffers.
	struct StaticCellRange
	{
		Dingo::Material* Material = nullptr;
		ShadowCasting Shadows = ShadowCasting::On;
		uint32_t ShadowGroups = 1;
		AABB Bounds;            // its meshes' padded world bounds
		glm::vec3 CasterMin{ 0.0f }; // its vertices, exactly, when it casts
		glm::vec3 CasterMax{ 0.0f };
		uint32_t FirstIndex = 0;
		uint32_t IndexCount = 0;
		uint32_t Meshes = 0;
		uint32_t Surfaces = 0; // meshes drawn with their SurfaceOverride
	};

	struct StaticCell
	{
		glm::ivec3 Coord{ 0 };
		std::vector<uint32_t> Members; // entry ids, ascending
		std::vector<StaticCellRange> Ranges;
		GraphicsBuffer* VertexBuffer = nullptr;
		GraphicsBuffer* IndexBuffer = nullptr;
		uint32_t RecordStart = 0;
		uint32_t RecordCount = 0;
		bool Dirty = false;
	};

	struct CellCoordHash
	{
		size_t operator()(const glm::ivec3& coord) const
		{
			return static_cast<size_t>(coord.x) * 73856093u ^ static_cast<size_t>(coord.y) * 19349663u ^ static_cast<size_t>(coord.z) * 83492791u;
		}
	};

	struct StaticBatchData
	{
		std::shared_ptr<StaticBatchShared> Shared;
		float CellSize = 32.0f;

		std::vector<StaticBatchEntry> Entries;
		std::vector<StaticMeshId> FreeIds;
		std::vector<StaticMeshId> Pending; // added or changed since the last submission
		std::vector<StaticMeshId> Loose;
		std::vector<StaticCell> Cells;
		std::unordered_map<glm::ivec3, uint32_t, CellCoordHash> CellLookup;
		uint32_t MeshCount = 0;
		uint32_t OccupiedCells = 0;
		uint64_t BakeCount = 0;

		void MarkPending(StaticMeshId id);
		void Detach(StaticMeshId id);
		// Frees the cells' buffers (into Shared->Retired while the renderer lives) and their records.
		void ReleaseCells(uint64_t frame);
	};

}
