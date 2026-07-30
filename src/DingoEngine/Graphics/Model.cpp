#include "depch.h"
#include "DingoEngine/Graphics/Model.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace Dingo
{

	static constexpr uint32_t k_ImportFlags =
		aiProcess_Triangulate           |
		aiProcess_GenSmoothNormals      |
		aiProcess_FlipUVs               |
		aiProcess_CalcTangentSpace      |
		aiProcess_JoinIdenticalVertices |
		aiProcess_PreTransformVertices;

	// Textures loaded so far for the model being loaded, keyed on the resolved path. Without
	// it a shared atlas is decoded AND uploaded once per submesh, each with its own command
	// list and queue submit, plus three filesystem probes. Load-scoped: the Model owns the
	// results, and managed models get the AssetManager's own dedup on top.
	using TextureCache = std::unordered_map<std::string, Texture*>;

	static Texture* LoadDiffuseTexture(aiMaterial* aiMat, const std::filesystem::path& modelDir, TextureCache& textureCache)
	{
		if (aiMat->GetTextureCount(aiTextureType_DIFFUSE) == 0)
			return nullptr;

		aiString aiPath;
		aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &aiPath);
		std::string rawPath = aiPath.C_Str();

		// Embedded textures (path starts with '*') are not yet supported
		if (!rawPath.empty() && rawPath[0] == '*')
			return nullptr;

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

			auto it = textureCache.find(key);
			if (it != textureCache.end())
				return it->second;

			if (!std::filesystem::exists(candidates[i]))
				continue;

			Texture* texture = Texture::CreateFromFile(candidates[i]);
			if (!texture)
				continue;

			textureCache[key] = texture;
			return texture;
		}

		return nullptr;
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

		std::vector<uint32_t> indices;
		indices.reserve(static_cast<size_t>(mesh->mNumFaces) * 3);
		for (uint32_t f = 0; f < mesh->mNumFaces; ++f)
		{
			const aiFace& face = mesh->mFaces[f];
			for (uint32_t j = 0; j < face.mNumIndices; ++j)
				indices.push_back(face.mIndices[j]);
		}

		SubMesh submesh;
		submesh.MeshData = Mesh::Create(vertices, indices);

		aiMaterial* aiMat = scene->mMaterials[mesh->mMaterialIndex];
		submesh.Mat = Material::Create(MaterialParams()
			.SetDebugName(mesh->mName.C_Str()));

		Texture* diffuse = LoadDiffuseTexture(aiMat, modelDir, textureCache);
		if (diffuse)
		{
			submesh.DiffuseTexture = diffuse;
			submesh.Mat->SetTexture(0, diffuse);
		}

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

	Model* Model::LoadFromFile(const std::filesystem::path& filepath)
	{
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(filepath.string(), k_ImportFlags);

		if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
		{
			DE_CORE_ERROR("Model::LoadFromFile failed for '{}': {}", filepath.string(), importer.GetErrorString());
			return nullptr;
		}

		Model* model = new Model();
		std::filesystem::path modelDir = filepath.parent_path();

		TextureCache textureCache;
		TraverseNode(scene->mRootNode, scene, modelDir, textureCache, model->m_SubMeshes);

		model->m_Textures.reserve(textureCache.size());
		for (const auto& [path, texture] : textureCache)
			model->m_Textures.push_back(texture);

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

		for (Texture*& texture : m_Textures)
			DestroyAndDelete(texture);
		m_Textures.clear();
	}

}
