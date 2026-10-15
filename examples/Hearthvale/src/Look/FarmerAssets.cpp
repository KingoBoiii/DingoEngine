#include "Look/FarmerAssets.h"
#include "GameTuning.h"

#include <algorithm>

namespace Dingo
{

	namespace
	{
		constexpr const char* k_ModelPath = "kaykit/characters/Barbarian.glb";
		constexpr const char* k_TexturePath = "kaykit/characters/barbarian_texture.png";
		constexpr const char* k_GeneralPath = "kaykit/animations/Rig_Medium_General.glb";
		constexpr const char* k_MovementPath = "kaykit/animations/Rig_Medium_MovementBasic.glb";
		constexpr const char* k_IdleName = "Idle_A";
		constexpr const char* k_WalkName = "Walking_A";
	}

	std::vector<const char*> FarmerAssetPaths()
	{
		return { k_ModelPath, k_TexturePath, k_GeneralPath, k_MovementPath };
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

		for (const SubMesh& submesh : m_Model->GetSubMeshes())
		{
			if (!submesh.Mesh)
				continue;
			for (const MeshVertex& vertex : submesh.Mesh->GetVertices())
				m_ModelHeight = (std::max)(m_ModelHeight, vertex.Position.y);
		}
	}

	FarmerAssets::~FarmerAssets()
	{
		DestroyAndDelete(m_Material);
	}

}
