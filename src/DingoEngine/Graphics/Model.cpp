#include "depch.h"
#include "DingoEngine/Graphics/Model.h"
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

	static Texture* LoadDiffuseTexture(aiMaterial* aiMat, const std::filesystem::path& modelDir)
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

		auto tryLoad = [&](const std::filesystem::path& p) -> Texture*
		{
			if (std::filesystem::exists(p))
				return Texture::CreateFromFile(p);
			return nullptr;
		};

		Texture* tex = nullptr;
		if (texPath.is_absolute())
			tex = tryLoad(texPath);
		if (!tex)
			tex = tryLoad(modelDir / texPath);
		if (!tex)
			tex = tryLoad(modelDir / texPath.filename());

		return tex;
	}

	static SubMesh ProcessMesh(aiMesh* mesh, const aiScene* scene,
	                           const std::filesystem::path& modelDir)
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

		Texture* diffuse = LoadDiffuseTexture(aiMat, modelDir);
		if (diffuse)
		{
			submesh.DiffuseTexture = diffuse;
			submesh.Mat->SetTexture(0, diffuse);
		}

		return submesh;
	}

	static void TraverseNode(aiNode* node, const aiScene* scene,
	                         const std::filesystem::path& modelDir,
	                         std::vector<SubMesh>& outSubMeshes)
	{
		for (uint32_t i = 0; i < node->mNumMeshes; ++i)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			outSubMeshes.push_back(ProcessMesh(mesh, scene, modelDir));
		}
		for (uint32_t i = 0; i < node->mNumChildren; ++i)
			TraverseNode(node->mChildren[i], scene, modelDir, outSubMeshes);
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
		TraverseNode(scene->mRootNode, scene, modelDir, model->m_SubMeshes);
		return model;
	}

	void Model::Destroy()
	{
		for (auto& sm : m_SubMeshes)
		{
			delete sm.MeshData;
			if (sm.DiffuseTexture)
			{
				sm.DiffuseTexture->Destroy();
				delete sm.DiffuseTexture;
			}
			if (sm.Mat)
			{
				sm.Mat->Destroy();
				delete sm.Mat;
			}
		}
		m_SubMeshes.clear();
	}

}
