#include "GameLayer.h"
#include "GameScripts.h"
#include "GameTuning.h"
#include "DungeonGenerator.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <format>
#include <string>

namespace Dingo
{

	void GameLayer::OnAttach()
	{
		m_Font = Font::Create("assets/fonts/arialbd.ttf");

		m_Scene = m_SceneManager.CreateScene("Dungeon");
		m_Scene->SetClearColor(COLOR_BG);
		m_SceneManager.SetActiveScene("Dungeon"); // selects the active scene
		SetupCamera();

		// ResetGame() generates the first dungeon and builds its tiles; every restart
		// rolls a fresh one. (It uses targeted destroys, not Clear(), so the camera
		// entity created above survives every restart.)
		ResetGame();

		m_Scene->OnStart(); // explicit start (no physics here -> just marks it running)
	}

	void GameLayer::OnDetach()
	{
		m_Scene->OnStop(); // explicit teardown

		if (m_Font)
		{
			m_Font->Destroy();
			m_Font = nullptr;
		}
	}

	// ------------------------------------------------------------------------
	// Dungeon generation
	// ------------------------------------------------------------------------

	void GameLayer::GenerateLevel()
	{
		// Roll a fresh random dungeon. The generator returns a '#'/'.' tile map plus
		// tile-space spawn points; we hand the map to the context (collision reads it)
		// and convert the spawns to world space via the same TileToWorld mapping the
		// tiles use.
		GeneratedDungeon dungeon = GenerateDungeon();
		m_Seed = dungeon.Seed;

		m_Context.Map = dungeon.Map;
		m_Context.MapWidth = dungeon.Width;
		m_Context.MapHeight = dungeon.Height;

		m_PlayerSpawn = m_Context.TileToWorld(dungeon.PlayerSpawn.x, dungeon.PlayerSpawn.y);

		m_EnemySpawns.clear();
		m_EnemySpawns.reserve(dungeon.EnemySpawns.size());
		for (const glm::ivec2& tile : dungeon.EnemySpawns)
			m_EnemySpawns.push_back(m_Context.TileToWorld(tile.x, tile.y));
	}

	void GameLayer::BuildTiles()
	{
		// Tear down the previous dungeon's tiles, then build one entity per cell.
		for (Entity& tile : m_TileEntities)
		{
			if (tile.IsValid())
				tile.Destroy();
		}
		m_TileEntities.clear();
		m_TileEntities.reserve((size_t)m_Context.MapWidth * (size_t)m_Context.MapHeight);

		for (int r = 0; r < m_Context.MapHeight; ++r)
		{
			for (int c = 0; c < m_Context.MapWidth; ++c)
			{
				const bool wall = m_Context.Map[r][c] == '#';
				const bool checker = ((c + r) & 1) != 0;
				const glm::vec4 color = wall
					? (checker ? COLOR_WALL_A : COLOR_WALL_B)
					: (checker ? COLOR_FLOOR_A : COLOR_FLOOR_B);

				Entity tile = m_Scene->CreateEntity(wall ? "Wall" : "Floor");
				auto& transform = tile.GetComponent<TransformComponent>();
				transform.Position = glm::vec3(m_Context.TileToWorld(c, r), 0.0f);
				transform.Size = glm::vec2(TILE_SIZE);
				tile.AddComponent<SpriteRendererComponent>(color);
				m_TileEntities.push_back(tile);
			}
		}
	}

	void GameLayer::ResetGame()
	{
		// Tear the actors down...
		if (m_Context.Player.IsValid())
			m_Context.Player.Destroy();
		for (EnemyScript* enemy : m_Scene->GetScriptsOfType<EnemyScript>())
			enemy->GetEntity().Destroy();
		for (LootScript* loot : m_Scene->GetScriptsOfType<LootScript>())
			loot->GetEntity().Destroy();

		// ...roll a fresh dungeon and rebuild its tiles...
		GenerateLevel();
		BuildTiles();

		// ...then reset state and spawn the actors into the new layout.
		m_Context.PlayerHealth = PLAYER_MAX_HEALTH;
		m_Context.LootCollected = 0;
		m_Context.State = GameContext::GameState::Playing;

		SpawnPlayer(*m_Scene, m_Context, m_PlayerSpawn);
		for (const glm::vec2& spawn : m_EnemySpawns)
			SpawnEnemy(*m_Scene, m_Context, spawn);
	}

	// ------------------------------------------------------------------------
	// Update
	// ------------------------------------------------------------------------

	void GameLayer::OnUpdate(float deltaTime)
	{
		if (Input::IsKeyPressed(Key::Escape))
			Application::Get().Close();

		if (m_Context.State == GameContext::GameState::Playing)
		{
			m_Scene->OnUpdate(deltaTime); // drives the player/enemy/loot scripts

			if (m_Context.PlayerHealth <= 0.0f)
			{
				m_Context.PlayerHealth = 0.0f;
				m_Context.State = GameContext::GameState::Dead;
			}
			else if (CountEnemies() == 0 && CountLoot() == 0)
			{
				// Room is cleared once every enemy is dead AND every dropped gem has
				// been picked up — so the last loot grab is what completes the room.
				m_Context.State = GameContext::GameState::Cleared;
			}
		}
		
		if (Input::IsKeyPressed(Key::R))
		{
			ResetGame();
		}

		UpdateCamera();

		// The world entities and the HUD/feedback overlay share the same camera, so
		// they render in a single scene block (one BeginScene/EndScene).
		Renderer2D& renderer = Application::Get().GetRenderer2D();
		const glm::vec2 viewport = renderer.GetViewportSize();
		const float aspect = (viewport.y > 0.0f) ? viewport.x / viewport.y : 1.0f;
		renderer.BeginScene(m_Scene->GetActiveCameraViewProjection(aspect));
		renderer.Clear(COLOR_BG);
		m_Scene->RenderEntities(renderer);
		RenderOverlay(renderer, deltaTime);
		renderer.EndScene();
	}

	void GameLayer::SetupCamera()
	{
		// An orthographic follow camera. The projection comes from OrthographicSize +
		// viewport aspect; the view comes from this entity's transform, which
		// UpdateCamera repositions each frame. Created once and kept across restarts
		// (ResetGame uses targeted destroys, never Clear()).
		m_CameraEntity = m_Scene->CreateEntity("Camera");
		auto& camera = m_CameraEntity.AddComponent<CameraComponent>();
		camera.Type = CameraComponent::ProjectionType::Orthographic;
		camera.OrthographicSize = ORTHO_SIZE;
		camera.OrthoNear = ORTHO_NEAR;
		camera.OrthoFar = ORTHO_FAR;
		camera.Primary = true;
	}

	void GameLayer::UpdateCamera()
	{
		const float aspect = (float)Application::Get().GetWindow().GetWidth() / (float)Application::Get().GetWindow().GetHeight();
		m_HalfH = ORTHO_SIZE * 0.5f;
		m_HalfW = m_HalfH * aspect;

		glm::vec2 playerPos = m_CameraPos;
		if (m_Context.Player.IsValid())
			playerPos = glm::vec2(m_Context.Player.GetComponent<TransformComponent>().Position);

		// World bounds covered by the tile quads.
		const float minX = -TILE_SIZE * 0.5f;
		const float maxX = (m_Context.MapWidth - 1) * TILE_SIZE + TILE_SIZE * 0.5f;
		const float maxY = TILE_SIZE * 0.5f;
		const float minY = -(m_Context.MapHeight - 1) * TILE_SIZE - TILE_SIZE * 0.5f;

		glm::vec2 target = playerPos;
		if (maxX - minX > 2.0f * m_HalfW)
			target.x = std::clamp(target.x, minX + m_HalfW, maxX - m_HalfW);
		else
			target.x = (minX + maxX) * 0.5f;

		if (maxY - minY > 2.0f * m_HalfH)
			target.y = std::clamp(target.y, minY + m_HalfH, maxY - m_HalfH);
		else
			target.y = (minY + maxY) * 0.5f;

		m_CameraPos = target;

		// Drive the camera entity's transform; GetActiveCameraViewProjection turns it
		// into the same view the old m_WorldVP encoded (ortho * translate(-cameraPos)).
		if (m_CameraEntity.IsValid())
			m_CameraEntity.GetComponent<TransformComponent>().Position = glm::vec3(m_CameraPos, 0.0f);
	}

	int GameLayer::CountEnemies()
	{
		return (int)m_Scene->GetScriptsOfType<EnemyScript>().size();
	}

	int GameLayer::CountLoot()
	{
		return (int)m_Scene->GetScriptsOfType<LootScript>().size();
	}

	// ------------------------------------------------------------------------
	// Overlay (feedback visuals + HUD) — drawn over the scene with the same camera
	// ------------------------------------------------------------------------

	void GameLayer::RenderOverlay(Renderer2D& r, float dt)
	{
		// Drawn inside the GameLayer's scene block (see OnUpdate), so no BeginScene here.

		// Player feedback (facing indicator + attack telegraph), read from the script.
		PlayerScript* player = m_Context.Player.IsValid() ? m_Context.Player.GetScript<PlayerScript>() : nullptr;
		if (player)
		{
			const glm::vec2 playerPos = glm::vec2(m_Context.Player.GetComponent<TransformComponent>().Position);

			const glm::vec2 tip = playerPos + player->Facing() * (PLAYER_HALF + 0.12f);
			r.DrawQuad(tip, glm::vec2(0.16f), COLOR_FACING);

			if (player->IsAttacking())
			{
				const glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(playerPos, 0.0f))
				                  * glm::scale(glm::mat4(1.0f), glm::vec3(PLAYER_ATTACK_RANGE * 2.0f));
				r.DrawCircle(t, COLOR_ATTACK_RING, 0.12f, 0.01f);
			}
		}

		// Enemy health bars, above damaged enemies.
		for (EnemyScript* enemy : m_Scene->GetScriptsOfType<EnemyScript>())
		{
			if (enemy->Health() >= enemy->MaxHealth())
				continue;
			const glm::vec2 enemyPos = glm::vec2(enemy->GetEntity().GetComponent<TransformComponent>().Position);
			DrawHealthBar(r, enemyPos + glm::vec2(0.0f, ENEMY_HALF + 0.22f), 0.8f, enemy->Health() / enemy->MaxHealth(), COLOR_ENEMY_BAR);
		}

		// HUD, positioned relative to the camera.
		const float left = m_CameraPos.x - m_HalfW;
		const float right = m_CameraPos.x + m_HalfW;
		const float top = m_CameraPos.y + m_HalfH;
		const float bottom = m_CameraPos.y - m_HalfH;
		const float pad = 0.4f;

		DrawHealthBar(r, glm::vec2(left + pad + 2.0f, top - pad - 0.2f), 4.0f, m_Context.PlayerHealth / PLAYER_MAX_HEALTH, COLOR_PLAYER, 0.34f);
		r.DrawText(std::format("HP  {}/{}", (int)m_Context.PlayerHealth, (int)PLAYER_MAX_HEALTH),
		           m_Font, glm::vec2(left + pad, top - pad - 0.66f), 0.32f, { COLOR_TEXT });
		r.DrawText(std::format("Enemies: {}", CountEnemies()),
		           m_Font, glm::vec2(left + pad, top - pad - 1.1f), 0.32f, { COLOR_TEXT });
		r.DrawText(std::format("Loot: {}", m_Context.LootCollected),
		           m_Font, glm::vec2(left + pad, top - pad - 1.5f), 0.32f, { COLOR_LOOT });
		r.DrawText(std::format("Seed: {}", m_Seed),
		           m_Font, glm::vec2(left + pad, top - pad - 1.9f), 0.26f, { COLOR_TEXT_DIM });

		r.DrawText("WASD move    SPACE attack    R new dungeon    ESC quit",
		           m_Font, glm::vec2(left + pad, bottom + pad), 0.28f, { COLOR_TEXT_DIM });
		r.DrawText(std::format("{:.0f} FPS", dt > 0.0f ? 1.0f / dt : 0.0f),
		           m_Font, glm::vec2(right - 2.2f, bottom + pad), 0.28f, { COLOR_TEXT_DIM });

		// Once the room is enemy-free but loot still litters the floor, nudge the
		// player toward the last gems (the room only clears once they're collected).
		if (m_Context.State == GameContext::GameState::Playing && CountEnemies() == 0 && CountLoot() > 0)
			DrawCenteredText(r, "Collect the remaining loot!", 0.5f, glm::vec2(0.0f, m_HalfH - 1.2f), COLOR_LOOT);

		// State banners.
		if (m_Context.State == GameContext::GameState::Dead)
		{
			DrawCenteredText(r, "YOU DIED", 1.4f, glm::vec2(0.0f, 0.6f), glm::vec4(0.90f, 0.30f, 0.25f, 1.0f));
			DrawCenteredText(r, "Press R to try again", 0.6f, glm::vec2(0.0f, -0.4f), COLOR_TEXT);
		}
		else if (m_Context.State == GameContext::GameState::Cleared)
		{
			DrawCenteredText(r, "DUNGEON CLEARED!", 1.4f, glm::vec2(0.0f, 0.7f), glm::vec4(1.0f, 0.85f, 0.30f, 1.0f));
			DrawCenteredText(r, std::format("Loot collected: {}", m_Context.LootCollected), 0.5f, glm::vec2(0.0f, -0.3f), COLOR_LOOT);
			DrawCenteredText(r, "Press R for a new dungeon", 0.6f, glm::vec2(0.0f, -1.1f), COLOR_TEXT);
		}
	}

	void GameLayer::DrawHealthBar(Renderer2D& r, const glm::vec2& center, float width, float frac, const glm::vec4& color, float height)
	{
		frac = std::clamp(frac, 0.0f, 1.0f);
		r.DrawQuad(center, glm::vec2(width, height), COLOR_BAR_BG);
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
