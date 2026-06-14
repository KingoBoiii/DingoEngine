#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace Dingo
{

	enum class PlayState
	{
		Playing,
		Dead,
		Cleared
	};

	struct Enemy
	{
		glm::vec2 Position{ 0.0f };
		float Health = 30.0f;
		float MaxHealth = 30.0f;
		float HitFlash = 0.0f; // seconds of hit-flash remaining
		bool Alive = true;
	};

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
		void LoadRoom();
		void ResetGame();

		void UpdateCamera();
		void UpdatePlayer(float dt);
		void UpdateCombat(float dt);
		void UpdateEnemies(float dt);
		void UpdateLoot();

		void RenderRoom(Renderer2D& r);
		void RenderEntities(Renderer2D& r);
		void RenderHud(Renderer2D& r, float dt);

		// Tile / collision helpers
		glm::vec2 TileToWorld(int col, int row) const;
		bool IsSolidTile(int col, int row) const;
		bool IsSolidWorld(float x, float y) const;
		glm::vec2 MoveWithCollision(const glm::vec2& pos, const glm::vec2& delta, float half) const;

		// Render helpers
		void DrawHealthBar(Renderer2D& r, const glm::vec2& center, float width, float frac, const glm::vec4& color, float height = 0.18f);
		void DrawCenteredText(Renderer2D& r, const std::string& text, float size, const glm::vec2& offset, const glm::vec4& color);

	private:
		// --- Camera ---
		float m_OrthoSize = 13.0f;
		float m_OrthoNear = -1.0f;
		float m_OrthoFar = 1.0f;
		glm::vec2 m_CameraPos{ 0.0f };
		float m_HalfW = 0.0f;
		float m_HalfH = 0.0f;

		// --- Map ---
		const float m_TileSize = 1.0f;
		std::vector<std::string> m_Map;
		int m_MapWidth = 0;
		int m_MapHeight = 0;
		glm::vec2 m_PlayerSpawn{ 0.0f };
		std::vector<glm::vec2> m_EnemySpawns;

		// --- Player ---
		glm::vec2 m_PlayerPos{ 0.0f };
		glm::vec2 m_PlayerFacing{ 0.0f, -1.0f };
		const float m_PlayerHalf = 0.34f;
		const float m_PlayerSpeed = 4.5f;
		float m_PlayerHealth = 100.0f;
		const float m_PlayerMaxHealth = 100.0f;
		float m_PlayerInvuln = 0.0f;
		float m_AttackTimer = 0.0f;
		float m_AttackCooldown = 0.0f;

		// --- Tuning ---
		const float m_PlayerAttackRange = 1.5f;
		const float m_PlayerAttackDamage = 18.0f;
		const float m_PlayerAttackActive = 0.12f;
		const float m_PlayerAttackCooldownTime = 0.35f;
		const float m_PlayerInvulnTime = 0.6f;

		const float m_EnemyHalf = 0.34f;
		const float m_EnemySpeed = 2.3f;
		const float m_EnemyAggroRange = 7.0f;
		const float m_EnemyContactDamage = 12.0f;

		// --- Loot ---
		std::vector<glm::vec2> m_Loot;        // dropped gem positions (world space)
		int m_LootCollected = 0;
		const float m_LootHalf = 0.16f;
		const float m_LootPickupDist = 0.6f;

		// --- State ---
		std::vector<Enemy> m_Enemies;
		PlayState m_State = PlayState::Playing;
		Font* m_Font = nullptr;
	};

}
