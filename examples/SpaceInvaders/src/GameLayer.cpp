#include "GameLayer.h"

#include <entt/entt.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <format>
#include <iterator>
#include <limits>
#include <unordered_map>
#include <vector>

namespace Dingo
{

	namespace
	{
		// --- Playfield ---
		constexpr float ORTHO_HEIGHT = 20.0f;

		// --- Player ---
		constexpr float PLAYER_Y = -8.4f;
		constexpr float PLAYER_SPEED = 13.0f;
		constexpr float PLAYER_FIRE_COOLDOWN = 0.35f;
		const glm::vec2 PLAYER_SIZE = { 1.7f, 0.7f };

		// --- Player bullet ---
		constexpr float PLAYER_BULLET_SPEED = 24.0f;
		const glm::vec2 PLAYER_BULLET_SIZE = { 0.18f, 0.8f };

		// --- Invaders ---
		constexpr int INVADER_ROWS = 5;
		constexpr int INVADER_COLS = 11;
		constexpr float INVADER_SPACING_X = 2.3f;
		constexpr float INVADER_SPACING_Y = 1.7f;
		constexpr float INVADER_TOP_Y = 8.2f;
		constexpr float INVADER_BASE_SPEED = 2.2f;
		constexpr float INVADER_DROP = 0.6f;
		constexpr float INVASION_Y = -6.4f; // invaders reaching this line = defeat
		const glm::vec2 INVADER_SIZE = { 1.3f, 0.95f };

		// --- Invader bombs ---
		constexpr float BOMB_SPEED = 9.0f;
		constexpr float INVADER_FIRE_INTERVAL = 1.1f;
		const glm::vec2 BOMB_SIZE = { 0.22f, 0.7f };

		// --- Shields ---
		constexpr int SHIELD_COUNT = 4;
		constexpr int SHIELD_COLS = 7;
		constexpr int SHIELD_ROWS = 3;
		constexpr float SHIELD_Y = -5.0f;
		constexpr float SHIELD_BLOCK = 0.5f;

		// --- Colours ---
		const glm::vec4 COLOR_BG     = { 0.02f, 0.02f, 0.06f, 1.0f };
		const glm::vec4 COLOR_PLAYER = { 0.35f, 0.95f, 0.45f, 1.0f };
		const glm::vec4 COLOR_BULLET = { 1.00f, 0.95f, 0.40f, 1.0f };
		const glm::vec4 COLOR_BOMB   = { 1.00f, 0.45f, 0.35f, 1.0f };
		const glm::vec4 COLOR_SHIELD = { 0.30f, 0.85f, 0.40f, 1.0f };
		const glm::vec4 COLOR_TEXT   = { 0.90f, 0.90f, 0.95f, 1.0f };

		// Top rows are worth more points and get a distinct colour.
		const glm::vec4 INVADER_ROW_COLORS[INVADER_ROWS] = {
			{ 0.95f, 0.40f, 0.85f, 1.0f }, // row 0 (top)
			{ 0.60f, 0.65f, 1.00f, 1.0f },
			{ 0.60f, 0.65f, 1.00f, 1.0f },
			{ 0.45f, 0.90f, 0.95f, 1.0f },
			{ 0.45f, 0.90f, 0.95f, 1.0f },
		};

		// --- Font sizes (world units) ---
		constexpr float FONT_TITLE = 2.4f;
		constexpr float FONT_SUB = 1.0f;
		constexpr float FONT_HUD = 0.7f;
		constexpr float FONT_SMALL = 0.42f;

		bool Overlaps(const TransformComponent& a, const TransformComponent& b)
		{
			const glm::vec2 ah = a.Size * 0.5f;
			const glm::vec2 bh = b.Size * 0.5f;
			return std::abs(a.Position.x - b.Position.x) < (ah.x + bh.x)
				&& std::abs(a.Position.y - b.Position.y) < (ah.y + bh.y);
		}

		TextComponent MakeText(const std::string& text, Font* font, float size, const glm::vec4& color, bool centered)
		{
			TextComponent t;
			t.Text = text;
			t.Font = font;
			t.Size = size;
			t.Color = color;
			t.Centered = centered;
			return t;
		}
	}

	void GameLayer::OnAttach()
	{
		const float aspect = (float)Application::Get().GetWindow().GetWidth() / (float)Application::Get().GetWindow().GetHeight();
		m_HalfHeight = ORTHO_HEIGHT * 0.5f;
		m_HalfWidth = m_HalfHeight * aspect;
		m_ViewProjection = glm::ortho(-m_HalfWidth, m_HalfWidth, -m_HalfHeight, m_HalfHeight, -1.0f, 1.0f);

		m_Font = Font::Create("assets/fonts/arialbd.ttf");

		m_MenuScene = m_SceneManager.CreateScene("Menu");
		m_GameScene = m_SceneManager.CreateScene("Game");
		m_GameOverScene = m_SceneManager.CreateScene("GameOver");

		for (Scene* scene : { m_MenuScene, m_GameScene, m_GameOverScene })
		{
			scene->SetViewProjection(m_ViewProjection);
			scene->SetClearColor(COLOR_BG);
		}

		BuildMenuScene();
		BuildGameOverScene();

		m_SceneManager.SetActiveScene("Menu");
	}

	void GameLayer::OnDetach()
	{
		// Scenes are owned by m_SceneManager and destroyed with it.
		if (m_Font)
		{
			m_Font->Destroy();
			m_Font = nullptr;
		}
	}

	void GameLayer::OnUpdate(float deltaTime)
	{
		if (Input::IsKeyDown(Key::Escape))
			Application::Get().Close();

		const std::string active = m_SceneManager.GetActiveSceneName();

		if (active == "Menu")
		{
			if (Input::IsKeyDown(Key::Space) || Input::IsKeyDown(Key::Enter))
				StartGame();
		}
		else if (active == "Game")
		{
			UpdatePlayer(deltaTime);
			UpdateFormation(deltaTime);
			UpdateFiring(deltaTime);
			UpdateBullets(deltaTime);
			UpdateCollisions();
			UpdateHud();

			if (m_Defeat)
				EndGame();
			else if (CountInvaders() == 0)
			{
				m_Wave++;
				SpawnInvaderFormation();
			}
		}
		else if (active == "GameOver")
		{
			if (Input::IsKeyDown(Key::Space) || Input::IsKeyDown(Key::Enter))
				m_SceneManager.SetActiveScene("Menu");
		}

		m_SceneManager.OnRender(Application::Get().GetRenderer2D());
	}

	// ----------------------------------------------------------------------
	// Scene construction
	// ----------------------------------------------------------------------

	void GameLayer::BuildMenuScene()
	{
		m_MenuScene->Clear();

		Entity title = m_MenuScene->CreateEntity("Title");
		title.GetComponent<TransformComponent>().Position = { 0.0f, 4.0f, 0.0f };
		title.AddComponent<TextComponent>(MakeText("SPACE INVADERS", m_Font, FONT_TITLE, { 0.45f, 0.95f, 0.55f, 1.0f }, true));

		Entity prompt = m_MenuScene->CreateEntity("Prompt");
		prompt.GetComponent<TransformComponent>().Position = { 0.0f, 0.5f, 0.0f };
		prompt.AddComponent<TextComponent>(MakeText("Press SPACE to play", m_Font, FONT_SUB, COLOR_TEXT, true));

		Entity controls = m_MenuScene->CreateEntity("Controls");
		controls.GetComponent<TransformComponent>().Position = { 0.0f, -1.6f, 0.0f };
		controls.AddComponent<TextComponent>(MakeText("Move: A / D  or  Left / Right       Shoot: SPACE", m_Font, FONT_SMALL, COLOR_TEXT, true));

		Entity footer = m_MenuScene->CreateEntity("Footer");
		footer.GetComponent<TransformComponent>().Position = { -m_HalfWidth + 0.4f, -m_HalfHeight + 0.4f, 0.0f };
		footer.AddComponent<TextComponent>(MakeText("Space Invaders - a Dingo Engine Scene/ECS example", m_Font, FONT_SMALL, { 0.5f, 0.5f, 0.6f, 1.0f }, false));
	}

	void GameLayer::BuildGameOverScene()
	{
		m_GameOverScene->Clear();

		Entity title = m_GameOverScene->CreateEntity("GameOverTitle");
		title.GetComponent<TransformComponent>().Position = { 0.0f, 3.5f, 0.0f };
		title.AddComponent<TextComponent>(MakeText("GAME OVER", m_Font, FONT_TITLE, { 0.95f, 0.35f, 0.30f, 1.0f }, true));

		m_GameOverScoreText = m_GameOverScene->CreateEntity("FinalScore");
		m_GameOverScoreText.GetComponent<TransformComponent>().Position = { 0.0f, 0.5f, 0.0f };
		m_GameOverScoreText.AddComponent<TextComponent>(MakeText("SCORE 0", m_Font, FONT_SUB, COLOR_TEXT, true));

		Entity prompt = m_GameOverScene->CreateEntity("Prompt");
		prompt.GetComponent<TransformComponent>().Position = { 0.0f, -2.0f, 0.0f };
		prompt.AddComponent<TextComponent>(MakeText("Press SPACE to return to the menu", m_Font, FONT_SMALL, COLOR_TEXT, true));
	}

	void GameLayer::StartGame()
	{
		m_Score = 0;
		m_Lives = 3;
		m_Wave = 1;
		m_Defeat = false;
		m_PlayerFireCooldown = 0.0f;
		m_InvaderFireTimer = INVADER_FIRE_INTERVAL;

		m_GameScene->Clear();
		BuildHud();
		SpawnPlayer();
		SpawnShields();
		SpawnInvaderFormation();

		m_SceneManager.SetActiveScene("Game");
	}

	void GameLayer::BuildHud()
	{
		m_ScoreText = m_GameScene->CreateEntity("ScoreHud");
		m_ScoreText.GetComponent<TransformComponent>().Position = { -m_HalfWidth + 0.5f, m_HalfHeight - 1.1f, 0.0f };
		m_ScoreText.AddComponent<TextComponent>(MakeText("SCORE 0", m_Font, FONT_HUD, COLOR_TEXT, false));

		m_WaveText = m_GameScene->CreateEntity("WaveHud");
		m_WaveText.GetComponent<TransformComponent>().Position = { 0.0f, m_HalfHeight - 1.1f, 0.0f };
		m_WaveText.AddComponent<TextComponent>(MakeText("WAVE 1", m_Font, FONT_HUD, COLOR_TEXT, true));

		// Right-aligned each frame in UpdateHud.
		m_LivesText = m_GameScene->CreateEntity("LivesHud");
		m_LivesText.GetComponent<TransformComponent>().Position = { m_HalfWidth - 0.5f, m_HalfHeight - 1.1f, 0.0f };
		m_LivesText.AddComponent<TextComponent>(MakeText("LIVES 3", m_Font, FONT_HUD, COLOR_TEXT, false));
	}

	void GameLayer::SpawnPlayer()
	{
		m_Player = m_GameScene->CreateEntity("Player");
		auto& transform = m_Player.GetComponent<TransformComponent>();
		transform.Position = { 0.0f, PLAYER_Y, 0.0f };
		transform.Size = PLAYER_SIZE;
		m_Player.AddComponent<SpriteRendererComponent>(COLOR_PLAYER);
		m_Player.AddComponent<PlayerTag>();
	}

	void GameLayer::SpawnShields()
	{
		const float spacing = (2.0f * m_HalfWidth) / (SHIELD_COUNT + 1);
		for (int i = 0; i < SHIELD_COUNT; i++)
		{
			const float cx = -m_HalfWidth + spacing * (i + 1);
			SpawnShieldBunker({ cx, SHIELD_Y });
		}
	}

	void GameLayer::SpawnShieldBunker(const glm::vec2& center)
	{
		const float halfCols = (SHIELD_COLS - 1) * 0.5f;
		const float halfRows = (SHIELD_ROWS - 1) * 0.5f;

		for (int r = 0; r < SHIELD_ROWS; r++)
		{
			for (int c = 0; c < SHIELD_COLS; c++)
			{
				// Carve a notch out of the bottom-centre for the classic bunker shape.
				if (r == SHIELD_ROWS - 1 && c == SHIELD_COLS / 2)
					continue;

				const float x = center.x + (c - halfCols) * SHIELD_BLOCK;
				const float y = center.y + (halfRows - r) * SHIELD_BLOCK;

				Entity block = m_GameScene->CreateEntity("ShieldBlock");
				auto& transform = block.GetComponent<TransformComponent>();
				transform.Position = { x, y, 0.0f };
				transform.Size = { SHIELD_BLOCK, SHIELD_BLOCK };
				block.AddComponent<SpriteRendererComponent>(COLOR_SHIELD);
				block.AddComponent<ShieldTag>();
			}
		}
	}

	void GameLayer::SpawnInvaderFormation()
	{
		m_InvaderDirection = 1.0f;
		m_InvaderSpeed = INVADER_BASE_SPEED + (m_Wave - 1) * 0.6f;

		const float startX = -((INVADER_COLS - 1) * 0.5f) * INVADER_SPACING_X;
		for (int row = 0; row < INVADER_ROWS; row++)
		{
			for (int col = 0; col < INVADER_COLS; col++)
			{
				Entity invader = m_GameScene->CreateEntity("Invader");
				auto& transform = invader.GetComponent<TransformComponent>();
				transform.Position = { startX + col * INVADER_SPACING_X, INVADER_TOP_Y - row * INVADER_SPACING_Y, 0.0f };
				transform.Size = INVADER_SIZE;
				invader.AddComponent<SpriteRendererComponent>(INVADER_ROW_COLORS[row]);

				auto& tag = invader.AddComponent<InvaderTag>();
				tag.Row = row;
				tag.Column = col;
			}
		}
	}

	// ----------------------------------------------------------------------
	// Systems
	// ----------------------------------------------------------------------

	void GameLayer::UpdatePlayer(float dt)
	{
		if (!m_Player || !m_GameScene->GetRegistry().valid((entt::entity)m_Player))
			return;

		auto& transform = m_Player.GetComponent<TransformComponent>();

		float dir = 0.0f;
		if (Input::IsKeyPressed(Key::A) || Input::IsKeyPressed(Key::Left))  dir -= 1.0f;
		if (Input::IsKeyPressed(Key::D) || Input::IsKeyPressed(Key::Right)) dir += 1.0f;

		transform.Position.x += dir * PLAYER_SPEED * dt;
		const float limit = m_HalfWidth - PLAYER_SIZE.x * 0.5f;
		transform.Position.x = glm::clamp(transform.Position.x, -limit, limit);

		if (m_PlayerFireCooldown > 0.0f)
			m_PlayerFireCooldown -= dt;

		if (Input::IsKeyDown(Key::Space) && m_PlayerFireCooldown <= 0.0f)
		{
			// Classic rule: only one player bullet in flight at a time.
			bool hasBullet = false;
			auto bullets = m_GameScene->GetAllEntitiesWith<BulletTag>();
			for (auto e : bullets)
			{
				if (bullets.get<BulletTag>(e).FromPlayer)
				{
					hasBullet = true;
					break;
				}
			}

			if (!hasBullet)
			{
				// Capture the spawn point BEFORE creating the bullet: creating an
				// entity can reallocate the TransformComponent pool and invalidate
				// the `transform` reference above.
				const float spawnX = transform.Position.x;
				const float spawnY = transform.Position.y + PLAYER_SIZE.y * 0.5f + PLAYER_BULLET_SIZE.y * 0.5f;

				Entity bullet = m_GameScene->CreateEntity("PlayerBullet");
				auto& bt = bullet.GetComponent<TransformComponent>();
				bt.Position = { spawnX, spawnY, 0.0f };
				bt.Size = PLAYER_BULLET_SIZE;
				bullet.AddComponent<SpriteRendererComponent>(COLOR_BULLET);
				bullet.AddComponent<BulletTag>().FromPlayer = true;
				bullet.AddComponent<VelocityComponent>().Velocity = { 0.0f, PLAYER_BULLET_SPEED };

				m_PlayerFireCooldown = PLAYER_FIRE_COOLDOWN;
			}
		}
	}

	void GameLayer::UpdateFormation(float dt)
	{
		auto view = m_GameScene->GetAllEntitiesWith<InvaderTag, TransformComponent>();

		const float step = m_InvaderDirection * m_InvaderSpeed * dt;
		float minX = std::numeric_limits<float>::max();
		float maxX = std::numeric_limits<float>::lowest();
		float lowestY = std::numeric_limits<float>::max();
		bool any = false;

		for (auto e : view)
		{
			auto& transform = view.get<TransformComponent>(e);
			transform.Position.x += step;
			minX = std::min(minX, transform.Position.x);
			maxX = std::max(maxX, transform.Position.x);
			lowestY = std::min(lowestY, transform.Position.y);
			any = true;
		}

		if (!any)
			return;

		const float limit = m_HalfWidth - INVADER_SIZE.x * 0.5f - 0.3f;
		const bool hitEdge = (m_InvaderDirection > 0.0f && maxX >= limit)
			|| (m_InvaderDirection < 0.0f && minX <= -limit);

		if (hitEdge)
		{
			m_InvaderDirection = -m_InvaderDirection;
			for (auto e : view)
				view.get<TransformComponent>(e).Position.y -= INVADER_DROP;
			lowestY -= INVADER_DROP;
		}

		if (lowestY <= INVASION_Y)
			m_Defeat = true;
	}

	void GameLayer::UpdateFiring(float dt)
	{
		m_InvaderFireTimer -= dt;
		if (m_InvaderFireTimer > 0.0f)
			return;

		// Invaders fire more often in later waves.
		m_InvaderFireTimer = std::max(0.35f, INVADER_FIRE_INTERVAL - (m_Wave - 1) * 0.12f);

		// Choose the lowest invader in each occupied column, then fire from a random one.
		std::unordered_map<int, entt::entity> bottomByColumn;
		std::unordered_map<int, float> bottomY;

		auto view = m_GameScene->GetAllEntitiesWith<InvaderTag, TransformComponent>();
		for (auto e : view)
		{
			auto [tag, transform] = view.get<InvaderTag, TransformComponent>(e);
			auto it = bottomY.find(tag.Column);
			if (it == bottomY.end() || transform.Position.y < it->second)
			{
				bottomY[tag.Column] = transform.Position.y;
				bottomByColumn[tag.Column] = e;
			}
		}

		if (bottomByColumn.empty())
			return;

		auto it = bottomByColumn.begin();
		std::advance(it, rand() % (int)bottomByColumn.size());
		const entt::entity shooter = it->second;

		// Copy the spawn point before creating the bomb (pool-reallocation safety).
		const glm::vec3 shooterPos = view.get<TransformComponent>(shooter).Position;

		Entity bomb = m_GameScene->CreateEntity("Bomb");
		auto& bt = bomb.GetComponent<TransformComponent>();
		bt.Position = { shooterPos.x, shooterPos.y - INVADER_SIZE.y * 0.5f - BOMB_SIZE.y * 0.5f, 0.0f };
		bt.Size = BOMB_SIZE;
		bomb.AddComponent<SpriteRendererComponent>(COLOR_BOMB);
		bomb.AddComponent<BulletTag>().FromPlayer = false;
		bomb.AddComponent<VelocityComponent>().Velocity = { 0.0f, -BOMB_SPEED };
	}

	void GameLayer::UpdateBullets(float dt)
	{
		std::vector<entt::entity> toDestroy;

		auto view = m_GameScene->GetAllEntitiesWith<BulletTag, VelocityComponent, TransformComponent>();
		for (auto e : view)
		{
			auto [tag, velocity, transform] = view.get<BulletTag, VelocityComponent, TransformComponent>(e);
			transform.Position.x += velocity.Velocity.x * dt;
			transform.Position.y += velocity.Velocity.y * dt;

			if (transform.Position.y > m_HalfHeight + 1.0f || transform.Position.y < -m_HalfHeight - 1.0f)
				toDestroy.push_back(e);
		}

		auto& registry = m_GameScene->GetRegistry();
		for (auto e : toDestroy)
			if (registry.valid(e))
				m_GameScene->DestroyEntity(Entity{ e, m_GameScene });
	}

	void GameLayer::UpdateCollisions()
	{
		auto& registry = m_GameScene->GetRegistry();
		std::vector<entt::entity> toDestroy;

		auto bullets = m_GameScene->GetAllEntitiesWith<BulletTag, TransformComponent>();
		for (auto b : bullets)
		{
			auto [btag, bt] = bullets.get<BulletTag, TransformComponent>(b);

			if (btag.FromPlayer)
			{
				bool consumed = false;

				auto invaders = m_GameScene->GetAllEntitiesWith<InvaderTag, TransformComponent>();
				for (auto inv : invaders)
				{
					auto [itag, it] = invaders.get<InvaderTag, TransformComponent>(inv);
					if (Overlaps(bt, it))
					{
						toDestroy.push_back(inv);
						toDestroy.push_back(b);
						m_Score += (INVADER_ROWS - itag.Row) * 10;
						consumed = true;
						break;
					}
				}
				if (consumed)
					continue;

				auto shields = m_GameScene->GetAllEntitiesWith<ShieldTag, TransformComponent>();
				for (auto s : shields)
				{
					if (Overlaps(bt, shields.get<TransformComponent>(s)))
					{
						toDestroy.push_back(s);
						toDestroy.push_back(b);
						break;
					}
				}
			}
			else
			{
				bool consumed = false;

				auto shields = m_GameScene->GetAllEntitiesWith<ShieldTag, TransformComponent>();
				for (auto s : shields)
				{
					if (Overlaps(bt, shields.get<TransformComponent>(s)))
					{
						toDestroy.push_back(s);
						toDestroy.push_back(b);
						consumed = true;
						break;
					}
				}
				if (consumed)
					continue;

				if (m_Player && registry.valid((entt::entity)m_Player))
				{
					if (Overlaps(bt, m_Player.GetComponent<TransformComponent>()))
					{
						toDestroy.push_back(b);
						m_Lives--;
						if (m_Lives <= 0)
							m_Defeat = true;
					}
				}
			}
		}

		for (auto e : toDestroy)
			if (registry.valid(e))
				m_GameScene->DestroyEntity(Entity{ e, m_GameScene });
	}

	void GameLayer::UpdateHud()
	{
		auto& registry = m_GameScene->GetRegistry();

		if (m_ScoreText && registry.valid((entt::entity)m_ScoreText))
			m_ScoreText.GetComponent<TextComponent>().Text = std::format("SCORE {}", m_Score);

		if (m_WaveText && registry.valid((entt::entity)m_WaveText))
			m_WaveText.GetComponent<TextComponent>().Text = std::format("WAVE {}", m_Wave);

		if (m_LivesText && registry.valid((entt::entity)m_LivesText))
		{
			auto& text = m_LivesText.GetComponent<TextComponent>();
			text.Text = std::format("LIVES {}", std::max(0, m_Lives));

			// Right-align against the right playfield edge.
			const float width = m_Font->GetStringWidth(text.Text, text.Size);
			m_LivesText.GetComponent<TransformComponent>().Position.x = m_HalfWidth - 0.5f - width;
		}
	}

	void GameLayer::EndGame()
	{
		if (m_GameOverScoreText && m_GameOverScene->GetRegistry().valid((entt::entity)m_GameOverScoreText))
			m_GameOverScoreText.GetComponent<TextComponent>().Text = std::format("SCORE {}", m_Score);

		m_SceneManager.SetActiveScene("GameOver");
	}

	int GameLayer::CountInvaders()
	{
		int count = 0;
		for ([[maybe_unused]] auto e : m_GameScene->GetAllEntitiesWith<InvaderTag>())
			count++;
		return count;
	}

}
