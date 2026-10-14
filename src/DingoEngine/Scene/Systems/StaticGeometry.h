#pragma once

// Engine-internal: a scene's MeshRendererComponent::Static entities, kept in a StaticBatch per
// renderer that draws them, so EnTT stays a private implementation detail.

#include "DingoEngine/Graphics/StaticBatch.h"

#include <entt/entt.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace Dingo
{

	class Renderer3D;

	namespace Internal
	{

		class StaticGeometry
		{
		public:
			struct Bake
			{
				struct Entry
				{
					entt::entity Entity = entt::null;
					StaticMeshId Id = k_InvalidStaticMesh;
					uint64_t Seen = 0;
					// A new mesh or material can reuse a freed one's address.
					uint64_t MeshId = 0;
					uint64_t MaterialId = 0;
				};

				std::shared_ptr<StaticBatch> Batch;
				std::vector<Entry> Entries; // by entity index
				uint64_t Pass = 0;
				uint32_t Tracked = 0; // entities tracked this pass
				uint64_t LastUsed = 0;
			};

			// The renderer's bake with a new pass begun, or null before its first static mesh.
			Bake* Find(Renderer3D& renderer);
			// At most k_MaxBakes are kept, the least recently used dropped first.
			Bake& Create(Renderer3D& renderer);
			// The entity's draw state this pass: added, or updated when it changed.
			void Track(Bake& bake, entt::entity entity, const StaticMesh& mesh);
			// Removes the entities not tracked this pass and submits the batch.
			void Submit(Bake& bake, Renderer3D& renderer);

			static constexpr size_t k_MaxBakes = 4;

		private:
			std::vector<Bake> m_Bakes;
			uint64_t m_Uses = 0;
			bool m_MovedWarned = false;
			bool m_EvictionWarned = false;
		};

	}

}
