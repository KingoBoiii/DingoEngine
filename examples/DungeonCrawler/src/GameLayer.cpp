#define NOMINMAX
#include "GameLayer.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <format>

namespace Dingo
{

	// Hand-crafted room. '#' = wall, '.' = floor, 'P' = player spawn, 'E' = enemy spawn.
	// Rows are normalized to equal width and sealed with a wall border at load time,
	// so a small miscount here cannot break tile indexing or leave the room open.
	static const char* s_RoomLayout[] = {
		"########################",
		"#..........E...........#",
		"#..####.........####...#",
		"#..#..............#....#",
		"#..#....P.........#....#",
		"#.................#....#",
		"#....##......##........#",
		"#....##......##....E...#",
		"#.....................##",
		"#..E....####...........#",
		"#.......#..#...........#",
		"#.......#..#....##.....#",
		"#..............##......#",
		"#..........E...........#",
		"#......................#",
		"########################",
	};

	void GameLayer::OnAttach()
	{
		LoadRoom();
		m_Font = Font::Create("assets/fonts/arialbd.ttf");
		ResetGame();
	}

	void GameLayer::OnDetach()
	{
		if (m_Font)
		{
			m_Font->Destroy();
			m_Font = nullptr;
		}
	}

	void GameLayer::LoadRoom()
	{
		m_Map.clear();
		m_EnemySpawns.clear();

		const size_t rows = sizeof(s_RoomLayout) / sizeof(s_RoomLayout[0]);
		size_t width = 0;
		for (size_t i = 0; i < rows; ++i)
			width = std::max(width, std::string(s_RoomLayout[i]).size());

		for (size_t i = 0; i < rows; ++i)
		{
			std::string row = s_RoomLayout[i];
			row.resize(width, '.'); // pad short rows with floor
			m_Map.push_back(row);
		}

		m_MapHeight = (int)m_Map.size();
		m_MapWidth = (int)width;

		// Seal the border so the room is always closed.
		for (int c = 0; c < m_MapWidth; ++c)
		{
			m_Map[0][c] = '#';
			m_Map[m_MapHeight - 1][c] = '#';
		}
		for (int r = 0; r < m_MapHeight; ++r)
		{
			m_Map[r][0] = '#';
			m_Map[r][m_MapWidth - 1] = '#';
		}

		// Extract spawns, then turn the markers into plain floor.
		for (int r = 0; r < m_MapHeight; ++r)
		{
			for (int c = 0; c < m_MapWidth; ++c)
			{
				char& cell = m_Map[r][c];
				if (cell == 'P')
				{
					m_PlayerSpawn = TileToWorld(c, r);
					cell = '.';
				}
				else if (cell == 'E')
				{
					m_EnemySpawns.push_back(TileToWorld(c, r));
					cell = '.';
				}
			}
		}
	}

	void GameLayer::ResetGame()
	{
		m_PlayerPos = m_PlayerSpawn;
		m_PlayerFacing = glm::vec2(0.0f, -1.0f);
		m_PlayerHealth = m_PlayerMaxHealth;
		m_PlayerInvuln = 0.0f;
		m_AttackTimer = 0.0f;
		m_AttackCooldown = 0.0f;

		m_Enemies.clear();
		for (const glm::vec2& spawn : m_EnemySpawns)
		{
			Enemy e;
			e.Position = spawn;
			e.Health = e.MaxHealth;
			e.HitFlash = 0.0f;
			e.Alive = true;
			m_Enemies.push_back(e);
		}

		m_State = PlayState::Playing;
	}

	// ------------------------------------------------------------------------
	// Tile / collision helpers
	// ------------------------------------------------------------------------

	glm::vec2 GameLayer::TileToWorld(int col, int row) const
	{
		return glm::vec2(col * m_TileSize, -row * m_TileSize);
	}

	bool GameLayer::IsSolidTile(int col, int row) const
	{
		if (col < 0 || col >= m_MapWidth || row < 0 || row >= m_MapHeight)
			return true;
		return m_Map[row][col] == '#';
	}

	bool GameLayer::IsSolidWorld(float x, float y) const
	{
		const int col = (int)std::floor(x / m_TileSize + 0.5f);
		const int row = (int)std::floor(-y / m_TileSize + 0.5f);
		return IsSolidTile(col, row);
	}

	// Axis-separated AABB-vs-tile sweep. Valid while half-size < one tile.
	glm::vec2 GameLayer::MoveWithCollision(const glm::vec2& pos, const glm::vec2& delta, float half) const
	{
		const float eps = 0.001f;
		glm::vec2 result = pos;

		if (delta.x != 0.0f)
		{
			const float nx = result.x + delta.x;
			const float edgeX = nx + (delta.x > 0.0f ? half : -half);
			if (!IsSolidWorld(edgeX, result.y - half + eps) &&
			    !IsSolidWorld(edgeX, result.y + half - eps))
			{
				result.x = nx;
			}
		}

		if (delta.y != 0.0f)
		{
			const float ny = result.y + delta.y;
			const float edgeY = ny + (delta.y > 0.0f ? half : -half);
			if (!IsSolidWorld(result.x - half + eps, edgeY) &&
			    !IsSolidWorld(result.x + half - eps, edgeY))
			{
				result.y = ny;
			}
		}

		return result;
	}

	// ------------------------------------------------------------------------
	// Update
	// ------------------------------------------------------------------------

	void GameLayer::OnUpdate(float deltaTime)
	{
		// NOTE: In DingoEngine, IsKeyDown == edge-triggered (just pressed),
		// IsKeyPressed == held. (Inverted from the usual convention.)
		if (Input::IsKeyDown(Key::Escape))
			Application::Get().Close();

		if (m_State == PlayState::Playing)
		{
			UpdatePlayer(deltaTime);
			UpdateCombat(deltaTime);
			UpdateEnemies(deltaTime);

			if (m_PlayerHealth <= 0.0f)
			{
				m_PlayerHealth = 0.0f;
				m_State = PlayState::Dead;
			}
			else
			{
				bool anyAlive = false;
				for (const Enemy& e : m_Enemies)
					anyAlive = anyAlive || e.Alive;
				if (!anyAlive)
					m_State = PlayState::Cleared;
			}
		}
		else if (Input::IsKeyDown(Key::R))
		{
			ResetGame();
		}

		UpdateCamera();

		Renderer2D& renderer = Application::Get().GetRenderer2D();

		const glm::mat4 projection = glm::ortho(-m_HalfW, m_HalfW, -m_HalfH, m_HalfH, m_OrthoNear, m_OrthoFar);
		const glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(-m_CameraPos.x, -m_CameraPos.y, 0.0f));

		renderer.BeginScene(projection * view);
		renderer.Clear(glm::vec4(0.04f, 0.04f, 0.06f, 1.0f));

		RenderRoom(renderer);
		RenderEntities(renderer);
		RenderHud(renderer, deltaTime);

		renderer.EndScene();
	}

	void GameLayer::UpdateCamera()
	{
		const float aspect = (float)Application::Get().GetWindow().GetWidth() / (float)Application::Get().GetWindow().GetHeight();
		m_HalfH = m_OrthoSize * 0.5f;
		m_HalfW = m_HalfH * aspect;

		// World bounds covered by the tile quads.
		const float minX = -m_TileSize * 0.5f;
		const float maxX = (m_MapWidth - 1) * m_TileSize + m_TileSize * 0.5f;
		const float maxY = m_TileSize * 0.5f;
		const float minY = -(m_MapHeight - 1) * m_TileSize - m_TileSize * 0.5f;

		glm::vec2 target = m_PlayerPos;

		if (maxX - minX > 2.0f * m_HalfW)
			target.x = std::clamp(target.x, minX + m_HalfW, maxX - m_HalfW);
		else
			target.x = (minX + maxX) * 0.5f;

		if (maxY - minY > 2.0f * m_HalfH)
			target.y = std::clamp(target.y, minY + m_HalfH, maxY - m_HalfH);
		else
			target.y = (minY + maxY) * 0.5f;

		m_CameraPos = target;
	}

	void GameLayer::UpdatePlayer(float dt)
	{
		glm::vec2 dir(0.0f);
		if (Input::IsKeyPressed(Key::W)) dir.y += 1.0f;
		if (Input::IsKeyPressed(Key::S)) dir.y -= 1.0f;
		if (Input::IsKeyPressed(Key::A)) dir.x -= 1.0f;
		if (Input::IsKeyPressed(Key::D)) dir.x += 1.0f;

		if (dir.x != 0.0f || dir.y != 0.0f)
		{
			dir = glm::normalize(dir);
			m_PlayerFacing = dir;
			m_PlayerPos = MoveWithCollision(m_PlayerPos, dir * m_PlayerSpeed * dt, m_PlayerHalf);
		}

		if (m_PlayerInvuln > 0.0f)
			m_PlayerInvuln -= dt;
	}

	void GameLayer::UpdateCombat(float dt)
	{
		if (m_AttackTimer > 0.0f) m_AttackTimer -= dt;
		if (m_AttackCooldown > 0.0f) m_AttackCooldown -= dt;

		if (Input::IsKeyDown(Key::Space) && m_AttackCooldown <= 0.0f)
		{
			m_AttackTimer = m_PlayerAttackActive;
			m_AttackCooldown = m_PlayerAttackCooldownTime;

			// Radial swing: hit every alive enemy within range.
			for (Enemy& e : m_Enemies)
			{
				if (!e.Alive) continue;
				const float dist = glm::length(e.Position - m_PlayerPos);
				if (dist <= m_PlayerAttackRange + m_EnemyHalf)
				{
					e.Health -= m_PlayerAttackDamage;
					e.HitFlash = 0.12f;
					if (e.Health <= 0.0f)
					{
						e.Health = 0.0f;
						e.Alive = false;
					}
				}
			}
		}
	}

	void GameLayer::UpdateEnemies(float dt)
	{
		const float touch = m_EnemyHalf + m_PlayerHalf;
		for (Enemy& e : m_Enemies)
		{
			if (e.HitFlash > 0.0f) e.HitFlash -= dt;
			if (!e.Alive) continue;

			const glm::vec2 toPlayer = m_PlayerPos - e.Position;
			const float dist = glm::length(toPlayer);
			if (dist > 0.01f && dist < m_EnemyAggroRange)
			{
				const glm::vec2 dir = toPlayer / dist;
				e.Position = MoveWithCollision(e.Position, dir * m_EnemySpeed * dt, m_EnemyHalf);
			}

			// Contact damage, gated by the player's invulnerability window.
			const glm::vec2 d = m_PlayerPos - e.Position;
			if (std::abs(d.x) < touch && std::abs(d.y) < touch && m_PlayerInvuln <= 0.0f)
			{
				m_PlayerHealth -= m_EnemyContactDamage;
				m_PlayerInvuln = m_PlayerInvulnTime;
			}
		}
	}

	// ------------------------------------------------------------------------
	// Render
	// ------------------------------------------------------------------------

	void GameLayer::RenderRoom(Renderer2D& r)
	{
		// Cull to the visible tile range (+1 tile margin).
		const int colMin = std::max(0, (int)std::floor((m_CameraPos.x - m_HalfW) / m_TileSize) - 1);
		const int colMax = std::min(m_MapWidth - 1, (int)std::ceil((m_CameraPos.x + m_HalfW) / m_TileSize) + 1);
		const int rowMin = std::max(0, (int)std::floor((-(m_CameraPos.y + m_HalfH)) / m_TileSize) - 1);
		const int rowMax = std::min(m_MapHeight - 1, (int)std::ceil((-(m_CameraPos.y - m_HalfH)) / m_TileSize) + 1);

		for (int row = rowMin; row <= rowMax; ++row)
		{
			for (int col = colMin; col <= colMax; ++col)
			{
				const glm::vec2 pos = TileToWorld(col, row);
				const bool checker = ((col + row) & 1) != 0;
				if (m_Map[row][col] == '#')
				{
					const glm::vec4 c = checker ? glm::vec4(0.34f, 0.36f, 0.46f, 1.0f) : glm::vec4(0.30f, 0.32f, 0.41f, 1.0f);
					r.DrawQuad(pos, glm::vec2(m_TileSize), c);
				}
				else
				{
					const glm::vec4 c = checker ? glm::vec4(0.15f, 0.15f, 0.20f, 1.0f) : glm::vec4(0.12f, 0.12f, 0.16f, 1.0f);
					r.DrawQuad(pos, glm::vec2(m_TileSize), c);
				}
			}
		}
	}

	void GameLayer::RenderEntities(Renderer2D& r)
	{
		for (const Enemy& e : m_Enemies)
		{
			if (!e.Alive) continue;
			glm::vec4 color = (e.HitFlash > 0.0f)
				? glm::vec4(1.0f, 0.85f, 0.85f, 1.0f)
				: glm::vec4(0.85f, 0.27f, 0.27f, 1.0f);
			r.DrawQuad(e.Position, glm::vec2(m_EnemyHalf * 2.0f), color);

			if (e.Health < e.MaxHealth)
			{
				const glm::vec2 barCenter = e.Position + glm::vec2(0.0f, m_EnemyHalf + 0.22f);
				DrawHealthBar(r, barCenter, 0.8f, e.Health / e.MaxHealth, glm::vec4(0.85f, 0.30f, 0.30f, 1.0f));
			}
		}

		// Player (flashes brighter while invulnerable).
		const glm::vec4 playerColor = (m_PlayerInvuln > 0.0f)
			? glm::vec4(0.75f, 1.0f, 0.80f, 1.0f)
			: glm::vec4(0.30f, 0.80f, 0.42f, 1.0f);
		r.DrawQuad(m_PlayerPos, glm::vec2(m_PlayerHalf * 2.0f), playerColor);

		// Facing indicator.
		const glm::vec2 tip = m_PlayerPos + m_PlayerFacing * (m_PlayerHalf + 0.12f);
		r.DrawQuad(tip, glm::vec2(0.16f), glm::vec4(0.92f, 0.96f, 1.0f, 1.0f));

		// Attack telegraph ring.
		if (m_AttackTimer > 0.0f)
		{
			const glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(m_PlayerPos, 0.0f))
			                  * glm::scale(glm::mat4(1.0f), glm::vec3(m_PlayerAttackRange * 2.0f));
			r.DrawCircle(t, glm::vec4(1.0f, 0.92f, 0.45f, 1.0f), 0.12f, 0.01f);
		}
	}

	void GameLayer::RenderHud(Renderer2D& r, float dt)
	{
		const glm::vec4 textColor(0.95f, 0.95f, 0.97f, 1.0f);
		const glm::vec4 dimColor(0.70f, 0.70f, 0.75f, 1.0f);

		// Screen corners expressed in world space (camera-relative HUD).
		const float left = m_CameraPos.x - m_HalfW;
		const float right = m_CameraPos.x + m_HalfW;
		const float top = m_CameraPos.y + m_HalfH;
		const float bottom = m_CameraPos.y - m_HalfH;
		const float pad = 0.4f;

		// Player health bar + readout (top-left).
		const glm::vec2 barCenter(left + pad + 2.0f, top - pad - 0.2f);
		DrawHealthBar(r, barCenter, 4.0f, m_PlayerHealth / m_PlayerMaxHealth, glm::vec4(0.30f, 0.80f, 0.42f, 1.0f), 0.34f);
		r.DrawText(std::format("HP  {}/{}", (int)m_PlayerHealth, (int)m_PlayerMaxHealth),
		           m_Font, glm::vec2(left + pad, top - pad - 0.66f), 0.32f, { textColor });

		int alive = 0;
		for (const Enemy& e : m_Enemies) if (e.Alive) ++alive;
		r.DrawText(std::format("Enemies: {}", alive),
		           m_Font, glm::vec2(left + pad, top - pad - 1.1f), 0.32f, { textColor });

		// Controls + perf (bottom).
		r.DrawText("WASD move    SPACE attack    R restart    ESC quit",
		           m_Font, glm::vec2(left + pad, bottom + pad), 0.28f, { dimColor });
		r.DrawText(std::format("{:.0f} FPS", dt > 0.0f ? 1.0f / dt : 0.0f),
		           m_Font, glm::vec2(right - 2.2f, bottom + pad), 0.28f, { dimColor });

		// State banners.
		if (m_State == PlayState::Dead)
		{
			DrawCenteredText(r, "YOU DIED", 1.4f, glm::vec2(0.0f, 0.6f), glm::vec4(0.90f, 0.30f, 0.25f, 1.0f));
			DrawCenteredText(r, "Press R to try again", 0.6f, glm::vec2(0.0f, -0.4f), textColor);
		}
		else if (m_State == PlayState::Cleared)
		{
			DrawCenteredText(r, "ROOM CLEARED!", 1.4f, glm::vec2(0.0f, 0.6f), glm::vec4(1.0f, 0.85f, 0.30f, 1.0f));
			DrawCenteredText(r, "Press R to play again", 0.6f, glm::vec2(0.0f, -0.4f), textColor);
		}
	}

	void GameLayer::DrawHealthBar(Renderer2D& r, const glm::vec2& center, float width, float frac, const glm::vec4& color, float height)
	{
		frac = std::clamp(frac, 0.0f, 1.0f);
		r.DrawQuad(center, glm::vec2(width, height), glm::vec4(0.05f, 0.05f, 0.06f, 1.0f));
		const float fw = width * frac;
		if (fw > 0.0f)
		{
			const glm::vec2 fcenter(center.x - width * 0.5f + fw * 0.5f, center.y);
			r.DrawQuad(fcenter, glm::vec2(fw, height * 0.7f), color);
		}
	}

	void GameLayer::DrawCenteredText(Renderer2D& r, const std::string& text, float size, const glm::vec2& offset, const glm::vec4& color)
	{
		const float w = m_Font->GetStringWidth(text, size);
		const glm::vec2 p(m_CameraPos.x - w * 0.5f + offset.x, m_CameraPos.y + offset.y);
		r.DrawText(text, m_Font, p, size, { color });
	}

}
