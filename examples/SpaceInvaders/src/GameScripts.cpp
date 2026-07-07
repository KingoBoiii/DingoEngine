#include "GameScripts.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <unordered_map>

namespace Dingo
{

	namespace
	{
		bool Overlaps(const TransformComponent& a, const TransformComponent& b)
		{
			const glm::vec2 ah = a.Size * 0.5f;
			const glm::vec2 bh = b.Size * 0.5f;
			return std::abs(a.Position.x - b.Position.x) < (ah.x + bh.x)
				&& std::abs(a.Position.y - b.Position.y) < (ah.y + bh.y);
		}
	}

	// --- Spawn helpers ----------------------------------------------------------

	Entity SpawnProjectile(Scene& scene, GameContext& context, const glm::vec2& position, const glm::vec2& velocity, bool fromPlayer)
	{
		Entity projectile = scene.CreateEntity(fromPlayer ? "PlayerBullet" : "Bomb");

		auto& transform = projectile.GetComponent<TransformComponent>();
		transform.Position = { position.x, position.y, 0.0f };
		transform.Size = fromPlayer ? PLAYER_BULLET_SIZE : BOMB_SIZE;

		projectile.AddComponent<SpriteRendererComponent>(fromPlayer ? COLOR_BULLET : COLOR_BOMB);
		projectile.AddScript<ProjectileScript>(&context, velocity, fromPlayer);
		return projectile;
	}

	void SpawnInvaderFormation(Scene& scene, GameContext& context)
	{
		(void)context;
		const float startX = -((INVADER_COLS - 1) * 0.5f) * INVADER_SPACING_X;

		for (int row = 0; row < INVADER_ROWS; row++)
		{
			for (int col = 0; col < INVADER_COLS; col++)
			{
				Entity invader = scene.CreateEntity("Invader");
				auto& transform = invader.GetComponent<TransformComponent>();
				transform.Position = { startX + col * INVADER_SPACING_X, INVADER_TOP_Y - row * INVADER_SPACING_Y, 0.0f };
				transform.Size = INVADER_SIZE;

				invader.AddComponent<SpriteRendererComponent>(INVADER_ROW_COLORS[row]);
				invader.AddScript<InvaderScript>(row, col);
			}
		}
	}

	// --- ProjectileScript -------------------------------------------------------

	void ProjectileScript::OnUpdate(float deltaTime)
	{
		// Move, and despawn once off-screen.
		{
			auto& transform = GetComponent<TransformComponent>();
			transform.Position.x += m_Velocity.x * deltaTime;
			transform.Position.y += m_Velocity.y * deltaTime;

			if (transform.Position.y > m_Context->HalfHeight + 1.0f || transform.Position.y < -m_Context->HalfHeight - 1.0f)
			{
				GetEntity().Destroy();
				return;
			}
		}

		// Copy our box before any Destroy() (deferred) — safe to read afterwards.
		const TransformComponent self = GetComponent<TransformComponent>();

		if (m_FromPlayer)
		{
			for (InvaderScript* invader : GetScene().GetScriptsOfType<InvaderScript>())
			{
				if (Overlaps(self, invader->GetEntity().GetComponent<TransformComponent>()))
				{
					m_Context->Score += invader->Points();
					invader->GetEntity().Destroy();
					GetEntity().Destroy();
					return;
				}
			}

			for (ShieldScript* shield : GetScene().GetScriptsOfType<ShieldScript>())
			{
				if (Overlaps(self, shield->GetEntity().GetComponent<TransformComponent>()))
				{
					shield->GetEntity().Destroy();
					GetEntity().Destroy();
					return;
				}
			}
		}
		else
		{
			for (ShieldScript* shield : GetScene().GetScriptsOfType<ShieldScript>())
			{
				if (Overlaps(self, shield->GetEntity().GetComponent<TransformComponent>()))
				{
					shield->GetEntity().Destroy();
					GetEntity().Destroy();
					return;
				}
			}

			if (m_Context->Player.IsValid() && Overlaps(self, m_Context->Player.GetComponent<TransformComponent>()))
			{
				m_Context->Lives--;
				if (m_Context->Lives <= 0)
				{
					m_Context->GameOver = true;
					RequestSceneTransition("GameOver");
				}

				GetEntity().Destroy();
				return;
			}
		}
	}

	// --- PlayerScript -----------------------------------------------------------

	void PlayerScript::OnUpdate(float deltaTime)
	{
		float direction = 0.0f;
		if (Input::IsKeyDown(Key::A) || Input::IsKeyDown(Key::Left))  direction -= 1.0f;
		if (Input::IsKeyDown(Key::D) || Input::IsKeyDown(Key::Right)) direction += 1.0f;

		float spawnX = 0.0f;
		float spawnY = 0.0f;
		{
			auto& transform = GetComponent<TransformComponent>();
			transform.Position.x += direction * PLAYER_SPEED * deltaTime;
			const float limit = m_Context->HalfWidth - PLAYER_SIZE.x * 0.5f;
			transform.Position.x = glm::clamp(transform.Position.x, -limit, limit);

			spawnX = transform.Position.x;
			spawnY = transform.Position.y + PLAYER_SIZE.y * 0.5f + PLAYER_BULLET_SIZE.y * 0.5f;
		}

		if (m_FireCooldown > 0.0f)
			m_FireCooldown -= deltaTime;

		if (Input::IsKeyPressed(Key::Space) && m_FireCooldown <= 0.0f)
		{
			// Classic rule: only one player bullet in flight at a time.
			int playerBullets = 0;
			for (ProjectileScript* p : GetScene().GetScriptsOfType<ProjectileScript>())
				if (p->IsFromPlayer())
					playerBullets++;

			if (playerBullets == 0)
			{
				SpawnProjectile(GetScene(), *m_Context, { spawnX, spawnY }, { 0.0f, PLAYER_BULLET_SPEED }, true);
				m_FireCooldown = PLAYER_FIRE_COOLDOWN;
			}
		}
	}

	// --- FormationControllerScript ----------------------------------------------

	void FormationControllerScript::OnCreate()
	{
		ResetForWave();
	}

	void FormationControllerScript::ResetForWave()
	{
		m_Direction = 1.0f;
		m_Speed = INVADER_BASE_SPEED + (m_Context->Wave - 1) * 0.6f;
		m_FireTimer = INVADER_FIRE_INTERVAL;
	}

	void FormationControllerScript::OnUpdate(float deltaTime)
	{
		std::vector<InvaderScript*> invaders = GetScene().GetScriptsOfType<InvaderScript>();

		if (invaders.empty())
		{
			m_Context->Wave++;
			ResetForWave();
			SpawnInvaderFormation(GetScene(), *m_Context);
			return;
		}

		// March the formation horizontally; track extents.
		const float step = m_Direction * m_Speed * deltaTime;
		float minX = std::numeric_limits<float>::max();
		float maxX = std::numeric_limits<float>::lowest();
		float lowestY = std::numeric_limits<float>::max();

		for (InvaderScript* invader : invaders)
		{
			auto& transform = invader->GetEntity().GetComponent<TransformComponent>();
			transform.Position.x += step;
			minX = std::min(minX, transform.Position.x);
			maxX = std::max(maxX, transform.Position.x);
			lowestY = std::min(lowestY, transform.Position.y);
		}

		const float limit = m_Context->HalfWidth - INVADER_SIZE.x * 0.5f - 0.3f;
		const bool hitEdge = (m_Direction > 0.0f && maxX >= limit) || (m_Direction < 0.0f && minX <= -limit);
		if (hitEdge)
		{
			m_Direction = -m_Direction;
			for (InvaderScript* invader : invaders)
				invader->GetEntity().GetComponent<TransformComponent>().Position.y -= INVADER_DROP;
			lowestY -= INVADER_DROP;
		}

		if (lowestY <= INVASION_Y && !m_Context->GameOver)
		{
			m_Context->GameOver = true;
			RequestSceneTransition("GameOver");
		}

		// Periodically drop a bomb from the lowest invader in a random column.
		m_FireTimer -= deltaTime;
		if (m_FireTimer <= 0.0f)
		{
			m_FireTimer = std::max(0.35f, INVADER_FIRE_INTERVAL - (m_Context->Wave - 1) * 0.12f);

			std::unordered_map<int, InvaderScript*> bottomByColumn;
			std::unordered_map<int, float> bottomY;
			for (InvaderScript* invader : invaders)
			{
				const int col = invader->Column();
				const float y = invader->GetEntity().GetComponent<TransformComponent>().Position.y;
				auto it = bottomY.find(col);
				if (it == bottomY.end() || y < it->second)
				{
					bottomY[col] = y;
					bottomByColumn[col] = invader;
				}
			}

			if (!bottomByColumn.empty())
			{
				auto it = bottomByColumn.begin();
				std::advance(it, rand() % (int)bottomByColumn.size());
				const glm::vec3 origin = it->second->GetEntity().GetComponent<TransformComponent>().Position;
				SpawnProjectile(GetScene(), *m_Context,
					{ origin.x, origin.y - INVADER_SIZE.y * 0.5f - BOMB_SIZE.y * 0.5f },
					{ 0.0f, -BOMB_SPEED }, false);
			}
		}
	}

}
