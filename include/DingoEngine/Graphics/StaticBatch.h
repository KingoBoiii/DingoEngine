#pragma once
#include "DingoEngine/Graphics/Enums/ShadowCasting.h"
#include "DingoEngine/Graphics/SurfaceOverride.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <optional>

namespace Dingo
{

	class Mesh;
	class Material;
	class Renderer3D;

	namespace Internal
	{
		struct StaticBatchData;
	}

	// One mesh of a StaticBatch, placed in world space: what Renderer3D::SubmitMesh takes, less the
	// cull distance.
	struct StaticMesh
	{
		const Dingo::Mesh* Mesh = nullptr;
		glm::mat4 Transform{ 1.0f };
		glm::vec4 Color{ 1.0f };
		Dingo::Material* Material = nullptr; // null = the renderer's default lit material
		Dingo::ShadowCasting Shadows = Dingo::ShadowCasting::On;
		uint32_t ShadowGroups = 1;
		std::optional<SurfaceOverride> Surface;

		StaticMesh& SetMesh(const Dingo::Mesh* mesh) { Mesh = mesh; return *this; }
		StaticMesh& SetTransform(const glm::mat4& transform) { Transform = transform; return *this; }
		StaticMesh& SetColor(const glm::vec4& color) { Color = color; return *this; }
		StaticMesh& SetMaterial(Dingo::Material* material) { Material = material; return *this; }
		StaticMesh& SetShadows(Dingo::ShadowCasting shadows) { Shadows = shadows; return *this; }
		StaticMesh& SetShadowGroups(uint32_t groups) { ShadowGroups = groups; return *this; }
		StaticMesh& SetSurface(const SurfaceOverride& surface) { Surface = surface; return *this; }
	};

	struct StaticBatchParams
	{
		// The side of the world grid the batch is split into. A mesh belongs to the cell its bounds'
		// centre falls in; each cell is drawn, culled and rebaked on its own.
		float CellSize = 32.0f;

		StaticBatchParams& SetCellSize(float size) { CellSize = size; return *this; }
	};

	using StaticMeshId = uint32_t;
	constexpr StaticMeshId k_InvalidStaticMesh = ~0u;

	// Meshes that don't move, transformed into world space once and kept on the GPU, so a frame
	// submitting them costs a cull of each cell instead of transforming every vertex
	// (Renderer3D::CreateStaticBatch, Renderer3D::SubmitStaticBatch). The picture is the one SubmitMesh
	// draws for the same meshes batched on the CPU (instanced, within the instancing tolerance). A
	// change (Add, Update, Remove, or a mesh whose Mesh::GetId changed, as a hot reload does) rebakes
	// only the cells it touches, at the next submission. Meshes and materials must outlive their
	// entries. Meshes the batch can't bake, those of a translucent, skinned-only or custom-shader
	// MaterialParams::Instancing material, larger than one draw or with a non-affine transform, are
	// submitted through SubmitMesh every time instead. An entry with a null or empty mesh counts but
	// draws nothing. Ids are reused after Remove and Clear, so a removed id can name a later mesh.
	class StaticBatch : public std::enable_shared_from_this<StaticBatch>
	{
	public:
		~StaticBatch();
		StaticBatch(const StaticBatch&) = delete;
		StaticBatch& operator=(const StaticBatch&) = delete;

		StaticMeshId Add(const StaticMesh& mesh);
		// False for an id the batch doesn't hold.
		bool Update(StaticMeshId id, const StaticMesh& mesh);
		bool Remove(StaticMeshId id);
		void Clear();
		// Valid until the next Add.
		const StaticMesh* Get(StaticMeshId id) const;

		uint32_t GetMeshCount() const;
		// Cells holding at least one mesh, as of the last submission.
		uint32_t GetCellCount() const;
		// Cells baked since the batch was made, for tooling and tests.
		uint64_t GetBakeCount() const;
		// False once the renderer that made it has shut down: it draws nothing anywhere.
		bool IsRendererAlive() const;

	private:
		explicit StaticBatch(std::unique_ptr<Internal::StaticBatchData> data);

		std::unique_ptr<Internal::StaticBatchData> m_Data;

		friend class Renderer3D;
	};

}
