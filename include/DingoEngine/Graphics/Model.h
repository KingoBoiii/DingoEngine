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
		Texture*  DiffuseTexture = nullptr;
	};

	class Model
	{
	public:
		// Returns nullptr on failure (error is logged). Caller owns the returned Model.
		static Model* LoadFromFile(const std::filesystem::path& filepath);

	public:
		Model() = default;

		void Destroy();

		const std::vector<SubMesh>& GetSubMeshes()    const { return m_SubMeshes; }
		uint32_t                    GetSubMeshCount()  const { return static_cast<uint32_t>(m_SubMeshes.size()); }

	private:
		std::vector<SubMesh> m_SubMeshes;
	};

}
