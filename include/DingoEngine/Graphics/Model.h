#pragma once
#include "DingoEngine/Graphics/Mesh.h"
#include "DingoEngine/Graphics/Material.h"
#include "DingoEngine/Graphics/Texture.h"

#include <filesystem>
#include <vector>

namespace Dingo
{

	struct SubMesh
	{
		Mesh*     MeshData       = nullptr;
		Material* Mat            = nullptr;
		// Borrowed, not owned: submeshes sharing one image share one Texture, which the
		// Model owns and frees in Destroy().
		Texture*  DiffuseTexture = nullptr;
	};

	class Model
	{
	public:
		// Returns nullptr on failure (error is logged). Caller owns the returned Model.
		static Model* LoadFromFile(const std::filesystem::path& filepath);

	public:
		Model() = default;
		// Routes through Destroy() so `delete model` without a prior Destroy() still frees
		// the submeshes, materials and textures. Both are idempotent.
		~Model();

		void Destroy();

		const std::vector<SubMesh>& GetSubMeshes()    const { return m_SubMeshes; }
		uint32_t                    GetSubMeshCount()  const { return static_cast<uint32_t>(m_SubMeshes.size()); }

	private:
		std::vector<SubMesh> m_SubMeshes;
		// Deduplicated diffuse textures, one entry per distinct image file.
		std::vector<Texture*> m_Textures;
	};

}
