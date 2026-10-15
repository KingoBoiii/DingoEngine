#include "Look/FarmerAssets.h"
#include "GameTuning.h"

#include <algorithm>

namespace Dingo
{

	namespace
	{
		constexpr const char* k_ModelPath = "kaykit/characters/Barbarian.glb";
		constexpr const char* k_TexturePath = "kaykit/characters/barbarian_texture.png";
		constexpr const char* k_KnightPath = "kaykit/characters/Knight.glb";
		constexpr const char* k_KnightTexturePath = "kaykit/characters/knight_texture.png";
		constexpr const char* k_GeneralPath = "kaykit/animations/Rig_Medium_General.glb";
		constexpr const char* k_MovementPath = "kaykit/animations/Rig_Medium_MovementBasic.glb";
		constexpr const char* k_IdleName = "Idle_A";
		constexpr const char* k_WalkName = "Walking_A";
	}

	std::vector<const char*> FarmerAssetPaths()
	{
		return { k_ModelPath, k_TexturePath, k_KnightPath, k_KnightTexturePath, k_GeneralPath, k_MovementPath };
	}

	FarmerAssets::FarmerAssets()
	{
		AssetManager& assets = Application::Get().GetAssetManager();

		m_Model = assets.GetModel(assets.Load(k_ModelPath));
		if (!m_Model)
		{
			DE_ERROR("Hearthvale: failed to load '{}'", k_ModelPath);
			return;
		}

		const ModelLoadParams clipsOnly = ModelLoadParams().SetClipsOnly(true);
		Model* general = assets.GetModel(assets.Load(k_GeneralPath, clipsOnly));
		Model* movement = assets.GetModel(assets.Load(k_MovementPath, clipsOnly));
		m_Idle = general ? general->FindAnimation(k_IdleName) : nullptr;
		m_Walk = movement ? movement->FindAnimation(k_WalkName) : nullptr;
		if (!m_Idle || !m_Walk)
		{
			DE_ERROR("Hearthvale: the farmer's '{}' or '{}' clip did not load", k_IdleName, k_WalkName);
		}

		Texture* texture = assets.GetTexture(assets.Load(k_TexturePath));
		if (!texture)
		{
			DE_ERROR("Hearthvale: failed to load '{}'", k_TexturePath);
		}
		m_Material = Application::Get().GetRenderer3D().CreateLitMaterial(MaterialParams()
			.SetDebugName("Farmer")
			.SetRoughness(PLAYER_MODEL_ROUGHNESS));
		if (texture)
			m_Material->SetTexture(0, texture);

		const auto heightOf = [](const Model* model) {
			float height = 0.0f;
			for (const SubMesh& submesh : model->GetSubMeshes())
			{
				if (!submesh.Mesh)
					continue;
				for (const MeshVertex& vertex : submesh.Mesh->GetVertices())
					height = (std::max)(height, vertex.Position.y);
			}
			return height;
		};
		m_ModelHeight = heightOf(m_Model);
		m_Characters.push_back({ k_ModelPath, m_Model, nullptr, m_ModelHeight });

		CharacterAsset knight{ k_KnightPath };
		knight.Loaded = assets.GetModel(assets.Load(k_KnightPath));
		if (knight.Loaded)
		{
			knight.Height = heightOf(knight.Loaded);
			Texture* knightTexture = assets.GetTexture(assets.Load(k_KnightTexturePath));
			knight.Lit = Application::Get().GetRenderer3D().CreateLitMaterial(MaterialParams()
				.SetDebugName("Knight")
				.SetRoughness(PLAYER_MODEL_ROUGHNESS));
			if (knightTexture)
				knight.Lit->SetTexture(0, knightTexture);
		}
		else
		{
			DE_ERROR("Hearthvale: failed to load '{}'", k_KnightPath);
		}
		m_Characters.push_back(std::move(knight));
		m_Characters.front().Lit = m_Material;
	}

	FarmerAssets::~FarmerAssets()
	{
		DestroyAndDelete(m_Material);
		for (size_t i = 1; i < m_Characters.size(); ++i)
			DestroyAndDelete(m_Characters[i].Lit);
	}

	const CharacterAsset* FarmerAssets::FindCharacter(std::string_view modelPath) const
	{
		for (const CharacterAsset& character : m_Characters)
		{
			if (character.Path == modelPath)
				return &character;
		}
		return nullptr;
	}

}
