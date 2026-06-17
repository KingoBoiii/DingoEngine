#pragma once
#include <DingoEngine.h>

#include "GameContext.h"
#include "GameTuning.h"

#include <glm/glm.hpp>

// Gameplay logic as ScriptableEntity behaviours. The player, each enemy, and each
// loot gem is an entity with built-in components; these scripts drive their
// behaviour and find each other via Scene::GetScriptsOfType<>(). No EnTT, no
// engine-side custom components.
namespace Dingo
{

	// Player ship: movement (with tile collision), facing, a radial melee attack,
	// taking contact damage with an invulnerability window, and loot pickup.
	class PlayerScript : public ScriptableEntity
	{
	public:
		PlayerScript(GameContext* context) : m_Context(context) {}

		const glm::vec2& Facing() const { return m_Facing; }
		bool IsAttacking() const { return m_AttackTimer > 0.0f; }
		bool IsInvulnerable() const { return m_Invuln > 0.0f; }

	protected:
		void OnUpdate(float deltaTime) override;

	private:
		GameContext* m_Context = nullptr;
		glm::vec2 m_Facing{ 0.0f, -1.0f };
		float m_Invuln = 0.0f;
		float m_AttackTimer = 0.0f;
		float m_AttackCooldown = 0.0f;
	};

	// Enemy: chases the player (with tile collision) when in aggro range. Takes
	// damage from the player's melee; drops a loot gem and despawns when killed.
	class EnemyScript : public ScriptableEntity
	{
	public:
		EnemyScript(GameContext* context) : m_Context(context) {}

		void Damage(float amount);
		float Health() const { return m_Health; }
		float MaxHealth() const { return m_MaxHealth; }

	protected:
		void OnUpdate(float deltaTime) override;

	private:
		GameContext* m_Context = nullptr;
		float m_Health = ENEMY_MAX_HEALTH;
		float m_MaxHealth = ENEMY_MAX_HEALTH;
		float m_HitFlash = 0.0f;
	};

	// Marker behaviour for a dropped loot gem, so the player can find gems via
	// Scene::GetScriptsOfType<LootScript>().
	class LootScript : public ScriptableEntity
	{
	};

	// --- Spawn helpers ---

	Entity SpawnPlayer(Scene& scene, GameContext& context, const glm::vec2& position);
	Entity SpawnEnemy(Scene& scene, GameContext& context, const glm::vec2& position);
	Entity SpawnLoot(Scene& scene, GameContext& context, const glm::vec2& position);

}
