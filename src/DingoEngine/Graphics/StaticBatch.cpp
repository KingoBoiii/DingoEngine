#include "depch.h"
#include "DingoEngine/Graphics/StaticBatch.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "StaticBatchData.h"

#include <algorithm>

namespace Dingo::Internal
{

	uint32_t StaticBatchShared::Allocate(uint32_t count)
	{
		if (count == 0)
			return 0;
		for (auto it = FreeRanges.begin(); it != FreeRanges.end(); ++it)
		{
			if (it->second < count)
				continue;
			const uint32_t start = it->first;
			it->first += count;
			it->second -= count;
			if (it->second == 0)
				FreeRanges.erase(it);
			return start;
		}
		const uint32_t start = static_cast<uint32_t>(Records.size());
		Records.resize(Records.size() + count);
		return start;
	}

	void StaticBatchShared::Release(uint32_t start, uint32_t count)
	{
		if (count == 0)
			return;
		auto it = std::lower_bound(FreeRanges.begin(), FreeRanges.end(), std::make_pair(start, 0u));
		it = FreeRanges.insert(it, { start, count });
		if (it + 1 != FreeRanges.end() && it->first + it->second == (it + 1)->first)
		{
			it->second += (it + 1)->second;
			FreeRanges.erase(it + 1);
		}
		if (it != FreeRanges.begin() && (it - 1)->first + (it - 1)->second == it->first)
		{
			(it - 1)->second += it->second;
			it = FreeRanges.erase(it) - 1;
		}
		// The renderer uploads records up to the end of the vector, so a free tail is given back.
		if (!FreeRanges.empty() && FreeRanges.back().first + FreeRanges.back().second == Records.size())
		{
			Records.resize(FreeRanges.back().first);
			FreeRanges.pop_back();
		}
	}

	void StaticBatchData::MarkPending(StaticMeshId id)
	{
		StaticBatchEntry& entry = Entries[id];
		if (entry.Queued)
			return;
		entry.Queued = true;
		Pending.push_back(id);
	}

	void StaticBatchData::Detach(StaticMeshId id)
	{
		StaticBatchEntry& entry = Entries[id];
		if (entry.Cell != ~0u)
		{
			StaticCell& cell = Cells[entry.Cell];
			const auto it = std::lower_bound(cell.Members.begin(), cell.Members.end(), id);
			if (it != cell.Members.end() && *it == id)
				cell.Members.erase(it);
			cell.Dirty = true;
			entry.Cell = ~0u;
		}
		if (entry.Loose)
		{
			std::erase(Loose, id);
			entry.Loose = false;
		}
	}

	void StaticBatchData::ReleaseCells(uint64_t frame)
	{
		for (StaticCell& cell : Cells)
		{
			for (GraphicsBuffer* buffer : { cell.VertexBuffer, cell.IndexBuffer })
			{
				if (!buffer)
					continue;
				if (Shared->RendererAlive)
					Shared->Retired.emplace_back(buffer, frame);
				else
					DestroyAndDelete(buffer);
			}
			cell.VertexBuffer = nullptr;
			cell.IndexBuffer = nullptr;
			Shared->Release(cell.RecordStart, cell.RecordCount);
			cell.RecordStart = 0;
			cell.RecordCount = 0;
		}
		Cells.clear();
		CellLookup.clear();
		FreeCells.clear();
		OccupiedCells = 0;
	}

}

namespace Dingo
{

	StaticBatch::StaticBatch(std::unique_ptr<Internal::StaticBatchData> data)
		: m_Data(std::move(data))
	{
		m_Data->Shared->Live.push_back(m_Data.get());
	}

	StaticBatch::~StaticBatch()
	{
		m_Data->ReleaseCells(Renderer::GetFrameIndex());
		std::erase(m_Data->Shared->Live, m_Data.get());
	}

	StaticMeshId StaticBatch::Add(const StaticMesh& mesh)
	{
		Internal::StaticBatchData& data = *m_Data;
		StaticMeshId id;
		if (!data.FreeIds.empty())
		{
			id = data.FreeIds.back();
			data.FreeIds.pop_back();
		}
		else
		{
			id = static_cast<StaticMeshId>(data.Entries.size());
			data.Entries.emplace_back();
		}

		Internal::StaticBatchEntry& entry = data.Entries[id];
		entry = {};
		entry.Desc = mesh;
		entry.Live = true;
		++data.MeshCount;
		data.MarkPending(id);
		return id;
	}

	bool StaticBatch::Update(StaticMeshId id, const StaticMesh& mesh)
	{
		Internal::StaticBatchData& data = *m_Data;
		if (id >= data.Entries.size() || !data.Entries[id].Live)
			return false;
		data.Detach(id);
		data.Entries[id].Desc = mesh;
		data.MarkPending(id);
		return true;
	}

	bool StaticBatch::Remove(StaticMeshId id)
	{
		Internal::StaticBatchData& data = *m_Data;
		if (id >= data.Entries.size() || !data.Entries[id].Live)
			return false;
		data.Detach(id);
		Internal::StaticBatchEntry& entry = data.Entries[id];
		entry.Live = false;
		entry.Desc = {};
		data.FreeIds.push_back(id);
		--data.MeshCount;
		return true;
	}

	void StaticBatch::Clear()
	{
		Internal::StaticBatchData& data = *m_Data;
		data.ReleaseCells(Renderer::GetFrameIndex());
		data.Entries.clear();
		data.FreeIds.clear();
		data.Pending.clear();
		data.Loose.clear();
		data.MeshCount = 0;
	}

	const StaticMesh* StaticBatch::Get(StaticMeshId id) const
	{
		const Internal::StaticBatchData& data = *m_Data;
		return id < data.Entries.size() && data.Entries[id].Live ? &data.Entries[id].Desc : nullptr;
	}

	uint32_t StaticBatch::GetMeshCount() const
	{
		return m_Data->MeshCount;
	}

	uint32_t StaticBatch::GetCellCount() const
	{
		return m_Data->OccupiedCells;
	}

	uint64_t StaticBatch::GetBakeCount() const
	{
		return m_Data->BakeCount;
	}

	bool StaticBatch::IsRendererAlive() const
	{
		return m_Data->Shared->RendererAlive;
	}

}
