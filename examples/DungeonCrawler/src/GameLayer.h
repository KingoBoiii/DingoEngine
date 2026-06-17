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
	// (see GameScripts.h). This layer is a thin orchestrator: it builds the room,
	// spawns the actors, drives the follow-camera, and draws the HUD / feedback
	// overlay on top of the rendered scene.
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
		void LoadRoom();   // parse the layout -> tile map + spawns + tile entities
		void ResetGame();  // (re)spawn the player and enemies
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
