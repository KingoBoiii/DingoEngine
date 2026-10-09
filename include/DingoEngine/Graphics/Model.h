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

	struct ModelLoadParams
	{
		// Loads the skeleton and clips (with the .events sidecar) but no meshes, materials or
		// textures: a clip library that carries a preview mesh. The skeleton is the one a full
		// load builds. A file without bones has no clips to keep, so it fails to load.
		bool ClipsOnly = false;

		ModelLoadParams& SetClipsOnly(bool clipsOnly)
		{
			ClipsOnly = clipsOnly;
			return *this;
		}

		bool operator==(const ModelLoadParams&) const = default;
	};

	class Model
	{
	public:
		// Returns nullptr on failure (error is logged). Caller owns the returned Model.
		// A relative filepath is looked up under the asset root first, then the working
		// directory.
		static Model* LoadFromFile(const std::filesystem::path& filepath);
		// Reload() reads the file again with the same params.
		static Model* LoadFromFile(const std::filesystem::path& filepath, const ModelLoadParams& params);

	public:
		Model() = default;
		// Routes through Destroy() so `delete model` without a prior Destroy() still frees
		// the submeshes, materials and textures. Both are idempotent.
		~Model();

		void Destroy();

		// Reads the file again, with its .events sidecar, into this same Model. The objects a game
		// may hold stay valid (hold Mesh*, Material* and Texture*, not SubMesh entries, which a
		// reload that adds submeshes moves):
		// - Meshes and materials by submesh index; extra submeshes are added, missing ones emptied.
		//   Each Mesh gets a new GetId().
		// - Textures by file path, re-read in place if the image changed; one the file no longer
		//   uses stays alive.
		// - Clips by name; a missing one becomes zero-length, a new one is added. Events added in
		//   code are dropped.
		// - The Skeleton, while every joint keeps its name and parent. Otherwise a new Skeleton
		//   replaces it, the old one stays alive, and a Scene's animators on it start over.
		// False, with nothing changed, when the file doesn't load. The AssetManager calls this for
		// a managed model, on Reload and on hot-reload.
		bool Reload();
		// Replaces every clip's events with the .events sidecar's (none when it's gone), leaving the
		// rest alone: what a hot-reload of the sidecar alone does. False, with nothing changed, when
		// the sidecar exists but can't be read.
		bool ReloadEvents();
		// Counts the successful reloads, ReloadEvents included.
		uint32_t GetGeneration() const { return m_Generation; }
		// The absolute path LoadFromFile read; empty for a Model built by hand.
		const std::filesystem::path& GetFilePath() const { return m_FilePath; }
		const ModelLoadParams& GetLoadParams() const { return m_LoadParams; }

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
		struct ModelTexture
		{
			std::string Path;
			Texture* Image = nullptr;
			std::filesystem::file_time_type WriteTime{};
			// Still owned by the model being reloaded: a temporary model mid-reload must not free it.
			bool Borrowed = false;
		};

		// `refresh` is a model being reloaded, whose textures the new file still uses are refreshed
		// in place and reused.
		static Model* Load(const std::filesystem::path& filepath, const ModelLoadParams& params, Model* refresh);
		void Adopt(Model& fresh);

	private:
		std::vector<SubMesh> m_SubMeshes;
		// Deduplicated diffuse textures, one entry per distinct image file.
		std::vector<ModelTexture> m_Textures;
		std::unique_ptr<Skeleton> m_Skeleton;
		std::vector<std::unique_ptr<AnimationClip>> m_Animations;
		// Replaced by a reload that changed the joints; animators may still be bound to them.
		std::vector<std::unique_ptr<Skeleton>> m_RetiredSkeletons;
		std::filesystem::path m_FilePath;
		ModelLoadParams m_LoadParams;
		uint32_t m_Generation = 0;
	};

}
