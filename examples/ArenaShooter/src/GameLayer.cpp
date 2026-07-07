#include "GameLayer.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <format>

namespace Dingo
{

	namespace
	{
		constexpr float PLAYER_SPRITE_SIZE = 52.0f;
		constexpr float ENEMY_SPRITE_SIZE = 42.0f;
		constexpr float BULLET_SPRITE_SIZE = 14.0f;

		constexpr float PLAYER_SPEED = 340.0f;
		constexpr float BULLET_SPEED = 720.0f;
		constexpr float BULLET_LIFE = 1.4f;
		constexpr float FIRE_INTERVAL = 0.14f;

		constexpr float ENEMY_BASE_SPEED = 70.0f;
		constexpr float ENEMY_RADIUS = 20.0f;
		constexpr float BULLET_RADIUS = 8.0f;
		constexpr float PLAYER_HIT_RADIUS = 22.0f;

		constexpr float INVULNERABILITY_TIME = 1.5f;
		constexpr float ARENA_MARGIN = 30.0f;

		// std140 uniform block feeding assets/shaders/background.glsl.
		struct BackgroundUniform
		{
			glm::vec2 Resolution;
			float Time;
			float Pad;
		};

		float RandomRange(std::mt19937& rng, float min, float max)
		{
			std::uniform_real_distribution<float> dist(min, max);
			return dist(rng);
		}
	}

	void GameLayer::OnAttach()
	{
		AssetManager& assets = Application::Get().GetAssetManager();

		m_PlayerTexHandle = assets.LoadAsync("sprites/player.png");
		m_EnemyTexHandle = assets.LoadAsync("sprites/enemy.png");
		m_BulletTexHandle = assets.LoadAsync("sprites/bullet.png");
		m_ShaderHandle = assets.LoadAsync("shaders/background.glsl");
		m_FontHandle = assets.LoadAsync("fonts/arialbd.ttf");
		m_ShootClip = assets.LoadAsync("audio/shoot.wav");
		m_DeathClip = assets.LoadAsync("audio/death.wav");
		m_HitClip = assets.LoadAsync("audio/hit.wav");
		m_WaveClip = assets.LoadAsync("audio/wave.wav");

		m_TotalAssets = 9;

		DE_INFO("Arena Shooter: queued {} assets for async loading.", m_TotalAssets);
	}

	void GameLayer::OnDetach()
	{
		if (m_BackgroundPipeline) { m_BackgroundPipeline->Destroy(); m_BackgroundPipeline = nullptr; }
		if (m_BackgroundVertexBuffer) { m_BackgroundVertexBuffer->Destroy(); m_BackgroundVertexBuffer = nullptr; }
		if (m_BackgroundIndexBuffer) { m_BackgroundIndexBuffer->Destroy(); m_BackgroundIndexBuffer = nullptr; }
		if (m_BackgroundUniformBuffer) { m_BackgroundUniformBuffer->Destroy(); m_BackgroundUniformBuffer = nullptr; }
	}

	void GameLayer::OnUpdate(float deltaTime)
	{
		if (Input::IsKeyPressed(Key::Escape))
			Application::Get().Close();

		m_Time += deltaTime;
		m_Width = (float)Application::Get().GetWindow().GetWidth();
		m_Height = (float)Application::Get().GetWindow().GetHeight();
		m_Projection = glm::ortho(0.0f, m_Width, 0.0f, m_Height, -1.0f, 1.0f);

		Renderer2D& renderer = Application::Get().GetRenderer2D();

		if (m_GameState == GameState::Loading)
		{
			renderer.BeginScene(m_Projection);
			renderer.Clear(glm::vec4(0.03f, 0.04f, 0.09f, 1.0f));
			UpdateLoading(renderer);
			renderer.EndScene();
			return;
		}

		RenderBackground();

		renderer.BeginScene(m_Projection);

		switch (m_GameState)
		{
			case GameState::Ready:    UpdateReady(deltaTime, renderer); break;
			case GameState::Playing:  UpdatePlaying(deltaTime, renderer); break;
			case GameState::GameOver: UpdateGameOver(deltaTime, renderer); break;
			default: break;
		}

		renderer.EndScene();
	}

	void GameLayer::UpdateLoading(Renderer2D& renderer)
	{
		AssetManager& assets = Application::Get().GetAssetManager();

		const uint32_t pending = assets.GetPendingCount();
		const float fraction = m_TotalAssets > 0 ? (float)(m_TotalAssets - pending) / (float)m_TotalAssets : 1.0f;

		const float barWidth = m_Width * 0.5f;
		const float barHeight = 26.0f;
		const float barX = (m_Width - barWidth) * 0.5f;
		const float barY = m_Height * 0.5f;

		renderer.DrawQuad(glm::vec2(barX + barWidth * 0.5f, barY + barHeight * 0.5f), glm::vec2(barWidth + 8.0f, barHeight + 8.0f), glm::vec4(0.10f, 0.12f, 0.20f, 1.0f));
		renderer.DrawQuad(glm::vec2(barX + (barWidth * fraction) * 0.5f, barY + barHeight * 0.5f), glm::vec2(barWidth * fraction, barHeight), glm::vec4(0.25f, 0.75f, 0.95f, 1.0f));

		if (pending == 0)
			OnAssetsReady();
	}

	void GameLayer::OnAssetsReady()
	{
		AssetManager& assets = Application::Get().GetAssetManager();

		m_PlayerTexture = assets.GetTexture(m_PlayerTexHandle);
		m_EnemyTexture = assets.GetTexture(m_EnemyTexHandle);
		m_BulletTexture = assets.GetTexture(m_BulletTexHandle);
		m_Font = assets.GetFont(m_FontHandle);
		m_BackgroundShader = assets.GetShader(m_ShaderHandle);

		if (!m_PlayerTexture || !m_EnemyTexture || !m_BulletTexture || !m_Font || !m_BackgroundShader)
			DE_WARN("Arena Shooter: one or more assets failed to load; check the asset root.");

		if (m_BackgroundShader)
		{
			const float quad[] = { -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f };
			const uint16_t indices[] = { 0, 1, 2, 2, 3, 0 };

			m_BackgroundVertexBuffer = GraphicsBuffer::CreateVertexBuffer(sizeof(quad), quad, true, "Background Vertices");
			m_BackgroundIndexBuffer = GraphicsBuffer::CreateIndexBuffer(sizeof(indices), indices, true, "Background Indices");
			m_BackgroundUniformBuffer = GraphicsBuffer::CreateUniformBuffer(sizeof(BackgroundUniform), "Background Uniform");

			VertexLayout layout = VertexLayout()
				.SetStride(sizeof(float) * 2)
				.AddAttribute("inPosition", Format::RG32_FLOAT, 0);

			m_BackgroundPipeline = Pipeline::Create(PipelineParams()
				.SetDebugName("Arena Background Pipeline")
				.SetShader(m_BackgroundShader)
				.SetFramebuffer(Renderer::GetSwapChainFramebuffer())
				.SetFillMode(FillMode::Solid)
				.SetCullMode(CullMode::None)
				.SetDepthTest(false)
				.SetDepthWrite(false)
				.SetVertexLayout(layout)
				.SetUniformBuffer(m_BackgroundUniformBuffer));
		}

		m_GameState = GameState::Ready;
		DE_INFO("Arena Shooter: all assets ready - entering menu.");
	}

	void GameLayer::UpdateReady(float deltaTime, Renderer2D& renderer)
	{
		if (WantsStart())
			StartGame();

		DrawCenteredText(renderer, "ARENA SHOOTER", 90.0f, glm::vec2(0.0f, 120.0f), glm::vec4(0.6f, 0.9f, 1.0f, 1.0f));
		DrawCenteredText(renderer, "PRESS ENTER OR (A) TO PLAY", 40.0f, glm::vec2(0.0f, 0.0f));
		DrawCenteredText(renderer, "MOVE: WASD / LEFT STICK   AIM+FIRE: MOUSE / RIGHT STICK", 24.0f, glm::vec2(0.0f, -70.0f), glm::vec4(0.8f, 0.8f, 0.85f, 1.0f));

		if (m_PlayerTexture)
			renderer.DrawQuad(glm::vec2(m_Width * 0.5f, m_Height * 0.5f - 190.0f), glm::vec2(PLAYER_SPRITE_SIZE, PLAYER_SPRITE_SIZE), m_PlayerTexture);
	}

	void GameLayer::UpdatePlaying(float deltaTime, Renderer2D& renderer)
	{
		AudioEngine& audio = Application::Get().GetAudioEngine();

		glm::vec2 move(0.0f);
		if (Input::IsKeyDown(Key::A)) move.x -= 1.0f;
		if (Input::IsKeyDown(Key::D)) move.x += 1.0f;
		if (Input::IsKeyDown(Key::W)) move.y += 1.0f;
		if (Input::IsKeyDown(Key::S)) move.y -= 1.0f;

		glm::vec2 stick = Input::GetGamepadLeftStick();
		move.x += stick.x;
		move.y -= stick.y;

		if (glm::dot(move, move) > 1.0f)
			move = glm::normalize(move);

		m_PlayerPosition += move * PLAYER_SPEED * deltaTime;
		m_PlayerPosition.x = std::clamp(m_PlayerPosition.x, ARENA_MARGIN, m_Width - ARENA_MARGIN);
		m_PlayerPosition.y = std::clamp(m_PlayerPosition.y, ARENA_MARGIN, m_Height - ARENA_MARGIN);

		const glm::vec2 aim = GetAimDirection();

		m_FireCooldown -= deltaTime;
		const bool firing = Input::IsMouseButtonDown(MouseButton::Left)
			|| Input::IsKeyDown(Key::Space)
			|| Input::IsGamepadButtonDown(GamepadButton::A)
			|| Input::GetGamepadAxis(GamepadAxis::RightTrigger) > 0.5f;

		if (firing && m_FireCooldown <= 0.0f)
		{
			m_Bullets.push_back({ m_PlayerPosition + aim * (PLAYER_SPRITE_SIZE * 0.4f), aim * BULLET_SPEED, BULLET_LIFE });
			m_FireCooldown = FIRE_INTERVAL;
			if (m_ShootClip != k_InvalidAsset)
				audio.PlayOneShot(Application::Get().GetAssetManager().GetAudioClip(m_ShootClip), 0.35f);
		}

		for (auto& bullet : m_Bullets)
		{
			bullet.Position += bullet.Velocity * deltaTime;
			bullet.Life -= deltaTime;
		}

		m_SpawnTimer -= deltaTime;
		if (m_EnemiesToSpawn > 0 && m_SpawnTimer <= 0.0f)
		{
			glm::vec2 spawn;
			const int edge = (int)RandomRange(m_Rng, 0.0f, 4.0f) % 4;
			switch (edge)
			{
				case 0: spawn = { RandomRange(m_Rng, 0.0f, m_Width), m_Height + ENEMY_SPRITE_SIZE }; break;
				case 1: spawn = { RandomRange(m_Rng, 0.0f, m_Width), -ENEMY_SPRITE_SIZE }; break;
				case 2: spawn = { -ENEMY_SPRITE_SIZE, RandomRange(m_Rng, 0.0f, m_Height) }; break;
				default: spawn = { m_Width + ENEMY_SPRITE_SIZE, RandomRange(m_Rng, 0.0f, m_Height) }; break;
			}
			const float speed = ENEMY_BASE_SPEED + (float)m_Wave * 8.0f + RandomRange(m_Rng, -10.0f, 20.0f);
			m_Enemies.push_back({ spawn, speed, 1.0f + (float)(m_Wave / 4) });
			--m_EnemiesToSpawn;
			m_SpawnTimer = std::max(0.25f, 0.9f - (float)m_Wave * 0.03f);
		}

		for (auto& enemy : m_Enemies)
		{
			glm::vec2 dir = m_PlayerPosition - enemy.Position;
			const float len = glm::length(dir);
			if (len > 0.001f)
				enemy.Position += (dir / len) * enemy.Speed * deltaTime;
		}

		for (auto& bullet : m_Bullets)
		{
			if (bullet.Life <= 0.0f)
				continue;
			for (auto& enemy : m_Enemies)
			{
				if (enemy.Health <= 0.0f)
					continue;
				const float r = ENEMY_RADIUS + BULLET_RADIUS;
				if (glm::dot(bullet.Position - enemy.Position, bullet.Position - enemy.Position) <= r * r)
				{
					bullet.Life = 0.0f;
					enemy.Health -= 1.0f;
					if (enemy.Health <= 0.0f)
					{
						m_Explosions.push_back({ enemy.Position, 0.0f, ENEMY_SPRITE_SIZE });
						m_Score += 100;
						if (m_DeathClip != k_InvalidAsset)
							audio.PlayOneShot(Application::Get().GetAssetManager().GetAudioClip(m_DeathClip), 0.6f);
					}
					break;
				}
			}
		}

		m_InvulnerabilityTimer = std::max(0.0f, m_InvulnerabilityTimer - deltaTime);
		if (m_InvulnerabilityTimer <= 0.0f)
		{
			for (auto& enemy : m_Enemies)
			{
				if (enemy.Health <= 0.0f)
					continue;
				const float r = ENEMY_RADIUS + PLAYER_HIT_RADIUS;
				if (glm::dot(m_PlayerPosition - enemy.Position, m_PlayerPosition - enemy.Position) <= r * r)
				{
					--m_Lives;
					m_InvulnerabilityTimer = INVULNERABILITY_TIME;
					if (m_HitClip != k_InvalidAsset)
						audio.PlayOneShot(Application::Get().GetAssetManager().GetAudioClip(m_HitClip), 0.7f);
					if (m_Lives <= 0)
						m_GameState = GameState::GameOver;
					break;
				}
			}
		}

		for (auto& explosion : m_Explosions)
			explosion.Timer += deltaTime;

		std::erase_if(m_Bullets, [](const Bullet& b) {
			return b.Life <= 0.0f;
		});
		std::erase_if(m_Enemies, [](const Enemy& e) {
			return e.Health <= 0.0f;
		});
		std::erase_if(m_Explosions, [](const Explosion& e) {
			return e.Timer >= 0.35f;
		});

		if (m_EnemiesToSpawn == 0 && m_Enemies.empty())
			StartNextWave();

		m_WaveBannerTimer = std::max(0.0f, m_WaveBannerTimer - deltaTime);

		for (const auto& bullet : m_Bullets)
			renderer.DrawQuad(bullet.Position, glm::vec2(BULLET_SPRITE_SIZE), m_BulletTexture);

		for (const auto& enemy : m_Enemies)
			renderer.DrawQuad(enemy.Position, glm::vec2(ENEMY_SPRITE_SIZE), m_EnemyTexture);

		for (const auto& explosion : m_Explosions)
		{
			const float t = explosion.Timer / 0.35f;
			const float size = explosion.Size * (0.6f + t * 1.6f);
			renderer.DrawQuad(explosion.Position, glm::vec2(size), glm::vec4(1.0f, 0.8f, 0.3f, 1.0f - t));
		}

		const bool flicker = m_InvulnerabilityTimer > 0.0f && (std::fmod(m_Time, 0.16f) < 0.08f);
		if (m_PlayerTexture && !flicker)
			renderer.DrawRotatedQuad(m_PlayerPosition, std::atan2(aim.y, aim.x) * 57.2957795f - 90.0f, glm::vec2(PLAYER_SPRITE_SIZE), m_PlayerTexture);

		RenderHUD(renderer);

		if (m_WaveBannerTimer > 0.0f)
			DrawCenteredText(renderer, std::format("WAVE {}", m_Wave), 70.0f, glm::vec2(0.0f, 60.0f), glm::vec4(1.0f, 0.85f, 0.4f, std::min(1.0f, m_WaveBannerTimer)));
	}

	void GameLayer::UpdateGameOver(float deltaTime, Renderer2D& renderer)
	{
		if (WantsStart())
			StartGame();

		DrawCenteredText(renderer, "GAME OVER", 90.0f, glm::vec2(0.0f, 100.0f), glm::vec4(1.0f, 0.35f, 0.3f, 1.0f));
		DrawCenteredText(renderer, std::format("SCORE {}   WAVE {}", m_Score, m_Wave), 44.0f, glm::vec2(0.0f, 10.0f));
		DrawCenteredText(renderer, "PRESS ENTER OR (A) TO RETRY", 32.0f, glm::vec2(0.0f, -70.0f), glm::vec4(0.85f, 0.85f, 0.9f, 1.0f));
	}

	void GameLayer::StartGame()
	{
		m_Bullets.clear();
		m_Enemies.clear();
		m_Explosions.clear();
		m_PlayerPosition = glm::vec2(m_Width * 0.5f, m_Height * 0.5f);
		m_Lives = 3;
		m_Score = 0;
		m_Wave = 0;
		m_EnemiesToSpawn = 0;
		m_InvulnerabilityTimer = 0.0f;
		m_FireCooldown = 0.0f;
		m_GameState = GameState::Playing;
		StartNextWave();
	}

	void GameLayer::StartNextWave()
	{
		++m_Wave;
		m_EnemiesToSpawn = 4 + m_Wave * 2;
		m_SpawnTimer = 0.0f;
		m_WaveBannerTimer = 2.0f;
		if (m_WaveClip != k_InvalidAsset)
			Application::Get().GetAudioEngine().PlayOneShot(Application::Get().GetAssetManager().GetAudioClip(m_WaveClip), 0.6f);
	}

	void GameLayer::RenderBackground()
	{
		if (!m_BackgroundPipeline)
			return;

		BackgroundUniform data{};
		data.Resolution = glm::vec2(m_Width, m_Height);
		data.Time = m_Time;
		m_BackgroundUniformBuffer->Upload(&data, sizeof(BackgroundUniform));
		Renderer::Upload(m_BackgroundUniformBuffer);

		Renderer::DrawIndexed(m_BackgroundPipeline, m_BackgroundVertexBuffer, m_BackgroundIndexBuffer, 6);
	}

	void GameLayer::RenderHUD(Renderer2D& renderer)
	{
		if (!m_Font)
			return;

		constexpr glm::vec4 color = { 0.95f, 0.97f, 1.0f, 1.0f };
		renderer.DrawText(std::format("SCORE {}", m_Score), m_Font, glm::vec2(24.0f, m_Height - 48.0f), 34.0f, { color });
		renderer.DrawText(std::format("WAVE {}", m_Wave), m_Font, glm::vec2(24.0f, m_Height - 90.0f), 34.0f, { color });

		const std::string lives = std::format("LIVES {}", std::max(0, m_Lives));
		const float w = m_Font->GetStringWidth(lives, 34.0f);
		renderer.DrawText(lives, m_Font, glm::vec2(m_Width - w - 24.0f, m_Height - 48.0f), 34.0f, { glm::vec4(1.0f, 0.6f, 0.55f, 1.0f) });
	}

	void GameLayer::DrawCenteredText(Renderer2D& renderer, const std::string& text, float size, const glm::vec2& offset, const glm::vec4& color)
	{
		if (!m_Font)
			return;
		const float w = m_Font->GetStringWidth(text, size);
		renderer.DrawText(text, m_Font, glm::vec2(m_Width * 0.5f - w * 0.5f + offset.x, m_Height * 0.5f + offset.y), size, { color });
	}

	glm::vec2 GameLayer::GetAimDirection() const
	{
		glm::vec2 rightStick = Input::GetGamepadRightStick();
		if (glm::dot(rightStick, rightStick) > 0.09f)
			return glm::normalize(glm::vec2(rightStick.x, -rightStick.y));

		glm::vec2 mouse = Input::GetMousePosition();
		glm::vec2 world = glm::vec2(mouse.x, m_Height - mouse.y);
		glm::vec2 dir = world - m_PlayerPosition;
		if (glm::dot(dir, dir) < 0.0001f)
			return glm::vec2(0.0f, 1.0f);
		return glm::normalize(dir);
	}

	bool GameLayer::WantsStart() const
	{
		return Input::IsKeyPressed(Key::Enter)
			|| Input::IsKeyPressed(Key::Space)
			|| Input::IsGamepadButtonPressed(GamepadButton::A);
	}

}
