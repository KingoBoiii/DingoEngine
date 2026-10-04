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

	// Loads everything the Moveset names through the AssetManager, which owns the models and textures.
	// The lit materials are the game's, so they go before the renderer does: destroy this in OnDetach.
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

		// In GetLibraryDefs() order; null where a library failed to load.
		std::span<Model* const> GetLibraries() const { return m_Libraries; }
		const ClipSet& GetClips() const { return m_Clips; }
		const AnimationClip* GetClip(std::string_view name) const { return m_Clips.Find(name); }
		// Any clip of any library, used or not (--pose can show one the Moveset never plays).
		const AnimationClip* FindAnyClip(std::string_view name) const;
		const GameSounds& GetSounds() const { return m_Sounds; }

	private:
		Model* LoadModel(const char* path);
		Texture* LoadTexture(const char* path);
		std::shared_ptr<AudioClip> LoadSound(const char* path);
		void BreakHitboxes();

	private:
		std::unordered_map<std::string, Model*> m_Models;
		std::unordered_map<std::string, Texture*> m_Textures;
		std::unordered_map<std::string, Material*> m_Materials;
		std::vector<Model*> m_Libraries;
		ClipSet m_Clips;
		GameSounds m_Sounds;
	};

}
