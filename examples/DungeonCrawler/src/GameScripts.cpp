#include "GameScripts.h"

#include <glm/glm.hpp>

#include <cmath>

namespace Dingo
{

	// --- Spawn helpers ----------------------------------------------------------

	Entity SpawnPlayer(Scene& scene, GameContext& context, const glm::vec2& position)
	{
		Entity player = scene.CreateEntity("Player");
		auto& transform = player.GetComponent<TransformComponent>();
		transform.Position = glm::vec3(position, 0.2f); // above tiles (z=0) and enemies (z=0.1)
		transform.Size = glm::vec2(PLAYER_HALF * 2.0f);
		player.AddComponent<SpriteRendererComponent>(COLOR_PLAYER);
		player.AddScript<PlayerScript>(&context);

		context.Player = player;
		context.PlayerHealth = PLAYER_MAX_HEALTH;
		return player;
	}

	Entity SpawnEnemy(Scene& scene, GameContext& context, const glm::vec2& position)
	{
		Entity enemy = scene.CreateEntity("Enemy");
		auto& transform = enemy.GetComponent<TransformComponent>();
		transform.Position = glm::vec3(position, 0.1f); // above tiles (z=0)
		transform.Size = glm::vec2(ENEMY_HALF * 2.0f);
		enemy.AddComponent<SpriteRendererComponent>(COLOR_ENEMY);
		enemy.AddScript<EnemyScript>(&context);
		return enemy;
	}

	Entity SpawnLoot(Scene& scene, GameContext& context, const glm::vec2& position)
	{
		(void)context;
		Entity loot = scene.CreateEntity("Loot");
		auto& transform = loot.GetComponent<TransformComponent>();
		transform.Position = glm::vec3(position, 0.0f);
		transform.Size = glm::vec2(LOOT_HALF * 2.0f);

		auto& circle = loot.AddComponent<CircleRendererComponent>();
		circle.Color = COLOR_LOOT;
		circle.Thickness = 1.0f;
		circle.Fade = 0.05f;

		loot.AddScript<LootScript>();
		return loot;
	}

	// --- PlayerScript -----------------------------------------------------------

	void PlayerScript::OnUpdate(float deltaTime)
	{
		glm::vec2 direction(0.0f);
		if (Input::IsKeyPressed(Key::W)) direction.y += 1.0f;
		if (Input::IsKeyPressed(Key::S)) direction.y -= 1.0f;
		if (Input::IsKeyPressed(Key::A)) direction.x -= 1.0f;
		if (Input::IsKeyPressed(Key::D)) direction.x += 1.0f;

		// Move (and capture our position for the checks below, since spawning loot
		// during the attack can invalidate the transform reference).
		glm::vec2 playerPos;
		{
			auto& transform = GetComponent<TransformComponent>();
			glm::vec2 pos = glm::vec2(transform.Position);
			if (direction.x != 0.0f || direction.y != 0.0f)
			{
				direction = glm::normalize(direction);
				m_Facing = direction;
				pos = m_Context->MoveWithCollision(pos, direction * PLAYER_SPEED * deltaTime, PLAYER_HALF);
				transform.Position = glm::vec3(pos, transform.Position.z);
			}
			playerPos = pos;
		}

		if (m_Invuln > 0.0f) m_Invuln -= deltaTime;
		if (m_AttackTimer > 0.0f) m_AttackTimer -= deltaTime;
		if (m_AttackCooldown > 0.0f) m_AttackCooldown -= deltaTime;

		// Radial melee: hit every enemy within range.
		if (Input::IsKeyDown(Key::Space) && m_AttackCooldown <= 0.0f)
		{
			m_AttackTimer = PLAYER_ATTACK_ACTIVE;
			m_AttackCooldown = PLAYER_ATTACK_COOLDOWN;

			for (EnemyScript* enemy : GetScene().GetScriptsOfType<EnemyScript>())
			{
				const glm::vec2 enemyPos = glm::vec2(enemy->GetEntity().GetComponent<TransformComponent>().Position);
				if (glm::length(enemyPos - playerPos) <= PLAYER_ATTACK_RANGE + ENEMY_HALF)
					enemy->Damage(PLAYER_ATTACK_DAMAGE);
			}
		}

		// Contact damage, gated by the invulnerability window.
		if (m_Invuln <= 0.0f)
		{
			const float touch = ENEMY_HALF + PLAYER_HALF;
			for (EnemyScript* enemy : GetScene().GetScriptsOfType<EnemyScript>())
			{
				if (enemy->Health() <= 0.0f)
					continue;

				const glm::vec2 enemyPos = glm::vec2(enemy->GetEntity().GetComponent<TransformComponent>().Position);
				const glm::vec2 d = playerPos - enemyPos;
				if (std::abs(d.x) < touch && std::abs(d.y) < touch)
				{
					m_Context->PlayerHealth -= ENEMY_CONTACT_DAMAGE;
					m_Invuln = PLAYER_INVULN_TIME;
					break;
				}
			}
		}

		// Vacuum up nearby loot.
		for (LootScript* loot : GetScene().GetScriptsOfType<LootScript>())
		{
			const glm::vec2 lootPos = glm::vec2(loot->GetEntity().GetComponent<TransformComponent>().Position);
			if (glm::length(lootPos - playerPos) <= LOOT_PICKUP_DIST)
			{
				m_Context->LootCollected++;
				loot->GetEntity().Destroy();
			}
		}

		GetComponent<SpriteRendererComponent>().Color = (m_Invuln > 0.0f) ? COLOR_PLAYER_INV : COLOR_PLAYER;
	}

	// --- EnemyScript ------------------------------------------------------------

	void EnemyScript::OnUpdate(float deltaTime)
	{
		if (m_HitFlash > 0.0f)
			m_HitFlash -= deltaTime;

		if (m_Context->Player.IsValid())
		{
			auto& transform = GetComponent<TransformComponent>();
			const glm::vec2 enemyPos = glm::vec2(transform.Position);
			const glm::vec2 playerPos = glm::vec2(m_Context->Player.GetComponent<TransformComponent>().Position);

			const glm::vec2 toPlayer = playerPos - enemyPos;
			const float dist = glm::length(toPlayer);
			if (dist > 0.01f && dist < ENEMY_AGGRO_RANGE)
			{
				const glm::vec2 dir = toPlayer / dist;
				const glm::vec2 moved = m_Context->MoveWithCollision(enemyPos, dir * ENEMY_SPEED * deltaTime, ENEMY_HALF);
				transform.Position = glm::vec3(moved, transform.Position.z);
			}
		}

		GetComponent<SpriteRendererComponent>().Color = (m_HitFlash > 0.0f) ? COLOR_ENEMY_FLASH : COLOR_ENEMY;
	}

	void EnemyScript::Damage(float amount)
	{
		m_Health -= amount;
		m_HitFlash = ENEMY_HIT_FLASH;

		if (m_Health <= 0.0f)
		{
			m_Health = 0.0f;
			const glm::vec2 deathPos = glm::vec2(GetComponent<TransformComponent>().Position);
			SpawnLoot(GetScene(), *m_Context, deathPos); // drop loot where it died
			GetEntity().Destroy();
		}
	}

}
