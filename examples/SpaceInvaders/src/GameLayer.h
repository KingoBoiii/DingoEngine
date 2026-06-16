#pragma once
#include <DingoEngine.h>

#include "GameComponents.h"

#include <glm/glm.hpp>

namespace Dingo
{

	// Space Invaders, built on the v0.3 Scene/ECS system.
	//
	// Three scenes (Menu / Game / GameOver) are owned by a SceneManager and swapped
	// as the game progresses. Every game object — the player, each invader, each
	// bullet, and each destructible shield block — is an entity in the Game scene.
	// Gameplay runs as a set of "systems": methods that query the scene with
	// Scene::GetAllEntitiesWith<...>() and mutate components.
	class GameLayer : public Layer
	{
	public:
		GameLayer() : Layer("Space Invaders Game Layer")
		{}
		virtual ~GameLayer() = default;

	public:
		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(float deltaTime) override;

	private:
		// --- Scene construction ---
		void BuildMenuScene();
		void BuildGameOverScene();

		void StartGame();
		void BuildHud();
		void SpawnPlayer();
		void SpawnShields();
		void SpawnShieldBunker(const glm::vec2& center);
		void SpawnInvaderFormation();

		// --- Systems (operate on the Game scene) ---
		void UpdatePlayer(float dt);
		void UpdateFormation(float dt);
		void UpdateFiring(float dt);
		void UpdateBullets(float dt);
		void UpdateCollisions();
		void UpdateHud();

		void EndGame();
		int CountInvaders();

		const uint32_t GetGameVersion() const { return DE_MAKE_VERSION(1, 0, 0); }

	private:
		Font* m_Font = nullptr;

		SceneManager m_SceneManager;
		Scene* m_MenuScene = nullptr;
		Scene* m_GameScene = nullptr;
		Scene* m_GameOverScene = nullptr;

		// --- Camera / playfield (world units) ---
		glm::mat4 m_ViewProjection{ 1.0f };
		float m_HalfWidth = 0.0f;
		float m_HalfHeight = 0.0f;

		// --- Cached handles into the Game scene ---
		Entity m_Player;
		Entity m_ScoreText;
		Entity m_LivesText;
		Entity m_WaveText;
		Entity m_GameOverScoreText;

		// --- Game state ---
		int m_Score = 0;
		int m_Lives = 3;
		int m_Wave = 1;
		bool m_Defeat = false;

		// --- Invader formation state ---
		float m_InvaderDirection = 1.0f; // +1 = right, -1 = left
		float m_InvaderSpeed = 0.0f;     // current horizontal speed (units/s)
		float m_InvaderFireTimer = 0.0f;

		// --- Player firing ---
		float m_PlayerFireCooldown = 0.0f;
	};

}
