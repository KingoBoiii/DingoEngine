#include "depch.h"
#include "StaticGeometry.h"
#include "DingoEngine/Graphics/Renderer3D.h"

#include <algorithm>
#include <cstring>

namespace Dingo::Internal
{

	namespace
	{
		// Bitwise, so a NaN the renderer sanitises doesn't rebake every frame.
		template<typename T>
		bool Same(const T& a, const T& b)
		{
			return std::memcmp(&a, &b, sizeof(T)) == 0;
		}

		bool SameSurface(const std::optional<SurfaceOverride>& a, const std::optional<SurfaceOverride>& b)
		{
			if (a.has_value() != b.has_value())
				return false;
			return !a || (Same(a->EmissiveColor, b->EmissiveColor) && Same(a->EmissiveStrength, b->EmissiveStrength) &&
				Same(a->Roughness, b->Roughness) && Same(a->Specular, b->Specular));
		}
	}

	StaticGeometry::Bake* StaticGeometry::Find(Renderer3D& renderer)
	{
		++m_Uses;
		std::erase_if(m_Bakes, [](const Bake& bake) { return !bake.Batch->IsRendererAlive(); });
		for (Bake& bake : m_Bakes)
		{
			if (renderer.OwnsStaticBatch(*bake.Batch))
			{
				bake.LastUsed = m_Uses;
				++bake.Pass;
				bake.Tracked = 0;
				return &bake;
			}
		}
		return nullptr;
	}

	StaticGeometry::Bake& StaticGeometry::Create(Renderer3D& renderer)
	{
		if (m_Bakes.size() >= k_MaxBakes)
		{
			if (!m_EvictionWarned)
			{
				DE_CORE_WARN("Scene: drawn by more than {} Renderer3Ds; their static geometry is rebaked as they take turns.", k_MaxBakes);
				m_EvictionWarned = true;
			}
			m_Bakes.erase(std::min_element(m_Bakes.begin(), m_Bakes.end(), [](const Bake& a, const Bake& b) { return a.LastUsed < b.LastUsed; }));
		}
		Bake& bake = m_Bakes.emplace_back();
		bake.Batch = renderer.CreateStaticBatch();
		bake.LastUsed = m_Uses;
		bake.Pass = 1;
		return bake;
	}

	void StaticGeometry::Track(Bake& bake, entt::entity entity, const StaticMesh& mesh)
	{
		const size_t index = static_cast<size_t>(entt::to_entity(entity));
		if (bake.Entries.size() <= index)
			bake.Entries.resize(index + 1);
		Bake::Entry& entry = bake.Entries[index];
		if (entry.Id != k_InvalidStaticMesh && entry.Entity != entity)
		{
			bake.Batch->Remove(entry.Id);
			entry.Id = k_InvalidStaticMesh;
		}

		entry.Entity = entity;
		entry.Seen = bake.Pass;
		++bake.Tracked;
		if (entry.Id == k_InvalidStaticMesh)
		{
			entry.Id = bake.Batch->Add(mesh);
			return;
		}

		const StaticMesh& baked = *bake.Batch->Get(entry.Id);
		const bool moved = !Same(baked.Transform, mesh.Transform);
		if (!moved && baked.Mesh == mesh.Mesh && Same(baked.Color, mesh.Color) && baked.Material == mesh.Material && baked.Shadows == mesh.Shadows &&
			baked.ShadowGroups == mesh.ShadowGroups && SameSurface(baked.Surface, mesh.Surface))
			return;

		if (moved && !m_MovedWarned)
		{
			DE_CORE_WARN("Scene: a MeshRendererComponent with Static set moved (entity {}); its cell of the scene's static geometry is rebaked every frame it moves. Clear Static on meshes that move.",
				static_cast<uint32_t>(entity));
			m_MovedWarned = true;
		}
		bake.Batch->Update(entry.Id, mesh);
	}

	void StaticGeometry::Submit(Bake& bake, Renderer3D& renderer)
	{
		if (bake.Tracked != bake.Batch->GetMeshCount())
		{
			for (Bake::Entry& entry : bake.Entries)
			{
				if (entry.Id != k_InvalidStaticMesh && entry.Seen != bake.Pass)
				{
					bake.Batch->Remove(entry.Id);
					entry = {};
				}
			}
		}
		renderer.SubmitStaticBatch(*bake.Batch);
	}

}
