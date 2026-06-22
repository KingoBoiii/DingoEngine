#pragma once
#include <DingoEngine.h>

#include "GameContext.h"

#include <glm/glm.hpp>

namespace Dingo
{

	// Angry Birds, built on the v0.4 physics system and the v0.3 Scene/ECS.
	//
	// The ground, the destructible block structures, the pigs, and the launched
	// birds are all entities with a RigidBody2DComponent plus a Box/Circle collider;
	// the engine's physics world (Box2D, hidden behind the Scene API) simulates them.
	// This layer is the thin orchestrator: it owns the scenes, builds the levels,
	// drives keyboard aiming + launching, and resolves win/lose.
	class GameLayer : public Layer
	{
	public:
		GameLayer() : Layer("Angry Birds Game Layer")
		{}
		virtual ~GameLayer() = default;

	public:
		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(float deltaTime) override;

	private:
		// Scene construction / flow
		void BuildMenuScene();
		void StartGame();          // from the menu: reset score, load level 1
		void StartLevel(int level);
		void BuildHud();
		void EndGame(bool won);    // transition to the GameOver scene

		// Level content
		void SpawnGround();
		void SpawnStructure(int level);
		Entity SpawnBlock(const glm::vec2& center, const glm::vec2& size, const glm::vec4& color);
		Entity SpawnPig(const glm::vec2& center);
		void SpawnWaitingBird();   // a ready-to-fire bird sitting on the slingshot

		// Per-frame game logic (active "Game" scene)
		void UpdateGame(float deltaTime);
		void HandleAiming();       // mouse drag-to-launch on the current bird
		void LaunchBird(const glm::vec2& velocity);
		void ResolveBird();        // current bird came to rest / left play -> next bird or lose
		void UpdateHud();

		// Rendering
		void RenderGame();         // scene entities + the aiming overlay

		glm::vec2 ScreenToWorld(const glm::vec2& screen) const; // window pixels -> world units

	private:
		Font* m_Font = nullptr;

		SceneManager m_SceneManager;
		Scene* m_MenuScene = nullptr;
		Scene* m_GameScene = nullptr;
		Scene* m_GameOverScene = nullptr;

		glm::mat4 m_ViewProjection{ 1.0f };
		GameContext m_Context;

		glm::vec2 m_SlingPos{ 0.0f };

		// Aiming / current bird state.
		Entity m_Bird;             // the bird currently on the sling or in flight
		bool m_Aiming = false;     // true while the bird waits to be launched
		bool m_Dragging = false;   // true while the player is pulling the bird back
		bool m_BirdInFlight = false;
		glm::vec2 m_LaunchVelocity{ 0.0f }; // from the current drag (drives the preview + the launch)
		float m_FlightTimer = 0.0f;
		float m_SettleTimer = 0.0f;

		// HUD entities, refreshed each frame from m_Context.
		Entity m_ScoreText;
		Entity m_LevelText;
		Entity m_BirdsText;
		Entity m_PigsText;
		Entity m_HintText;

		// GameOver text, set on the transition.
		Entity m_ResultText;
		Entity m_ResultScoreText;
	};

}
