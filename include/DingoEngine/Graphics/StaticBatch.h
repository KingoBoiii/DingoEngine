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

	/// One mesh of a StaticBatch, placed in world space: what Renderer3D::SubmitMesh takes, less
	/// the cull distance.
	struct StaticMesh
	{
		const Dingo::Mesh* Mesh = nullptr; ///< Not owned; outlives the entry. Null draws nothing.
		glm::mat4 Transform{ 1.0f }; ///< Model-to-world matrix, baked in at the next submission.
		glm::vec4 Color{ 1.0f }; ///< RGBA vertex colour, as SubmitMesh's color.
		/// Not owned; must outlive the entry. Null is the renderer's default lit material.
		Dingo::Material* Material = nullptr;
		Dingo::ShadowCasting Shadows = Dingo::ShadowCasting::On; ///< Shadow casting for this mesh.
		uint32_t ShadowGroups = 1; ///< Caster-group bit mask, as SubmitMesh's shadowGroups.
		/// Per-mesh emissive, roughness and specular; empty uses the material's own.
		std::optional<SurfaceOverride> Surface;

		/// Each setter sets the field of the same name and returns *this, for chaining.
		///@{
		StaticMesh& SetMesh(const Dingo::Mesh* mesh) { Mesh = mesh; return *this; }
		StaticMesh& SetTransform(const glm::mat4& transform) { Transform = transform; return *this; }
		StaticMesh& SetColor(const glm::vec4& color) { Color = color; return *this; }
		StaticMesh& SetMaterial(Dingo::Material* material) { Material = material; return *this; }
		StaticMesh& SetShadows(Dingo::ShadowCasting shadows) { Shadows = shadows; return *this; }
		StaticMesh& SetShadowGroups(uint32_t groups) { ShadowGroups = groups; return *this; }
		StaticMesh& SetSurface(const SurfaceOverride& surface) { Surface = surface; return *this; }
		///@}
	};

	/// Settings for Renderer3D::CreateStaticBatch.
	struct StaticBatchParams
	{
		/// The side of the world grid the batch is split into, in world units. A mesh belongs to
		/// the cell its bounds' centre falls in; each cell is drawn, culled and rebaked on its own.
		/// A value that is not finite or not above 0 falls back to 32.
		float CellSize = 32.0f;

		/// Sets CellSize and returns *this, for chaining.
		StaticBatchParams& SetCellSize(float size) { CellSize = size; return *this; }
	};

	/// Handle to one mesh of a StaticBatch, from StaticBatch::Add.
	using StaticMeshId = uint32_t;
	/// An id no batch ever hands out.
	constexpr StaticMeshId k_InvalidStaticMesh = ~0u;

	/// Meshes that don't move, transformed into world space once and kept on the GPU, so a frame
	/// submitting them costs a cull of each cell instead of transforming every vertex
	/// (Renderer3D::CreateStaticBatch, Renderer3D::SubmitStaticBatch). The picture is the one
	/// SubmitMesh draws for the same meshes batched on the CPU (instanced, within the instancing
	/// tolerance). A change (Add, Update, Remove, or a mesh whose Mesh::GetId changed, as a hot
	/// reload does) rebakes only the cells it touches, at the next submission. Meshes and materials
	/// must outlive their entries. Meshes the batch can't bake, those of a translucent,
	/// skinned-only or custom-shader MaterialParams::Instancing material, larger than one draw or
	/// with a non-affine transform, are submitted through SubmitMesh every time instead. An entry
	/// with a null or empty mesh counts but draws nothing. Ids are reused after Remove and Clear,
	/// so a removed id can name a later mesh.
	class StaticBatch : public std::enable_shared_from_this<StaticBatch>
	{
	public:
		/// Releases the batch's GPU cells (freed once the frames using them are done).
		~StaticBatch();
		/// Not copyable: the batch owns GPU cells.
		///@{
		StaticBatch(const StaticBatch&) = delete;
		StaticBatch& operator=(const StaticBatch&) = delete;
		///@}

		/// Adds a copy of the entry and returns its id; the mesh bakes at the next submission.
		StaticMeshId Add(const StaticMesh& mesh);
		/// Replaces the entry's contents and rebakes its cells at the next submission.
		/// False for an id the batch doesn't hold.
		bool Update(StaticMeshId id, const StaticMesh& mesh);
		/// Removes the entry and frees its id for reuse; false for an id the batch doesn't hold.
		bool Remove(StaticMeshId id);
		/// Removes every entry and releases the baked cells; all ids become invalid.
		void Clear();
		/// The entry for an id, or nullptr for an id the batch doesn't hold.
		/// Valid until the next Add.
		const StaticMesh* Get(StaticMeshId id) const;

		/// Number of entries, including those that draw nothing.
		uint32_t GetMeshCount() const;
		/// Cells holding at least one mesh, as of the last submission.
		uint32_t GetCellCount() const;
		/// Cells baked since the batch was made, for tooling and tests.
		uint64_t GetBakeCount() const;
		/// False once the renderer that made it has shut down: it draws nothing anywhere.
		bool IsRendererAlive() const;

	private:
		explicit StaticBatch(std::unique_ptr<Internal::StaticBatchData> data);

		std::unique_ptr<Internal::StaticBatchData> m_Data;

		friend class Renderer3D;
	};

}
