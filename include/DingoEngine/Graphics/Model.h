#pragma once
#include "DingoEngine/Graphics/Mesh.h"
#include "DingoEngine/Graphics/Material.h"
#include "DingoEngine/Graphics/Texture.h"
#include "DingoEngine/Graphics/Skeleton.h"
#include "DingoEngine/Graphics/AnimationClip.h"

#include <filesystem>
#include <memory>
#include <string_view>
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
		// A relative filepath is looked up under the asset root first, then the working
		// directory.
		static Model* LoadFromFile(const std::filesystem::path& filepath);

	public:
		Model() = default;
		// Routes through Destroy() so `delete model` without a prior Destroy() still frees
		// the submeshes, materials and textures. Both are idempotent.
		~Model();

		void Destroy();

		const std::vector<SubMesh>& GetSubMeshes()    const { return m_SubMeshes; }
		uint32_t                    GetSubMeshCount()  const { return static_cast<uint32_t>(m_SubMeshes.size()); }

		// A model with bones loads its skeleton, skins and clips; one with a skeleton and clips
		// but no meshes is a clip library. Both count as skinned. Otherwise the skeleton is
		// null and the meshes are pre-transformed into model space, as before v0.8.
		bool            IsSkinned()   const { return m_Skeleton != nullptr; }
		const Skeleton* GetSkeleton() const { return m_Skeleton.get(); }

		uint32_t       GetAnimationCount() const { return static_cast<uint32_t>(m_Animations.size()); }
		// nullptr when out of range or not found. The Model owns its clips.
		AnimationClip* GetAnimation(uint32_t index) const;
		AnimationClip* FindAnimation(std::string_view name) const;

		// Adds clip events from a text file, one per line: `<clip> <seconds> <event>` for an instant,
		// `<clip> <begin>..<end> <event>` for a range; `#` starts a comment, and a name with spaces
		// goes in double quotes. A line that doesn't parse, or names a clip the model lacks, warns
		// with its number and is skipped. False when the file can't be read. LoadFromFile reads
		// `<model stem>.events` beside a model with clips by itself.
		bool LoadEvents(const std::filesystem::path& filepath);

	private:
		std::vector<SubMesh> m_SubMeshes;
		// Deduplicated diffuse textures, one entry per distinct image file.
		std::vector<Texture*> m_Textures;
		std::unique_ptr<Skeleton> m_Skeleton;
		std::vector<std::unique_ptr<AnimationClip>> m_Animations;
	};

}
