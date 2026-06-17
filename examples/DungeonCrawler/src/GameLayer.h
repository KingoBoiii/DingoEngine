#pragma once
#include <DingoEngine.h>

#include "GameContext.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace Dingo
{

	// Top-down dungeon crawler, built on the v0.3 Scene system.
	//
	// The room tiles, the player, the enemies, and dropped loot are all entities
	// with built-in components; their behaviour lives in ScriptableEntity scripts
	// (see GameScripts.h). This layer is a thin orchestrator: it generates a random
	// dungeon (see DungeonGenerator.h), spawns the actors, drives the follow-camera,
	// and draws the HUD / feedback overlay on top of the rendered scene.
	//
	// The dungeon is regenerated on every run, so each restart (R) is a fresh layout.
	class GameLayer : public Layer
	{
	public:
		GameLayer() : Layer("Dungeon Crawler Game Layer")
		{}
		virtual ~GameLayer() = default;

	public:
		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(float deltaTime) override;

	private:
		void GenerateLevel(); // roll a new random dungeon -> tile map + spawns
		void BuildTiles();    // (re)build the tile entities from the current map
		void ResetGame();     // regenerate the dungeon, then (re)spawn player + enemies
		void UpdateCamera();
		int CountEnemies();
		int CountLoot();

		void RenderOverlay(Renderer2D& r, float dt);
		void DrawHealthBar(Renderer2D& r, const glm::vec2& center, float width, float frac, const glm::vec4& color, float height = 0.18f);
		void DrawCenteredText(Renderer2D& r, const std::string& text, float size, const glm::vec2& offset, const glm::vec4& color);

	private:
		Scene m_Scene{ "Dungeon" };
		Font* m_Font = nullptr;
		GameContext m_Context;

		// Tile entities for the current dungeon, torn down and rebuilt on each
		// regeneration (BuildTiles).
		std::vector<Entity> m_TileEntities;
		unsigned int m_Seed = 0; // seed of the current dungeon (shown in the HUD)

		// --- Follow camera ---
		float m_HalfW = 0.0f;
		float m_HalfH = 0.0f;
		glm::vec2 m_CameraPos{ 0.0f };
		glm::mat4 m_WorldVP{ 1.0f };

		// --- Spawns (parsed from the room layout) ---
		glm::vec2 m_PlayerSpawn{ 0.0f };
		std::vector<glm::vec2> m_EnemySpawns;
	};

}
