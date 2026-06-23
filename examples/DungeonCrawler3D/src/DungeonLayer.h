#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <vector>

namespace Dingo
{

	// A 3D dungeon-crawler prototype built entirely on the engine's new 3D scene.
	//
	// Everything is an ECS entity in a single Scene: the floor and walls are static box
	// rigid bodies, the player is a dynamic sphere, skeletons are dynamic spheres that
	// chase + can be killed, and treasures are body-less spinning boxes collected by
	// proximity. The Scene owns the Jolt-backed Physics3D world and renders the meshes
	// through Renderer3D — this layer feeds input, runs the game rules, and draws the HUD.
	//
	// Dungeons are PROCEDURALLY GENERATED (rooms + corridors; see DungeonGenerator.h),
	// like the 2D DungeonCrawler — a fresh layout every run / restart. Combat mirrors the
	// 2D version: SPACE does a radial melee swing; enemies have health and chase within an
	// aggro range; the player has a health bar and brief invulnerability after being hit.
	//
	// Primitives only (boxes + spheres) — a compact slice of the v0.5 "3D dungeon crawler".
	class DungeonLayer : public Layer
	{
	public:
		DungeonLayer() : Layer("DungeonCrawler3D") {}
		virtual ~DungeonLayer() = default;

	public:
		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(float deltaTime) override;
		void OnEvent(Event& event) override;

	private:
		enum class State { Playing, Won, Lost };

		// A simulated enemy plus its gameplay state (health / hit-flash). Game-specific
		// data lives here in the layer rather than in an ECS component, keeping the
		// engine's built-in component set unchanged.
		struct Enemy
		{
			Entity Handle;
			float Health = 0.0f;
			float HitFlash = 0.0f;
		};

		// Scene construction
		void BuildDungeon();
		void Restart();
		glm::vec3 CellToWorld(int col, int row, float y) const;

		Entity SpawnFloor(float worldWidth, float worldDepth);
		Entity SpawnWall(const glm::vec3& position, const glm::vec3& scale, const glm::vec4& color);
		Entity SpawnPlayer(const glm::vec3& position);
		Entity SpawnTreasure(const glm::vec3& position);
		Entity SpawnEnemy(const glm::vec3& position);

		// Gameplay
		void UpdatePlayer(float deltaTime);
		void UpdateEnemies(float deltaTime);
		void UpdateTreasures(float deltaTime);
		void Attack();                       // radial melee swing (SPACE)
		void UpdateAttackFx(float deltaTime); // the expanding ground-slam ring
		void UpdateCamera();

		// Rendering
		void RenderHud();
		void UpdateOrthoProjection();
		void DrawCenteredText(Renderer2D& renderer, const std::string& text, float size, float y, const glm::vec4& color);

	private:
		std::unique_ptr<Scene> m_Scene;

		PerspectiveCamera m_Camera;
		float m_AspectRatio = 16.0f / 9.0f;

		// Borrowed from Renderer3D — unit box (1x1x1) and unit-diameter sphere.
		Mesh* m_BoxMesh = nullptr;
		Mesh* m_SphereMesh = nullptr;

		Font* m_Font = nullptr;
		glm::mat4 m_OrthoProjection = glm::mat4(1.0f);
		float m_OrthoSize = 11.0f;

		// Generated-dungeon dimensions (in tiles), used by CellToWorld for centering.
		int m_GridCols = 0;
		int m_GridRows = 0;
		unsigned int m_Seed = 0;

		// Entities / gameplay state.
		Entity m_Player;
		glm::vec3 m_PlayerSpawn{ 0.0f };
		std::vector<Entity> m_Treasures;
		std::vector<Enemy> m_Enemies;

		Entity m_AttackFx;          // expanding translucent ring, briefly alive after a swing
		float m_AttackFxTime = 0.0f;
		float m_AttackCooldown = 0.0f;

		// Game state
		State m_State = State::Playing;
		float m_PlayerHealth = 0.0f;
		float m_Invuln = 0.0f;      // brief i-frames after taking contact damage
		int m_Collected = 0;
		int m_TotalTreasure = 0;
		int m_EnemiesSlain = 0;
		float m_ElapsedTime = 0.0f; // drives treasure spin / bob
	};

}
