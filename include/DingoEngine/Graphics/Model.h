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

	/// One mesh of a Model with the material it draws with. The Model owns all of it.
	struct SubMesh
	{
		Dingo::Mesh*     Mesh     = nullptr; ///< The geometry, owned by the Model.
		/// The material built from the file's, owned by the Model.
		Dingo::Material* Material = nullptr;
		/// Borrowed, not owned: submeshes sharing one image share one Texture, which the
		/// Model owns and frees in Destroy(). Null when the material has no diffuse image.
		Texture*  DiffuseTexture = nullptr;
		/// The material's base colour: glTF's baseColorFactor, else the diffuse colour with the
		/// material's opacity as alpha; white when the file has neither, though some importers fill
		/// in a default (OBJ's is 0.6 grey without Kd). Nothing applies it: pass it as the mesh
		/// colour.
		glm::vec4 BaseColor      = glm::vec4(1.0f);
	};

	/// Options for Model::LoadFromFile; the fluent setter sets the field of the same name.
	struct ModelLoadParams
	{
		/// Loads the skeleton and clips (with the .events sidecar) but no meshes, materials or
		/// textures: a clip library that carries a preview mesh. The skeleton is the one a full
		/// load builds. A file without bones has no clips to keep, so it fails to load.
		bool ClipsOnly = false;

		/// Sets ClipsOnly and returns *this, for chaining.
		ModelLoadParams& SetClipsOnly(bool clipsOnly)
		{
			ClipsOnly = clipsOnly;
			return *this;
		}

		/// True when every option matches.
		bool operator==(const ModelLoadParams&) const = default;
	};

	/// A model file (glTF, FBX, OBJ and the other formats assimp reads) loaded into submeshes,
	/// materials, textures and, for a skinned file, a Skeleton and its animation clips.
	class Model
	{
	public:
		/// Loads the model and creates its textures, reading the `<stem>.events` sidecar if the
		/// file has clips. Returns nullptr on failure (error is logged). Caller owns the returned
		/// Model. A relative filepath is looked up under the asset root first, then the working
		/// directory.
		[[nodiscard]] static Model* LoadFromFile(const std::filesystem::path& filepath);
		/// As above with options. Reload() reads the file again with the same params.
		[[nodiscard]] static Model* LoadFromFile(const std::filesystem::path& filepath, const ModelLoadParams& params);

	public:
		/// An empty model with no submeshes, skeleton or file; LoadFromFile makes real ones.
		Model() = default;
		/// Routes through Destroy() so `delete model` without a prior Destroy() still frees
		/// the submeshes, materials and textures. Both are idempotent.
		~Model();

		/// Frees the submeshes' meshes and materials, the textures and the skeleton and clips, so
		/// every pointer taken from the model dies. GPU teardown: do it in Layer::OnDetach, never
		/// while a frame that draws it is in flight.
		void Destroy();

		/// Reads the file again, with its .events sidecar, into this same Model. The objects a game
		/// may hold stay valid (hold Mesh*, Material* and Texture*, not SubMesh entries, which a
		/// reload that adds submeshes moves):
		/// - Meshes and materials by submesh index; extra submeshes are added, missing ones
		///   emptied. Each Mesh gets a new GetId().
		/// - Textures by file path, re-read in place if the image changed; one the file no longer
		///   uses stays alive.
		/// - Clips by name; a missing one becomes zero-length, a new one is added. Events added in
		///   code are dropped.
		/// - The Skeleton, while every joint keeps its name and parent. Otherwise a new Skeleton
		///   replaces it, the old one stays alive, and a Scene's animators on it start over.
		/// False, with nothing changed, when the file doesn't load or the model wasn't loaded from
		/// a file. The AssetManager calls this for a managed model, on Reload and on hot-reload.
		bool Reload();
		/// Replaces every clip's events with the .events sidecar's (none when it's gone), leaving
		/// the rest alone: what a hot-reload of the sidecar alone does. False, with nothing
		/// changed, when the sidecar exists but can't be read, or the model wasn't loaded from a
		/// file.
		bool ReloadEvents();
		/// Counts the successful reloads, ReloadEvents included.
		uint32_t GetGeneration() const { return m_Generation; }
		/// The absolute path LoadFromFile read; empty for a Model built by hand.
		const std::filesystem::path& GetFilePath() const { return m_FilePath; }
		/// The options the model was loaded with, which Reload() reuses.
		const ModelLoadParams& GetLoadParams() const { return m_LoadParams; }

		/// The submeshes, in file order; empty for a clip library loaded with ClipsOnly.
		const std::vector<SubMesh>& GetSubMeshes()    const { return m_SubMeshes; }
		/// The number of submeshes in GetSubMeshes().
		uint32_t                    GetSubMeshCount()  const { return static_cast<uint32_t>(m_SubMeshes.size()); }

		/// A model with bones loads its skeleton, skins and clips; one with a skeleton and clips
		/// but no meshes is a clip library. Both count as skinned. Otherwise the skeleton is
		/// null and the meshes are pre-transformed into model space, as before v0.8.
		bool            IsSkinned()   const { return m_Skeleton != nullptr; }
		/// The skeleton the clips animate; null for a model without bones. The Model owns it.
		const Skeleton* GetSkeleton() const { return m_Skeleton.get(); }

		/// The number of clips; 0 for a model without bones.
		uint32_t       GetAnimationCount() const { return static_cast<uint32_t>(m_Animations.size()); }
		/// The clip at `index`; nullptr when out of range. The Model owns its clips.
		AnimationClip* GetAnimation(uint32_t index) const;
		/// The first clip with that name; nullptr when not found. The Model owns its clips.
		AnimationClip* FindAnimation(std::string_view name) const;

		/// Adds clip events from a text file, one per line: `<clip> <seconds> <event>` for an
		/// instant, `<clip> <begin>..<end> <event>` for a range; `#` starts a comment, and a name
		/// with spaces goes in double quotes. A line that doesn't parse, or names a clip the model
		/// lacks, warns with its number and is skipped. False when the file can't be read.
		/// LoadFromFile reads `<model stem>.events` beside a model with clips by itself.
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
