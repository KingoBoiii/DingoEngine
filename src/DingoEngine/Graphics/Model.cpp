#include "depch.h"
#include "DingoEngine/Graphics/Model.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Asset/AssetPath.h"
#include "DingoEngine/Core/FileSystem.h"
#include "DingoEngine/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/config.h>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>

namespace Dingo
{

	static constexpr uint32_t k_StaticImportFlags =
		aiProcess_Triangulate           |
		aiProcess_GenSmoothNormals      |
		aiProcess_FlipUVs               |
		aiProcess_CalcTangentSpace      |
		aiProcess_JoinIdenticalVertices |
		aiProcess_PreTransformVertices;

	static constexpr uint32_t k_SkinnedImportFlags =
		aiProcess_Triangulate           |
		aiProcess_GenSmoothNormals      |
		aiProcess_FlipUVs               |
		aiProcess_JoinIdenticalVertices |
		aiProcess_LimitBoneWeights;

	// Textures loaded so far for the model being loaded, keyed on the resolved path. Without
	// it a shared atlas is decoded AND uploaded once per submesh, each with its own command
	// list and queue submit, plus three filesystem probes. Load-scoped: the Model owns the
	// results, and managed models get the AssetManager's own dedup on top.
	struct CachedTexture
	{
		Texture* Image = nullptr;
		std::filesystem::file_time_type WriteTime{};
		bool Borrowed = false;
	};

	struct TextureCache
	{
		std::unordered_map<std::string, CachedTexture> Loaded;
		// A reloading model's textures by path: reused when the file still uses them, so a material
		// a game built from one keeps working.
		std::unordered_map<std::string, CachedTexture> Reusable;
		// Embedded images are keyed by the model's path and their own name, and dated by the model.
		std::filesystem::path ModelPath;
		std::filesystem::file_time_type ModelWriteTime{};
	};

	static std::filesystem::file_time_type WriteTimeOf(const std::filesystem::path& path)
	{
		std::error_code error;
		const std::filesystem::file_time_type time = std::filesystem::last_write_time(path, error);
		return error ? std::filesystem::file_time_type{} : time;
	}

	static TextureParams MakeImageParams(uint32_t width, uint32_t height, uint32_t channels, const uint8_t* data)
	{
		return TextureParams()
			.SetDebugName("Texture (File)")
			.SetWidth(width)
			.SetHeight(height)
			.SetDimension(TextureDimension::Texture2D)
			.SetFormat(channels == 4 ? TextureFormat::RGBA : TextureFormat::RGB)
			.SetIsRenderTarget(false)
			.SetInitialData(data);
	}

	static bool RefreshTexture(Texture& texture, const std::filesystem::path& path)
	{
		uint32_t width = 0, height = 0, channels = 0;
		const uint8_t* data = FileSystem::ReadImage(path, &width, &height, &channels, true, true);
		if (!data)
		{
			DE_CORE_WARN("Model: couldn't read '{}' again, so its texture keeps the old image", path.generic_string());
			return false;
		}

		texture.Reinitialize(MakeImageParams(width, height, channels, data));
		FileSystem::FreeImage(data);
		return true;
	}

	// Rows flipped like ReadImage's, so embedded and file images share one UV convention.
	static bool DecodeEmbeddedImage(const aiTexture* embedded, std::vector<uint8_t>& rgba, uint32_t& width, uint32_t& height)
	{
		if (embedded->mHeight == 0)
		{
			uint32_t channels = 0;
			const uint8_t* data = FileSystem::ReadImageFromMemory(embedded->pcData, embedded->mWidth, &width, &height, &channels, true, true);
			if (!data)
				return false;

			rgba.assign(data, data + static_cast<size_t>(width) * height * 4);
			FileSystem::FreeImage(data);
			return true;
		}

		width = embedded->mWidth;
		height = embedded->mHeight;
		rgba.resize(static_cast<size_t>(width) * height * 4);
		for (uint32_t y = 0; y < height; ++y)
		{
			const aiTexel* row = embedded->pcData + static_cast<size_t>(height - 1 - y) * width;
			uint8_t* out = rgba.data() + static_cast<size_t>(y) * width * 4;
			for (uint32_t x = 0; x < width; ++x)
			{
				out[x * 4 + 0] = row[x].r;
				out[x * 4 + 1] = row[x].g;
				out[x * 4 + 2] = row[x].b;
				out[x * 4 + 3] = row[x].a;
			}
		}
		return true;
	}

	static Texture* LoadEmbeddedTexture(const aiTexture* embedded, const std::string& name, TextureCache& textureCache)
	{
		const std::string key = textureCache.ModelPath.generic_string() + "#" + name;
		if (auto it = textureCache.Loaded.find(key); it != textureCache.Loaded.end())
			return it->second.Image;

		CachedTexture texture;
		auto reusable = textureCache.Reusable.find(key);
		if (reusable != textureCache.Reusable.end())
		{
			texture = reusable->second;
			texture.Borrowed = true;
			textureCache.Reusable.erase(reusable);
			if (texture.WriteTime == textureCache.ModelWriteTime)
			{
				textureCache.Loaded[key] = texture;
				return texture.Image;
			}
		}

		std::vector<uint8_t> rgba;
		uint32_t width = 0, height = 0;
		if (!DecodeEmbeddedImage(embedded, rgba, width, height))
		{
			DE_CORE_WARN("Model '{}': couldn't decode its embedded image '{}'{}", textureCache.ModelPath.filename().string(), name,
				texture.Image ? ", so its texture keeps the old image" : "; the submesh has no texture");
			if (texture.Image)
				textureCache.Loaded[key] = texture;
			return texture.Image;
		}

		const TextureParams params = MakeImageParams(width, height, 4, rgba.data());
		if (texture.Image)
			texture.Image->Reinitialize(params);
		else
			texture.Image = Texture::Create(params);
		texture.WriteTime = textureCache.ModelWriteTime;
		textureCache.Loaded[key] = texture;
		return texture.Image;
	}

	static Texture* LoadDiffuseTexture(aiMaterial* aiMat, const aiScene* scene, const std::filesystem::path& modelDir, TextureCache& textureCache)
	{
		if (aiMat->GetTextureCount(aiTextureType_DIFFUSE) == 0)
			return nullptr;

		aiString aiPath;
		aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &aiPath);
		std::string rawPath = aiPath.C_Str();

		if (!rawPath.empty() && rawPath[0] == '*')
		{
			if (const aiTexture* embedded = scene->GetEmbeddedTexture(rawPath.c_str()))
				return LoadEmbeddedTexture(embedded, rawPath, textureCache);

			DE_CORE_WARN("Model '{}': a material names embedded image '{}', which the file doesn't hold", textureCache.ModelPath.filename().string(), rawPath);
			return nullptr;
		}

		std::filesystem::path texPath = rawPath;

		std::filesystem::path candidates[3];
		uint32_t candidateCount = 0;
		if (texPath.is_absolute())
			candidates[candidateCount++] = texPath;
		candidates[candidateCount++] = modelDir / texPath;
		candidates[candidateCount++] = modelDir / texPath.filename();

		for (uint32_t i = 0; i < candidateCount; ++i)
		{
			const std::string key = candidates[i].generic_string();

			auto it = textureCache.Loaded.find(key);
			if (it != textureCache.Loaded.end())
				return it->second.Image;

			if (!std::filesystem::exists(candidates[i]))
				continue;

			CachedTexture texture;
			const std::filesystem::file_time_type writeTime = WriteTimeOf(candidates[i]);
			if (auto reusable = textureCache.Reusable.find(key); reusable != textureCache.Reusable.end())
			{
				texture = reusable->second;
				texture.Borrowed = true;
				textureCache.Reusable.erase(reusable);
				if (writeTime != texture.WriteTime && RefreshTexture(*texture.Image, candidates[i]))
					texture.WriteTime = writeTime;
			}
			else
			{
				texture.Image = Texture::CreateFromFile(candidates[i]);
				texture.WriteTime = writeTime;
			}
			if (!texture.Image)
				continue;

			textureCache.Loaded[key] = texture;
			return texture.Image;
		}

		// FBX embeds an image under its file name.
		if (const aiTexture* embedded = scene->GetEmbeddedTexture(rawPath.c_str()))
			return LoadEmbeddedTexture(embedded, rawPath, textureCache);

		return nullptr;
	}

	static std::vector<uint32_t> ReadIndices(const aiMesh* mesh)
	{
		std::vector<uint32_t> indices;
		indices.reserve(static_cast<size_t>(mesh->mNumFaces) * 3);
		for (uint32_t f = 0; f < mesh->mNumFaces; ++f)
		{
			const aiFace& face = mesh->mFaces[f];
			for (uint32_t j = 0; j < face.mNumIndices; ++j)
				indices.push_back(face.mIndices[j]);
		}
		return indices;
	}

	static void AssignMaterial(SubMesh& submesh, const aiMesh* mesh, const aiScene* scene,
	                           const std::filesystem::path& modelDir, TextureCache& textureCache)
	{
		aiMaterial* aiMat = scene->mMaterials[mesh->mMaterialIndex];
		submesh.Mat = Material::Create(MaterialParams()
			.SetDebugName(mesh->mName.C_Str()));

		Texture* diffuse = LoadDiffuseTexture(aiMat, scene, modelDir, textureCache);
		if (diffuse)
		{
			submesh.DiffuseTexture = diffuse;
			submesh.Mat->SetTexture(0, diffuse);
		}
	}

	static SubMesh ProcessMesh(aiMesh* mesh, const aiScene* scene,
	                           const std::filesystem::path& modelDir,
	                           TextureCache& textureCache)
	{
		std::vector<MeshVertex> vertices;
		vertices.reserve(mesh->mNumVertices);

		for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
		{
			MeshVertex v;
			v.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

			if (mesh->HasNormals())
				v.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
			else
				v.Normal = { 0.0f, 1.0f, 0.0f };

			if (mesh->HasTextureCoords(0))
				v.TexCoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
			else
				v.TexCoord = { 0.0f, 0.0f };

			vertices.push_back(v);
		}

		SubMesh submesh;
		submesh.MeshData = Mesh::Create(vertices, ReadIndices(mesh));
		AssignMaterial(submesh, mesh, scene, modelDir, textureCache);
		return submesh;
	}

	static void TraverseNode(aiNode* node, const aiScene* scene,
	                         const std::filesystem::path& modelDir,
	                         TextureCache& textureCache,
	                         std::vector<SubMesh>& outSubMeshes)
	{
		for (uint32_t i = 0; i < node->mNumMeshes; ++i)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			outSubMeshes.push_back(ProcessMesh(mesh, scene, modelDir, textureCache));
		}
		for (uint32_t i = 0; i < node->mNumChildren; ++i)
			TraverseNode(node->mChildren[i], scene, modelDir, textureCache, outSubMeshes);
	}

	static glm::mat4 ToGlm(const aiMatrix4x4& m)
	{
		return glm::transpose(glm::make_mat4(&m.a1));
	}

	template<typename Matrix>
	static bool IsFinite(const Matrix& m)
	{
		for (int c = 0; c < Matrix::length(); ++c)
		{
			for (int r = 0; r < Matrix::col_type::length(); ++r)
			{
				if (!std::isfinite(m[c][r]))
					return false;
			}
		}
		return true;
	}

	// A joint scaled to zero at rest (a hidden prop) has no inverse; identity keeps NaNs out
	// of every vertex that shares a palette with it.
	template<typename Matrix>
	static Matrix SafeInverse(const Matrix& m, bool& singular)
	{
		const Matrix inverse = glm::inverse(m);
		if (glm::determinant(m) != 0.0f && IsFinite(inverse))
			return inverse;

		singular = true;
		return Matrix(1.0f);
	}

	static glm::mat3 NormalMatrix(const glm::mat4& m, bool& singular)
	{
		return glm::transpose(SafeInverse(glm::mat3(m), singular));
	}

	// The cofactor matrix is the inverse transpose times the determinant: Renderer3D_Lit.glsl skins
	// normals with it, so this must stay the same formula.
	static glm::vec3 CofactorNormal(const glm::mat3& m, const glm::vec3& normal)
	{
		const glm::mat3 cofactor(glm::cross(m[1], m[2]), glm::cross(m[2], m[0]), glm::cross(m[0], m[1]));
		const float handedness = glm::dot(m[0], cofactor[0]) < 0.0f ? -1.0f : 1.0f;
		return cofactor * normal * handedness;
	}

	static glm::vec3 SafeNormalize(const glm::vec3& v)
	{
		const float length = glm::length(v);
		return length > 0.0f ? v / length : glm::vec3(0.0f, 1.0f, 0.0f);
	}

	static JointPose ToJointPose(const aiMatrix4x4& m)
	{
		aiVector3D scaling, position;
		aiQuaternion rotation;
		m.Decompose(scaling, rotation, position);

		JointPose pose;
		pose.Translation = { position.x, position.y, position.z };
		pose.Rotation    = glm::quat(rotation.w, rotation.x, rotation.y, rotation.z);
		pose.Scale       = { scaling.x, scaling.y, scaling.z };
		return pose;
	}

	static glm::mat4 GlobalTransform(const aiNode* node)
	{
		glm::mat4 global(1.0f);
		for (; node; node = node->mParent)
			global = ToGlm(node->mTransformation) * global;
		return global;
	}

	static bool HasBones(const aiScene* scene)
	{
		for (uint32_t i = 0; i < scene->mNumMeshes; ++i)
		{
			if (scene->mMeshes[i]->HasBones())
				return true;
		}
		return false;
	}

	static bool IsClipLibrary(const aiScene* scene)
	{
		return scene->mNumMeshes == 0 && scene->mNumAnimations > 0;
	}

	static bool HasFbxPivotNodes(const aiNode* node)
	{
		if (std::strstr(node->mName.C_Str(), "$AssimpFbx$"))
			return true;
		for (uint32_t i = 0; i < node->mNumChildren; ++i)
		{
			if (HasFbxPivotNodes(node->mChildren[i]))
				return true;
		}
		return false;
	}

	static const aiNode* CommonAncestor(const aiNode* a, const aiNode* b)
	{
		std::unordered_set<const aiNode*> ancestors;
		for (const aiNode* node = a; node; node = node->mParent)
			ancestors.insert(node);
		for (const aiNode* node = b; node; node = node->mParent)
		{
			if (ancestors.contains(node))
				return node;
		}
		return nullptr;
	}

	static void CollectSubtree(const aiNode* node, int32_t parent, std::vector<const aiNode*>& outNodes, std::vector<int32_t>& outParents)
	{
		const int32_t index = static_cast<int32_t>(outNodes.size());
		outNodes.push_back(node);
		outParents.push_back(parent);
		for (uint32_t i = 0; i < node->mNumChildren; ++i)
			CollectSubtree(node->mChildren[i], index, outNodes, outParents);
	}

	// Rotation absolutely, translation relative to its size: rigs in centimetres would hide a
	// rotation mismatch inside a tolerance scaled by their translations.
	static bool SameBindFrame(const glm::mat4& a, const glm::mat4& b)
	{
		for (int c = 0; c < 3; ++c)
		{
			for (int r = 0; r < 3; ++r)
			{
				if (std::abs(a[c][r] - b[c][r]) > 1e-3f)
					return false;
			}
		}

		const glm::vec3 ta(a[3]);
		const glm::vec3 tb(b[3]);
		return glm::length(ta - tb) <= 1e-3f * std::max({ 1.0f, glm::length(ta), glm::length(tb) });
	}

	template<typename T, typename Key, typename Convert>
	static AnimationTrack<T> ReadTrack(const Key* keys, uint32_t keyCount, double ticksPerSecond, Convert convert)
	{
		AnimationTrack<T> track;
		track.Times.reserve(keyCount);
		track.Values.reserve(keyCount);

		bool allStep = keyCount > 0;
		for (uint32_t i = 0; i < keyCount; ++i)
		{
			track.Times.push_back(static_cast<float>(keys[i].mTime / ticksPerSecond));
			track.Values.push_back(convert(keys[i].mValue));
			allStep = allStep && keys[i].mInterpolation == aiAnimInterpolation_Step;
		}
		track.Interpolation = allStep ? AnimationInterpolation::Step : AnimationInterpolation::Linear;
		return track;
	}

	struct SkinnedImport
	{
		std::unique_ptr<Skeleton>                   Skel;
		std::vector<std::unique_ptr<AnimationClip>> Clips;
		std::vector<SubMesh>                        SubMeshes;
	};

	struct SkinContext
	{
		const Skeleton*                            Skel = nullptr;
		std::unordered_map<const aiNode*, int32_t> JointOfNode;
		std::vector<glm::mat4>                     InverseBinds;
		std::vector<glm::mat4>                     RestPalette;
		std::vector<glm::mat4>                     MeshToSkin;
		std::vector<int32_t>                       MeshFirstJoint;
		bool                                       Singular = false;
	};

	struct Influences
	{
		glm::u16vec4 Joints{ 0 };
		glm::vec4    Weights{ 0.0f };
	};

	static void AddInfluence(Influences& influences, uint16_t joint, float weight)
	{
		if (weight <= influences.Weights[3])
			return;

		influences.Joints[3]  = joint;
		influences.Weights[3] = weight;
		for (int slot = 3; slot > 0 && influences.Weights[slot] > influences.Weights[slot - 1]; --slot)
		{
			std::swap(influences.Joints[slot], influences.Joints[slot - 1]);
			std::swap(influences.Weights[slot], influences.Weights[slot - 1]);
		}
	}

	// rigidJoint >= 0 binds every vertex to that joint with weight 1 (a mesh hanging off a
	// joint); otherwise the mesh's own bones weight it.
	static SubMesh ProcessSkinnedMesh(const aiMesh* mesh, uint32_t meshIndex, int32_t rigidJoint, int32_t fallbackJoint,
	                                  SkinContext& skin, const aiScene* scene, const std::string& modelName,
	                                  const std::filesystem::path& modelDir, TextureCache& textureCache)
	{
		const uint32_t vertexCount = mesh->mNumVertices;

		std::vector<Influences> influences(vertexCount);
		glm::mat4 toSkin(1.0f);
		if (rigidJoint >= 0)
		{
			toSkin = SafeInverse(skin.InverseBinds[rigidJoint], skin.Singular);
			for (Influences& influence : influences)
				influence = { glm::u16vec4(static_cast<uint16_t>(rigidJoint)), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f) };
		}
		else
		{
			toSkin = skin.MeshToSkin[meshIndex];
			uint32_t missingBones = 0;
			for (uint32_t b = 0; b < mesh->mNumBones; ++b)
			{
				const aiBone* bone = mesh->mBones[b];
				const int32_t joint = skin.Skel->FindJoint(bone->mName.C_Str());
				if (joint == Skeleton::k_InvalidJoint)
				{
					++missingBones;
					continue;
				}

				for (uint32_t w = 0; w < bone->mNumWeights; ++w)
				{
					const aiVertexWeight& weight = bone->mWeights[w];
					if (weight.mVertexId < vertexCount && weight.mWeight > 0.0f)
						AddInfluence(influences[weight.mVertexId], static_cast<uint16_t>(joint), weight.mWeight);
				}
			}

			uint32_t unweighted = 0;
			for (Influences& influence : influences)
			{
				const float total = influence.Weights.x + influence.Weights.y + influence.Weights.z + influence.Weights.w;
				if (total > 0.0f)
				{
					influence.Weights /= total;
				}
				else
				{
					influence = { glm::u16vec4(static_cast<uint16_t>(fallbackJoint), 0, 0, 0), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f) };
					++unweighted;
				}

				// An unused slot still multiplies its joint's matrix by 0 on the GPU, so it
				// repeats a joint this vertex really uses rather than whatever joint 0 is.
				for (int k = 1; k < 4; ++k)
				{
					if (influence.Weights[k] <= 0.0f)
						influence.Joints[k] = influence.Joints[0];
				}
			}

			if (missingBones > 0)
				DE_CORE_WARN("Model '{}': mesh '{}' has {} bone(s) with no matching node; their weights are ignored.", modelName, mesh->mName.C_Str(), missingBones);
			if (unweighted > 0)
				DE_CORE_WARN("Model '{}': mesh '{}' has {} vertices without joint weights; they follow joint '{}'.", modelName, mesh->mName.C_Str(), unweighted, skin.Skel->GetJoint(fallbackJoint).Name);
		}

		const glm::mat3 toSkinNormals = NormalMatrix(toSkin, skin.Singular);

		std::vector<SkinnedMeshVertex> skinVertices(vertexCount);
		std::vector<MeshVertex>        restVertices(vertexCount);
		for (uint32_t i = 0; i < vertexCount; ++i)
		{
			const glm::vec3 position(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
			const glm::vec3 normal = mesh->HasNormals() ? glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z) : glm::vec3(0.0f, 1.0f, 0.0f);
			const glm::vec2 texCoord = mesh->HasTextureCoords(0) ? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y) : glm::vec2(0.0f);

			SkinnedMeshVertex& skinned = skinVertices[i];
			skinned.Position = glm::vec3(toSkin * glm::vec4(position, 1.0f));
			skinned.Normal   = SafeNormalize(toSkinNormals * normal);
			skinned.TexCoord = texCoord;
			skinned.Joints   = influences[i].Joints;
			skinned.Weights  = influences[i].Weights;

			// The same blend the skinned vertex stage does, so the rest pose matches a skinned draw of it.
			glm::mat4 blended(0.0f);
			for (int k = 0; k < 4; ++k)
				blended += skinned.Weights[k] * skin.RestPalette[skinned.Joints[k]];

			const glm::vec3 restPosition = glm::vec3(blended * glm::vec4(skinned.Position, 1.0f));
			restVertices[i] = { restPosition, SafeNormalize(CofactorNormal(glm::mat3(blended), skinned.Normal)), texCoord };
		}

		SubMesh submesh;
		submesh.MeshData = Mesh::CreateSkinned(std::move(restVertices), std::move(skinVertices), ReadIndices(mesh));
		AssignMaterial(submesh, mesh, scene, modelDir, textureCache);
		return submesh;
	}

	static SubMesh ProcessPlacedMesh(const aiMesh* mesh, const glm::mat4& global, bool& singular, const aiScene* scene,
	                                 const std::filesystem::path& modelDir, TextureCache& textureCache)
	{
		const glm::mat3 normalMatrix = NormalMatrix(global, singular);

		std::vector<MeshVertex> vertices(mesh->mNumVertices);
		for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
		{
			const glm::vec3 position(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
			const glm::vec3 normal = mesh->HasNormals() ? glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z) : glm::vec3(0.0f, 1.0f, 0.0f);

			vertices[i].Position = glm::vec3(global * glm::vec4(position, 1.0f));
			vertices[i].Normal   = SafeNormalize(normalMatrix * normal);
			vertices[i].TexCoord = mesh->HasTextureCoords(0) ? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y) : glm::vec2(0.0f);
		}

		SubMesh submesh;
		submesh.MeshData = Mesh::Create(vertices, ReadIndices(mesh));
		AssignMaterial(submesh, mesh, scene, modelDir, textureCache);
		return submesh;
	}

	// Without a skeleton (skin.Skel null) every mesh is placed by its node, as
	// PreTransformVertices would. A skinned mesh is drawn once however many nodes use it:
	// skinning ignores the node transform, so the copies would coincide.
	static void TraverseSkinnedNode(const aiNode* node, const glm::mat4& parentGlobal, SkinContext& skin,
	                                std::unordered_set<uint32_t>& skinnedMeshesDone,
	                                const aiScene* scene, const std::string& modelName,
	                                const std::filesystem::path& modelDir, TextureCache& textureCache,
	                                std::vector<SubMesh>& outSubMeshes)
	{
		const glm::mat4 global = parentGlobal * ToGlm(node->mTransformation);

		auto jointIt = skin.JointOfNode.find(node);
		const int32_t nodeJoint = jointIt != skin.JointOfNode.end() ? jointIt->second : Skeleton::k_InvalidJoint;

		for (uint32_t i = 0; i < node->mNumMeshes; ++i)
		{
			const uint32_t meshIndex = node->mMeshes[i];
			const aiMesh* mesh = scene->mMeshes[meshIndex];

			if (skin.Skel && mesh->HasBones())
			{
				if (!skinnedMeshesDone.insert(meshIndex).second)
					continue;

				int32_t fallbackJoint = skin.MeshFirstJoint[meshIndex];
				if (fallbackJoint == Skeleton::k_InvalidJoint)
					fallbackJoint = nodeJoint != Skeleton::k_InvalidJoint ? nodeJoint : 0;
				outSubMeshes.push_back(ProcessSkinnedMesh(mesh, meshIndex, Skeleton::k_InvalidJoint, fallbackJoint, skin, scene, modelName, modelDir, textureCache));
			}
			else if (skin.Skel && nodeJoint != Skeleton::k_InvalidJoint)
				outSubMeshes.push_back(ProcessSkinnedMesh(mesh, meshIndex, nodeJoint, nodeJoint, skin, scene, modelName, modelDir, textureCache));
			else
				outSubMeshes.push_back(ProcessPlacedMesh(mesh, global, skin.Singular, scene, modelDir, textureCache));
		}

		for (uint32_t i = 0; i < node->mNumChildren; ++i)
			TraverseSkinnedNode(node->mChildren[i], global, skin, skinnedMeshesDone, scene, modelName, modelDir, textureCache, outSubMeshes);
	}

	static std::vector<std::unique_ptr<AnimationClip>> ReadAnimations(const aiScene* scene, const Skeleton& skeleton, const std::string& modelName)
	{
		std::vector<std::unique_ptr<AnimationClip>> clips;
		clips.reserve(scene->mNumAnimations);

		for (uint32_t a = 0; a < scene->mNumAnimations; ++a)
		{
			const aiAnimation* animation = scene->mAnimations[a];
			const double ticksPerSecond = animation->mTicksPerSecond > 0.0 ? animation->mTicksPerSecond : 25.0;

			std::vector<AnimationChannel> channels;
			channels.reserve(animation->mNumChannels);
			std::vector<bool> animated(skeleton.GetJointCount(), false);
			float duration = static_cast<float>(animation->mDuration / ticksPerSecond);
			uint32_t dropped = 0;

			for (uint32_t c = 0; c < animation->mNumChannels; ++c)
			{
				const aiNodeAnim* nodeAnim = animation->mChannels[c];
				const int32_t joint = skeleton.FindJoint(nodeAnim->mNodeName.C_Str());
				if (joint == Skeleton::k_InvalidJoint || animated[joint])
				{
					++dropped;
					continue;
				}
				animated[joint] = true;

				AnimationChannel& channel = channels.emplace_back();
				channel.JointName   = nodeAnim->mNodeName.C_Str();
				channel.Translation = ReadTrack<glm::vec3>(nodeAnim->mPositionKeys, nodeAnim->mNumPositionKeys, ticksPerSecond,
					[](const aiVector3D& v) { return glm::vec3(v.x, v.y, v.z); });
				channel.Rotation    = ReadTrack<glm::quat>(nodeAnim->mRotationKeys, nodeAnim->mNumRotationKeys, ticksPerSecond,
					[](const aiQuaternion& q) { return glm::quat(q.w, q.x, q.y, q.z); });
				channel.Scale       = ReadTrack<glm::vec3>(nodeAnim->mScalingKeys, nodeAnim->mNumScalingKeys, ticksPerSecond,
					[](const aiVector3D& v) { return glm::vec3(v.x, v.y, v.z); });

				for (const auto* times : { &channel.Translation.Times, &channel.Rotation.Times, &channel.Scale.Times })
				{
					if (!times->empty())
						duration = std::max(duration, times->back());
				}
			}

			if (dropped > 0)
				DE_CORE_WARN("Model '{}': clip '{}' has {} channel(s) for nodes outside its skeleton or animated twice; ignored.", modelName, animation->mName.C_Str(), dropped);

			clips.push_back(std::make_unique<AnimationClip>(animation->mName.C_Str(), duration, std::move(channels), &skeleton));
		}

		return clips;
	}

	// Inverse binds come from each bone's offset matrix, which is inverse(bone bind) x (that
	// mesh's own bind transform): meshes skinned to one bone can disagree by a per-mesh factor
	// (FBX bakes each mesh's node transform in). The mesh with the most bones fixes the frame,
	// then meshes join through the bones they share, each moving its vertices into that frame
	// through MeshToSkin; a mesh that bridged two frames defined separately would land wrong.
	static void ComputeInverseBinds(const aiScene* scene, const std::unordered_map<std::string, int32_t>& jointByName,
	                                std::vector<bool>& hasInverseBind, SkinContext& skin, const std::string& modelName)
	{
		std::vector<std::vector<std::pair<int32_t, glm::mat4>>> meshBones(scene->mNumMeshes);
		for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
		{
			const aiMesh* mesh = scene->mMeshes[m];
			for (uint32_t b = 0; b < mesh->mNumBones; ++b)
			{
				auto it = jointByName.find(mesh->mBones[b]->mName.C_Str());
				if (it != jointByName.end())
					meshBones[m].emplace_back(it->second, ToGlm(mesh->mBones[b]->mOffsetMatrix));
			}
			skin.MeshFirstJoint[m] = meshBones[m].empty() ? Skeleton::k_InvalidJoint : meshBones[m].front().first;
		}

		std::vector<bool> done(scene->mNumMeshes, false);
		while (true)
		{
			int32_t best = -1;
			size_t bestShared = 0;
			for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
			{
				if (done[m] || meshBones[m].empty())
					continue;

				size_t shared = 0;
				for (const auto& [joint, offset] : meshBones[m])
					shared += hasInverseBind[joint] ? 1 : 0;

				if (best < 0 || shared > bestShared || (shared == bestShared && meshBones[m].size() > meshBones[best].size()))
				{
					best = static_cast<int32_t>(m);
					bestShared = shared;
				}
			}
			if (best < 0)
				break;

			done[best] = true;
			glm::mat4 meshToSkin(1.0f);
			for (const auto& [joint, offset] : meshBones[best])
			{
				if (!hasInverseBind[joint])
					continue;
				if (offset != skin.InverseBinds[joint])
					meshToSkin = SafeInverse(skin.InverseBinds[joint], skin.Singular) * offset;
				break;
			}

			const glm::mat4 skinToMesh = SafeInverse(meshToSkin, skin.Singular);
			bool consistent = true;
			for (const auto& [joint, offset] : meshBones[best])
			{
				if (!hasInverseBind[joint])
				{
					skin.InverseBinds[joint] = offset * skinToMesh;
					hasInverseBind[joint] = true;
				}
				else if (!SameBindFrame(skin.InverseBinds[joint] * meshToSkin, offset))
					consistent = false;
			}

			if (!consistent)
				DE_CORE_WARN("Model '{}': mesh '{}' binds its bones in a different pose than another mesh; it may not skin correctly.", modelName, scene->mMeshes[best]->mName.C_Str());

			skin.MeshToSkin[best] = meshToSkin;
		}
	}

	static SkinnedImport ImportSkinned(const aiScene* scene, const std::string& modelName, const std::filesystem::path& modelDir, TextureCache& textureCache,
	                                   bool loadMeshes)
	{
		SkinnedImport out;
		SkinContext skin;
		std::unordered_set<uint32_t> skinnedMeshesDone;

		// The skeleton is the subtree under the deepest node that holds every bone and every
		// animated node, so end joints and helper nodes a socket may name come along.
		const aiNode* armatureRoot = nullptr;
		auto include = [&](const aiString& name)
		{
			const aiNode* node = scene->mRootNode->FindNode(name);
			if (node)
				armatureRoot = armatureRoot ? CommonAncestor(armatureRoot, node) : node;
		};
		for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
		{
			for (uint32_t b = 0; b < scene->mMeshes[m]->mNumBones; ++b)
				include(scene->mMeshes[m]->mBones[b]->mName);
		}
		for (uint32_t a = 0; a < scene->mNumAnimations; ++a)
		{
			for (uint32_t c = 0; c < scene->mAnimations[a]->mNumChannels; ++c)
				include(scene->mAnimations[a]->mChannels[c]->mNodeName);
		}

		std::vector<const aiNode*> subtree;
		std::vector<int32_t>       subtreeParents;
		if (armatureRoot)
			CollectSubtree(armatureRoot, Skeleton::k_InvalidJoint, subtree, subtreeParents);

		if (!armatureRoot || subtree.size() > std::numeric_limits<uint16_t>::max())
		{
			if (armatureRoot)
				DE_CORE_WARN("Model '{}': its skeleton would have {} joints, more than a skin vertex can index; it loads without one.", modelName, subtree.size());
			else
				DE_CORE_WARN("Model '{}': none of its bones or animated nodes is in the node graph; it loads without a skeleton.", modelName);

			if (loadMeshes)
				TraverseSkinnedNode(scene->mRootNode, glm::mat4(1.0f), skin, skinnedMeshesDone, scene, modelName, modelDir, textureCache, out.SubMeshes);
			return out;
		}

		// Skin vertices index joints directly, so the joints they can reach (bones, nodes
		// carrying an unskinned mesh, and their ancestors) go first. A palette then needs only
		// those, however many helper or animated nodes the subtree also holds. Both groups stay
		// parents first: a reachable joint's parent is reachable.
		std::unordered_map<std::string, int32_t> subtreeByName;
		for (int32_t i = 0; i < static_cast<int32_t>(subtree.size()); ++i)
			subtreeByName.try_emplace(subtree[i]->mName.C_Str(), i);

		std::vector<bool> reachable(subtree.size(), false);
		for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
		{
			for (uint32_t b = 0; b < scene->mMeshes[m]->mNumBones; ++b)
			{
				auto it = subtreeByName.find(scene->mMeshes[m]->mBones[b]->mName.C_Str());
				if (it != subtreeByName.end())
					reachable[it->second] = true;
			}
		}
		for (size_t i = 0; i < subtree.size(); ++i)
		{
			for (uint32_t k = 0; k < subtree[i]->mNumMeshes; ++k)
			{
				if (!scene->mMeshes[subtree[i]->mMeshes[k]]->HasBones())
					reachable[i] = true;
			}
		}
		for (size_t i = subtree.size(); i-- > 0;)
		{
			if (reachable[i] && subtreeParents[i] >= 0)
				reachable[subtreeParents[i]] = true;
		}

		std::vector<int32_t> order;
		order.reserve(subtree.size());
		for (bool group : { true, false })
		{
			for (size_t i = 0; i < subtree.size(); ++i)
			{
				if (reachable[i] == group)
					order.push_back(static_cast<int32_t>(i));
			}
		}
		std::vector<int32_t> jointOfSubtree(subtree.size());
		for (size_t j = 0; j < order.size(); ++j)
			jointOfSubtree[order[j]] = static_cast<int32_t>(j);

		const uint32_t jointCount     = static_cast<uint32_t>(order.size());
		const uint32_t skinJointCount = static_cast<uint32_t>(std::count(reachable.begin(), reachable.end(), true));
		const glm::mat4 rootTransform = armatureRoot->mParent ? GlobalTransform(armatureRoot->mParent) : glm::mat4(1.0f);

		std::vector<Joint>     joints(jointCount);
		std::vector<glm::mat4> restGlobals(jointCount);
		std::unordered_map<std::string, int32_t> jointByName;
		for (uint32_t j = 0; j < jointCount; ++j)
		{
			const aiNode* node = subtree[order[j]];
			const int32_t subtreeParent = subtreeParents[order[j]];

			joints[j].Name     = node->mName.C_Str();
			joints[j].Parent   = subtreeParent < 0 ? Skeleton::k_InvalidJoint : jointOfSubtree[subtreeParent];
			joints[j].RestPose = ToJointPose(node->mTransformation);

			const glm::mat4 local = joints[j].RestPose.ToMatrix();
			restGlobals[j] = joints[j].Parent < 0 ? local : restGlobals[joints[j].Parent] * local;

			jointByName.try_emplace(joints[j].Name, static_cast<int32_t>(j));
			skin.JointOfNode.emplace(node, static_cast<int32_t>(j));
		}

		skin.MeshToSkin.assign(scene->mNumMeshes, glm::mat4(1.0f));
		skin.MeshFirstJoint.assign(scene->mNumMeshes, Skeleton::k_InvalidJoint);
		skin.InverseBinds.assign(jointCount, glm::mat4(1.0f));
		std::vector<bool> hasInverseBind(jointCount, false);
		ComputeInverseBinds(scene, jointByName, hasInverseBind, skin, modelName);

		skin.RestPalette.resize(jointCount);
		for (uint32_t j = 0; j < jointCount; ++j)
		{
			if (!hasInverseBind[j])
				skin.InverseBinds[j] = SafeInverse(rootTransform * restGlobals[j], skin.Singular);
			joints[j].InverseBind = skin.InverseBinds[j];

			skin.RestPalette[j] = rootTransform * restGlobals[j] * skin.InverseBinds[j];
		}

		out.Skel = std::make_unique<Skeleton>(std::move(joints), rootTransform, skinJointCount);
		skin.Skel = out.Skel.get();

		out.Clips = ReadAnimations(scene, *out.Skel, modelName);
		if (loadMeshes)
			TraverseSkinnedNode(scene->mRootNode, glm::mat4(1.0f), skin, skinnedMeshesDone, scene, modelName, modelDir, textureCache, out.SubMeshes);

		if (skin.Singular)
			DE_CORE_WARN("Model '{}': a joint or mesh has a transform that can't be inverted (zero scale?); identity is used in its place.", modelName);

		return out;
	}

	Model* Model::LoadFromFile(const std::filesystem::path& filepath)
	{
		return Load(filepath, ModelLoadParams(), nullptr);
	}

	Model* Model::LoadFromFile(const std::filesystem::path& filepath, const ModelLoadParams& params)
	{
		return Load(filepath, params, nullptr);
	}

	Model* Model::Load(const std::filesystem::path& filepath, const ModelLoadParams& params, Model* refresh)
	{
		const std::filesystem::path resolvedPath = Internal::ResolveRawAssetPath(filepath);

		// Read once without post-processing to see whether the file is skinned, then apply
		// the steps for that path. ApplyPostProcessing(flags) after ReadFile(path, 0) is
		// what ReadFile(path, flags) does, so static models load exactly as before.
		Assimp::Importer importer;
		// Otherwise Collada and BVH give an animation-only file a dummy mesh with a bone per
		// node, and it never reads as a clip library. A file with geometry is unaffected.
		importer.SetPropertyBool(AI_CONFIG_IMPORT_NO_SKELETON_MESHES, true);
		const aiScene* scene = importer.ReadFile(resolvedPath.string(), 0);

		const bool skinned = scene && (HasBones(scene) || IsClipLibrary(scene));
		if (skinned && scene->mRootNode && HasFbxPivotNodes(scene->mRootNode))
		{
			// Pivot helper nodes would sit between the joints and take over their names and
			// animation channels; read again with them folded into the joints.
			importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
			scene = importer.ReadFile(resolvedPath.string(), 0);
		}

		// LimitBoneWeights would drop bones that weight nothing, such as an unweighted root
		// joint, and move the skeleton's root with them.
		if (skinned)
			importer.SetPropertyBool(AI_CONFIG_IMPORT_REMOVE_EMPTY_BONES, false);

		if (scene)
			scene = importer.ApplyPostProcessing(skinned ? k_SkinnedImportFlags : k_StaticImportFlags);

		// A clip library has no meshes, which assimp flags as incomplete.
		const bool incomplete = scene && (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) && !(skinned && IsClipLibrary(scene));
		if (!scene || incomplete || !scene->mRootNode)
		{
			DE_CORE_ERROR("Model::LoadFromFile failed for '{}': {}", resolvedPath.string(), importer.GetErrorString());
			return nullptr;
		}

		const std::string modelName = resolvedPath.filename().string();
		if (params.ClipsOnly && !skinned)
		{
			DE_CORE_ERROR("Model::LoadFromFile failed for '{}': ClipsOnly, but the file has no bones, so it has no clips to load", resolvedPath.string());
			return nullptr;
		}
		if (!skinned && scene->mNumAnimations > 0)
			DE_CORE_WARN("Model '{}': {} clip(s) ignored; a model without bones loads as a static mesh.", modelName, scene->mNumAnimations);

		Model* model = new Model();
		model->m_FilePath = std::filesystem::absolute(resolvedPath);
		model->m_LoadParams = params;
		// Absolute, so the material textures found beside the model are not resolved a second
		// time against the asset root by Texture::CreateFromFile.
		std::filesystem::path modelDir = model->m_FilePath.parent_path();

		TextureCache textureCache;
		textureCache.ModelPath = model->m_FilePath;
		textureCache.ModelWriteTime = WriteTimeOf(model->m_FilePath);
		if (refresh)
		{
			for (const ModelTexture& texture : refresh->m_Textures)
				textureCache.Reusable.try_emplace(texture.Path, CachedTexture{ texture.Image, texture.WriteTime });
		}
		if (skinned)
		{
			SkinnedImport skinnedImport = ImportSkinned(scene, modelName, modelDir, textureCache, !params.ClipsOnly);
			model->m_Skeleton   = std::move(skinnedImport.Skel);
			model->m_Animations = std::move(skinnedImport.Clips);
			model->m_SubMeshes  = std::move(skinnedImport.SubMeshes);
		}
		else
		{
			TraverseNode(scene->mRootNode, scene, modelDir, textureCache, model->m_SubMeshes);
		}

		model->m_Textures.reserve(textureCache.Loaded.size());
		for (const auto& [path, texture] : textureCache.Loaded)
			model->m_Textures.push_back({ path, texture.Image, texture.WriteTime, texture.Borrowed });

		if (!model->m_Animations.empty())
		{
			std::filesystem::path events = resolvedPath;
			events.replace_extension(".events");
			std::error_code error;
			if (std::filesystem::exists(events, error))
				model->LoadEvents(events);
		}

		return model;
	}

	Model::~Model()
	{
		Destroy();
	}

	void Model::Destroy()
	{
		for (auto& sm : m_SubMeshes)
		{
			delete sm.MeshData;
			DestroyAndDelete(sm.Mat);
		}
		m_SubMeshes.clear();

		for (ModelTexture& texture : m_Textures)
		{
			if (!texture.Borrowed)
				DestroyAndDelete(texture.Image);
		}
		m_Textures.clear();

		m_Animations.clear();
		m_Skeleton.reset();
		m_RetiredSkeletons.clear();
	}

	bool Model::Reload()
	{
		if (m_FilePath.empty())
		{
			DE_CORE_WARN("Model::Reload: the model wasn't loaded from a file");
			return false;
		}

		std::unique_ptr<Model> fresh(Load(m_FilePath, m_LoadParams, this));
		if (!fresh)
			return false;

		Adopt(*fresh);
		++m_Generation;
		return true;
	}

	bool Model::ReloadEvents()
	{
		if (m_FilePath.empty())
		{
			DE_CORE_WARN("Model::ReloadEvents: the model wasn't loaded from a file");
			return false;
		}

		std::filesystem::path events = m_FilePath;
		events.replace_extension(".events");
		std::error_code error;
		const bool exists = !m_Animations.empty() && std::filesystem::exists(events, error);
		if (exists && !std::ifstream(events).is_open())
		{
			DE_CORE_WARN("Model '{}': couldn't read '{}', so the clips keep their events", m_FilePath.filename().string(), events.filename().string());
			return false;
		}

		for (const std::unique_ptr<AnimationClip>& clip : m_Animations)
			clip->ClearEvents();
		if (exists)
			LoadEvents(events);
		++m_Generation;
		return true;
	}

	namespace
	{

		bool SameJoints(const Skeleton& a, const Skeleton& b)
		{
			if (a.GetJointCount() != b.GetJointCount() || a.GetSkinJointCount() != b.GetSkinJointCount())
				return false;

			for (uint32_t i = 0; i < a.GetJointCount(); ++i)
			{
				if (a.GetJoint(i).Name != b.GetJoint(i).Name || a.GetJoint(i).Parent != b.GetJoint(i).Parent)
					return false;
			}
			return true;
		}

	}

	void Model::Adopt(Model& fresh)
	{
		if (m_Skeleton && fresh.m_Skeleton && SameJoints(*m_Skeleton, *fresh.m_Skeleton))
		{
			m_Skeleton->Reinitialize(*fresh.m_Skeleton);
		}
		else
		{
			if (m_Skeleton)
			{
				DE_CORE_WARN("Model '{}': the reload changed the skeleton's joints, so animators on it start over", m_FilePath.filename().string());
				m_RetiredSkeletons.push_back(std::move(m_Skeleton));
			}
			m_Skeleton = std::move(fresh.m_Skeleton);
		}
		const Skeleton* skeleton = m_Skeleton.get();

		// The n-th clip of a name takes the n-th of that name in the file: Mixamo calls every clip
		// "mixamo.com".
		const size_t clipCount = m_Animations.size();
		std::vector<bool> matched(clipCount, false);
		for (std::unique_ptr<AnimationClip>& clip : fresh.m_Animations)
		{
			size_t match = 0;
			while (match < clipCount && (matched[match] || m_Animations[match]->GetName() != clip->GetName()))
				++match;

			if (match < clipCount)
			{
				matched[match] = true;
				m_Animations[match]->Reinitialize(*clip, skeleton);
			}
			else
			{
				clip->m_SourceSkeleton = skeleton;
				m_Animations.push_back(std::move(clip));
			}
		}
		for (size_t i = 0; i < clipCount; ++i)
		{
			if (!matched[i])
			{
				m_Animations[i]->Clear();
				m_Animations[i]->m_SourceSkeleton = skeleton;
			}
		}
		fresh.m_Animations.clear();

		const size_t meshCount = m_SubMeshes.size();
		for (size_t i = 0; i < fresh.m_SubMeshes.size(); ++i)
		{
			SubMesh& incoming = fresh.m_SubMeshes[i];
			if (i >= meshCount)
			{
				m_SubMeshes.push_back(incoming);
				continue;
			}

			SubMesh& current = m_SubMeshes[i];
			current.MeshData->Reinitialize(*incoming.MeshData);
			current.Mat->SetTexture(0, incoming.DiffuseTexture);
			current.DiffuseTexture = incoming.DiffuseTexture;
			delete incoming.MeshData;
			DestroyAndDelete(incoming.Mat);
		}
		for (size_t i = fresh.m_SubMeshes.size(); i < meshCount; ++i)
		{
			m_SubMeshes[i].MeshData->Clear();
			m_SubMeshes[i].Mat->SetTexture(0, nullptr);
			m_SubMeshes[i].DiffuseTexture = nullptr;
		}
		fresh.m_SubMeshes.clear();

		// One the file stopped using is kept: a game's own material may still draw with it.
		for (ModelTexture& texture : fresh.m_Textures)
			texture.Borrowed = false;
		for (ModelTexture& texture : m_Textures)
		{
			const bool reused = std::any_of(fresh.m_Textures.begin(), fresh.m_Textures.end(), [&](const ModelTexture& other) { return other.Image == texture.Image; });
			if (!reused)
				fresh.m_Textures.push_back(std::move(texture));
		}
		m_Textures = std::move(fresh.m_Textures);
		fresh.m_Textures.clear();
	}

	AnimationClip* Model::GetAnimation(uint32_t index) const
	{
		return index < m_Animations.size() ? m_Animations[index].get() : nullptr;
	}

	AnimationClip* Model::FindAnimation(std::string_view name) const
	{
		for (const auto& clip : m_Animations)
		{
			if (clip->GetName() == name)
				return clip.get();
		}
		return nullptr;
	}

	namespace
	{

		// Splits on whitespace up to a `#`; a double-quoted token may hold spaces. False on an
		// unclosed quote.
		bool TokenizeEventLine(std::string_view line, std::vector<std::string>& tokens)
		{
			tokens.clear();
			size_t i = 0;
			while (i < line.size())
			{
				if (std::isspace(static_cast<unsigned char>(line[i])))
				{
					i++;
					continue;
				}
				if (line[i] == '#')
					break;

				if (line[i] == '"')
				{
					const size_t close = line.find('"', i + 1);
					if (close == std::string_view::npos)
						return false;
					tokens.emplace_back(line.substr(i + 1, close - i - 1));
					i = close + 1;
					continue;
				}

				const size_t start = i;
				while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i])) && line[i] != '#')
					i++;
				tokens.emplace_back(line.substr(start, i - start));
			}
			return true;
		}

		bool ParseSeconds(std::string_view text, float& value)
		{
			const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
			return error == std::errc() && end == text.data() + text.size() && std::isfinite(value);
		}

	}

	bool Model::LoadEvents(const std::filesystem::path& filepath)
	{
		std::ifstream file(filepath);
		if (!file)
		{
			DE_CORE_ERROR("Model::LoadEvents: can't read '{}'", filepath.string());
			return false;
		}

		const std::string fileName = filepath.filename().string();
		std::string line;
		std::vector<std::string> tokens;
		for (uint32_t number = 1; std::getline(file, line); ++number)
		{
			if (number == 1 && line.starts_with("\xEF\xBB\xBF"))
				line.erase(0, 3);

			if (!TokenizeEventLine(line, tokens))
			{
				DE_CORE_WARN("{} line {}: an unclosed quote; line skipped", fileName, number);
				continue;
			}
			if (tokens.empty())
				continue;
			if (tokens.size() != 3)
			{
				DE_CORE_WARN("{} line {}: expected `<clip> <time> <event>` or `<clip> <begin>..<end> <event>`; line skipped", fileName, number);
				continue;
			}

			AnimationClip* clip = FindAnimation(tokens[0]);
			if (!clip)
			{
				DE_CORE_WARN("{} line {}: the model has no clip '{}'; line skipped", fileName, number, tokens[0]);
				continue;
			}

			const std::string_view time = tokens[1];
			const size_t dots = time.find("..");
			float begin = 0.0f;
			float end = 0.0f;
			const bool parsed = dots == std::string_view::npos
				? ParseSeconds(time, begin)
				: ParseSeconds(time.substr(0, dots), begin) && ParseSeconds(time.substr(dots + 2), end);
			if (!parsed)
			{
				DE_CORE_WARN("{} line {}: '{}' isn't a time in seconds or a <begin>..<end> range; line skipped", fileName, number, tokens[1]);
				continue;
			}

			if (dots != std::string_view::npos && end < begin)
			{
				DE_CORE_WARN("{} line {}: the range '{}' ends before it begins (a range can't wrap past the clip's end); line skipped", fileName, number, tokens[1]);
				continue;
			}

			if (begin < 0.0f || begin > clip->GetDuration() || (dots != std::string_view::npos && end > clip->GetDuration()))
				DE_CORE_WARN("{} line {}: '{}' reaches past clip '{}' ({:.3f} s), so part of it never fires", fileName, number, tokens[1], tokens[0], clip->GetDuration());

			if (dots == std::string_view::npos)
				clip->AddEvent(begin, std::move(tokens[2]));
			else
				clip->AddEventRange(begin, end, std::move(tokens[2]));
		}
		return true;
	}

}
