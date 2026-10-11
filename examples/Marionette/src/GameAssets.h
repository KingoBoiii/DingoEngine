#pragma once
#include "Audio.h"
#include "Moveset.h"

#include <DingoEngine.h>

#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace Dingo
{

	class HitDebugView;

	// A clip library whose clips' events a hot-reload replaced: its .events file, and how many clips now differ.
	struct EventChange
	{
		std::string File;
		int Clips = 0;
	};

	// What the arena is built from, made once: every bout and every screen with the arena in it draws with these.
	struct ArenaAssets
	{
		Mesh* FlameMesh = nullptr;
		Material* Floor = nullptr;
		Material* Wall = nullptr;
		Material* Brazier = nullptr;
		Material* Flame = nullptr;
	};

	// Loads everything the Moveset names through the AssetManager, which owns the models, the textures and the font.
	// The lit materials, the arena's mesh and the debug view are the game's, so they go before the renderer does:
	// destroy this in OnDetach.
	class GameAssets
	{
	public:
		GameAssets();
		~GameAssets();

		GameAssets(const GameAssets&) = delete;
		GameAssets& operator=(const GameAssets&) = delete;

		Model* GetCharacter(const FighterDef& fighter) const;
		Model* GetModel(const char* path) const;
		Texture* GetTexture(const char* path) const;
		// One per texture, shared by the fighters that wear it and the weapons they carry.
		Material* GetMaterial(const FighterDef& fighter) const;
		Font* GetFont() const { return m_Font; }
		const ArenaAssets& GetArena() const { return m_Arena; }
		// Null unless --debug-hitbox.
		const HitDebugView* GetDebugView() const { return m_DebugView.get(); }

		// What failed to load that a bout cannot be played without; empty when it can.
		const std::vector<std::string>& GetProblems() const { return m_Problems; }
		bool IsPlayable() const { return m_Problems.empty(); }

		// In GetLibraryDefs() order; null where a library failed to load.
		std::span<Model* const> GetLibraries() const { return m_Libraries; }
		const ClipSet& GetClips() const { return m_Clips; }
		const AnimationClip* GetClip(std::string_view name) const { return m_Clips.Find(name); }
		// Any clip of any library, used or not (--pose can show one the Moveset never plays).
		const AnimationClip* FindAnyClip(std::string_view name) const;
		const GameSounds& GetSounds() const { return m_Sounds; }

		// The libraries whose clips got new events (an AnimationClip::GetEventRevision moved) since the last call.
		std::vector<EventChange> PollEventChanges();
		// Counts the polls that found a change, for whoever derived something from the events.
		uint32_t GetEventGeneration() const { return m_EventGeneration; }

	private:
		struct ClipWatch
		{
			uint64_t Revision = 0;
			std::vector<AnimationClipEvent> Events;
		};

		Model* LoadModel(const char* path, const ModelLoadParams& params = ModelLoadParams());
		Texture* LoadTexture(const char* path);
		std::shared_ptr<AudioClip> LoadSound(const char* path);
		void BuildArena();
		void FindProblems();
		void BreakHitboxes();
		void WatchEvents();

	private:
		std::unordered_map<std::string, Model*> m_Models;
		std::unordered_map<std::string, Texture*> m_Textures;
		std::unordered_map<std::string, Material*> m_Materials;
		std::vector<Model*> m_Libraries;
		Font* m_Font = nullptr;
		ArenaAssets m_Arena;
		std::unique_ptr<HitDebugView> m_DebugView;
		ClipSet m_Clips;
		GameSounds m_Sounds;
		std::vector<std::string> m_Problems;
		std::vector<std::vector<ClipWatch>> m_Watches;
		uint32_t m_EventGeneration = 0;
	};

}
