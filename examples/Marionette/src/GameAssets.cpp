#include "GameAssets.h"
#include "GameTuning.h"

namespace Dingo
{

	GameAssets::GameAssets()
	{
		Renderer3D& renderer3D = Application::Get().GetRenderer3D();

		for (const FighterDef& fighter : GetFighterDefs())
		{
			LoadModel(fighter.Model);
			Texture* texture = LoadTexture(fighter.Texture);
			if (fighter.RightWeapon)
				LoadModel(fighter.RightWeapon);
			if (fighter.LeftWeapon)
				LoadModel(fighter.LeftWeapon);

			if (!m_Materials.contains(fighter.Texture))
			{
				Material* material = renderer3D.CreateLitMaterial(MaterialParams()
					.SetDebugName(fighter.Texture)
					.SetRoughness(FIGHTER_ROUGHNESS));
				if (texture)
					material->SetTexture(0, texture);
				m_Materials.emplace(fighter.Texture, material);
			}
		}

		for (const LibraryDef& library : GetLibraryDefs())
			m_Libraries.push_back(LoadModel(library.Path));

		m_Clips = ResolveClips(m_Libraries);
	}

	GameAssets::~GameAssets()
	{
		for (auto& [path, material] : m_Materials)
			DestroyAndDelete(material);
	}

	Model* GameAssets::LoadModel(const char* path)
	{
		if (const auto it = m_Models.find(path); it != m_Models.end())
			return it->second;

		AssetManager& assets = Application::Get().GetAssetManager();
		Model* model = assets.GetModel(assets.Load(path));
		if (!model)
		{
			DE_ERROR("Marionette: failed to load model '{}'", path);
		}
		m_Models.emplace(path, model);
		return model;
	}

	Texture* GameAssets::LoadTexture(const char* path)
	{
		if (const auto it = m_Textures.find(path); it != m_Textures.end())
			return it->second;

		AssetManager& assets = Application::Get().GetAssetManager();
		Texture* texture = assets.GetTexture(assets.Load(path));
		if (!texture)
		{
			DE_ERROR("Marionette: failed to load texture '{}'", path);
		}
		m_Textures.emplace(path, texture);
		return texture;
	}

	Model* GameAssets::GetCharacter(const FighterDef& fighter) const
	{
		return GetModel(fighter.Model);
	}

	Model* GameAssets::GetModel(const char* path) const
	{
		const auto it = m_Models.find(path);
		return it != m_Models.end() ? it->second : nullptr;
	}

	Texture* GameAssets::GetTexture(const char* path) const
	{
		const auto it = m_Textures.find(path);
		return it != m_Textures.end() ? it->second : nullptr;
	}

	Material* GameAssets::GetMaterial(const FighterDef& fighter) const
	{
		const auto it = m_Materials.find(fighter.Texture);
		return it != m_Materials.end() ? it->second : nullptr;
	}

}
