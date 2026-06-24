#pragma once
#include <DingoEngine.h>

#include "GameContext.h"

#include <glm/glm.hpp>

namespace Dingo
{

	// Space Invaders, built on the v0.3 Scene system.
	//
	// The engine's ECS backend is fully hidden: the player, invaders, bullets, and
	// shield blocks are entities with built-in components (Transform + Sprite), and
	// all gameplay logic lives in ScriptableEntity behaviours (see GameScripts.h).
	// This layer is the thin orchestrator — it owns the scenes, the shared game
	// state, and the HUD, and switches between Menu / Game / GameOver.
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
		void BuildMenuScene();
		void BuildGameOverScene();
		void SetupCamera(Scene* scene);

		void StartGame();
		void BuildHud();
		void SpawnPlayer();
		void SpawnShields();
		void SpawnShieldBunker(const glm::vec2& center);

		void UpdateHud();
		void EndGame();

	private:
		Font* m_Font = nullptr;

		SceneManager m_SceneManager;
		Scene* m_MenuScene = nullptr;
		Scene* m_GameScene = nullptr;
		Scene* m_GameOverScene = nullptr;

		GameContext m_Context;

		// HUD entities, updated each frame from m_Context.
		Entity m_ScoreText;
		Entity m_LivesText;
		Entity m_WaveText;
		Entity m_GameOverScoreText;
	};

}
