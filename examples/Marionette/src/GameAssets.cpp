#include "GameAssets.h"
#include "GameTuning.h"
#include "HitGeometry.h"
#include "LaunchOptions.h"

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <vector>

namespace
{
	constexpr const char* k_FootstepPath = "audio/footstep.wav";
	constexpr const char* k_SwingPath = "audio/swing.wav";
	constexpr const char* k_HitPath = "audio/hit.wav";
	constexpr const char* k_BlockPath = "audio/block.wav";
	constexpr const char* k_ParryPath = "audio/parry.wav";
	constexpr const char* k_DodgePath = "audio/dodge.wav";
	constexpr const char* k_KoPath = "audio/ko.wav";
	constexpr const char* k_WinPath = "audio/win.wav";
	constexpr const char* k_LosePath = "audio/lose.wav";
	constexpr const char* k_CracklePath = "audio/brazier.wav";
	constexpr const char* k_FontPath = "fonts/arialbd.ttf";

	bool SameEvents(const std::vector<Dingo::AnimationClipEvent>& a, const std::vector<Dingo::AnimationClipEvent>& b)
	{
		return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](const Dingo::AnimationClipEvent& x, const Dingo::AnimationClipEvent& y)
		{
			return x.Name == y.Name && x.Time == y.Time && x.EndTime == y.EndTime && x.Range == y.Range;
		});
	}
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
			m_Libraries.push_back(LoadModel(library.Path, ModelLoadParams().SetClipsOnly(true)));

		m_Clips = ResolveClips(m_Libraries);
		if (GetLaunchOptions().BreakHitbox)
			BreakHitboxes();
		ValidateMoveset(m_Clips);
		WatchEvents();

		AssetManager& assets = Application::Get().GetAssetManager();
		m_Font = assets.GetFont(assets.Load(k_FontPath));
		if (!m_Font)
		{
			DE_ERROR("Marionette: failed to load font '{}'", k_FontPath);
		}

		BuildArena();
		if (GetLaunchOptions().DebugHitbox)
			m_DebugView = std::make_unique<HitDebugView>();
		FindProblems();

		m_Sounds.Footstep = LoadSound(k_FootstepPath);
		m_Sounds.Swing = LoadSound(k_SwingPath);
		m_Sounds.Hit = LoadSound(k_HitPath);
		m_Sounds.Block = LoadSound(k_BlockPath);
		m_Sounds.Parry = LoadSound(k_ParryPath);
		m_Sounds.Dodge = LoadSound(k_DodgePath);
		m_Sounds.Ko = LoadSound(k_KoPath);
		m_Sounds.Win = LoadSound(k_WinPath);
		m_Sounds.Lose = LoadSound(k_LosePath);
		m_Sounds.Crackle = LoadSound(k_CracklePath);
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

	void GameAssets::WatchEvents()
	{
		m_Watches.assign(m_Libraries.size(), {});
		for (size_t i = 0; i < m_Libraries.size(); ++i)
		{
			const Model* library = m_Libraries[i];
			for (uint32_t k = 0; library && k < library->GetAnimationCount(); ++k)
			{
				const AnimationClip* clip = library->GetAnimation(k);
				m_Watches[i].push_back({ clip->GetEventRevision(), clip->GetEvents() });
			}
		}
	}

	std::vector<EventChange> GameAssets::PollEventChanges()
	{
		std::vector<EventChange> changes;
		for (size_t i = 0; i < m_Libraries.size(); ++i)
		{
			const Model* library = m_Libraries[i];
			if (!library)
				continue;

			const uint32_t count = library->GetAnimationCount();
			std::vector<ClipWatch>& watches = m_Watches[i];
			watches.resize(count);

			bool moved = false;
			int differing = 0;
			for (uint32_t k = 0; k < count; ++k)
			{
				const AnimationClip* clip = library->GetAnimation(k);
				ClipWatch& watch = watches[k];
				if (clip->GetEventRevision() == watch.Revision)
					continue;

				moved = true;
				differing += SameEvents(clip->GetEvents(), watch.Events) ? 0 : 1;
				watch.Revision = clip->GetEventRevision();
				watch.Events = clip->GetEvents();
			}

			if (moved)
				changes.push_back({ library->GetFilePath().stem().string() + ".events", differing });
		}

		if (!changes.empty())
			++m_EventGeneration;
		return changes;
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

		m_DebugView.reset();
		for (Material* material : { m_Arena.Floor, m_Arena.Wall, m_Arena.Brazier, m_Arena.Flame })
			DestroyAndDelete(material);
		delete m_Arena.FlameMesh;
	}

	void GameAssets::BuildArena()
	{
		Renderer3D& renderer3D = Application::Get().GetRenderer3D();
		m_Arena.FlameMesh = Mesh::CreateSphere(FLAME_MESH_RADIUS, FLAME_MESH_RINGS, FLAME_MESH_SEGMENTS);
		m_Arena.Floor = renderer3D.CreateLitMaterial(MaterialParams().SetDebugName("ArenaFloor").SetRoughness(FLOOR_ROUGHNESS));
		m_Arena.Wall = renderer3D.CreateLitMaterial(MaterialParams().SetDebugName("ArenaWall").SetRoughness(WALL_ROUGHNESS));
		m_Arena.Brazier = renderer3D.CreateLitMaterial(MaterialParams().SetDebugName("ArenaBrazier").SetRoughness(BRAZIER_ROUGHNESS));
		m_Arena.Flame = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("ArenaFlame")
			.SetEmissiveColor(FLAME_COLOR)
			.SetEmissiveStrength(GetLaunchOptions().NoPost ? FLAME_EMISSIVE_NO_POST : FLAME_EMISSIVE));
	}

	void GameAssets::FindProblems()
	{
		for (size_t i = 0; i < m_Libraries.size(); ++i)
		{
			if (!m_Libraries[i])
				m_Problems.push_back(std::format("clip library '{}'", GetLibraryDefs()[i].Path));
		}
		for (const FighterDef& fighter : GetFighterDefs())
		{
			const Model* model = GetCharacter(fighter);
			if (!model || !model->GetSkeleton())
				m_Problems.push_back(std::format("character model '{}'", fighter.Model));
		}
		for (const std::string& clip : m_Clips.GetMissing())
			m_Problems.push_back(std::format("clip '{}'", clip));

		if (!m_Problems.empty())
		{
			std::string list;
			for (const std::string& problem : m_Problems)
				list += std::format("{}{}", list.empty() ? "" : ", ", problem);
			DE_ERROR("Marionette: bouts cannot be played, missing: {}", list);
		}
	}

	Model* GameAssets::LoadModel(const char* path, const ModelLoadParams& params)
	{
		if (const auto it = m_Models.find(path); it != m_Models.end())
			return it->second;

		AssetManager& assets = Application::Get().GetAssetManager();
		Model* model = assets.GetModel(assets.Load(path, params));
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
