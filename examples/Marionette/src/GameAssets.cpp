#include "GameAssets.h"
#include "GameTuning.h"
#include "LaunchOptions.h"

#include <algorithm>
#include <optional>
#include <vector>

namespace
{
	constexpr const char* k_FootstepPath = "audio/footstep.wav";
	constexpr const char* k_SwingPath = "audio/swing.wav";
	constexpr const char* k_HitPath = "audio/hit.wav";
	constexpr const char* k_BlockPath = "audio/block.wav";
	constexpr const char* k_ParryPath = "audio/parry.wav";
	constexpr const char* k_DodgePath = "audio/dodge.wav";
}

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
		if (GetLaunchOptions().BreakHitbox)
			BreakHitboxes();
		ValidateMoveset(m_Clips);

		m_Sounds.Footstep = LoadSound(k_FootstepPath);
		m_Sounds.Swing = LoadSound(k_SwingPath);
		m_Sounds.Hit = LoadSound(k_HitPath);
		m_Sounds.Block = LoadSound(k_BlockPath);
		m_Sounds.Parry = LoadSound(k_ParryPath);
		m_Sounds.Dodge = LoadSound(k_DodgePath);
	}

	std::shared_ptr<AudioClip> GameAssets::LoadSound(const char* path)
	{
		std::shared_ptr<AudioClip> clip = Application::Get().GetAudioEngine().LoadClip(path);
		if (!clip)
		{
			DE_ERROR("Marionette: failed to load audio clip '{}'", path);
		}
		return clip;
	}

	const AnimationClip* GameAssets::FindAnyClip(std::string_view name) const
	{
		for (const Model* library : m_Libraries)
		{
			if (const AnimationClip* clip = library ? library->FindAnimation(name) : nullptr)
				return clip;
		}
		return nullptr;
	}

	void GameAssets::BreakHitboxes()
	{
		size_t moved = 0;
		for (const MoveDef& move : GetMoves())
		{
			AnimationClip* clip = nullptr;
			for (const Model* library : m_Libraries)
			{
				if (library && (clip = library->FindAnimation(move.Clip)))
					break;
			}

			if (!clip || !FindRange(*clip, Events::HITBOX))
				continue;

			const std::vector<AnimationClipEvent> events = clip->GetEvents();
			clip->ClearEvents();

			const float begin = clip->GetDuration() - move.FadeOut + BREAK_HITBOX_MARGIN;
			const float end = std::min(begin + BREAK_HITBOX_LENGTH, clip->GetDuration());
			for (const AnimationClipEvent& event : events)
			{
				if (event.Range && event.Name == Events::HITBOX)
					clip->AddEventRange(begin, end, event.Name);
				else if (event.Range)
					clip->AddEventRange(event.Time, event.EndTime, event.Name);
				else
					clip->AddEvent(event.Time, event.Name);
			}
			++moved;
		}
		DE_WARN("Marionette: --break-hitbox moved the hitbox of {} moves into the tail of their one-shots; none of them should land", moved);
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
