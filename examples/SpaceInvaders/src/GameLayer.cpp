#include "GameLayer.h"
#include "GameScripts.h"
#include "GameTuning.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <format>

namespace Dingo
{

	namespace
	{
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
		m_Context.HalfHeight = ORTHO_HEIGHT * 0.5f;
		m_Context.HalfWidth = m_Context.HalfHeight * aspect;
		m_ViewProjection = glm::ortho(-m_Context.HalfWidth, m_Context.HalfWidth, -m_Context.HalfHeight, m_Context.HalfHeight, -1.0f, 1.0f);

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
			m_SceneManager.OnUpdate(deltaTime);   // drives the entity scripts
			UpdateHud();

			if (m_Context.GameOver)
				EndGame();
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
		footer.GetComponent<TransformComponent>().Position = { -m_Context.HalfWidth + 0.4f, -m_Context.HalfHeight + 0.4f, 0.0f };
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
		m_Context.Score = 0;
		m_Context.Lives = 3;
		m_Context.Wave = 1;
		m_Context.GameOver = false;

		m_GameScene->Clear();

		BuildHud();
		SpawnPlayer();
		SpawnShields();
		SpawnInvaderFormation(*m_GameScene, m_Context);

		// The controller entity is invisible (no renderable components); it just
		// runs the formation behaviour.
		m_GameScene->CreateEntity("FormationController").AddScript<FormationControllerScript>(&m_Context);

		m_SceneManager.SetActiveScene("Game");
	}

	void GameLayer::BuildHud()
	{
		m_ScoreText = m_GameScene->CreateEntity("ScoreHud");
		m_ScoreText.GetComponent<TransformComponent>().Position = { -m_Context.HalfWidth + 0.5f, m_Context.HalfHeight - 1.1f, 0.0f };
		m_ScoreText.AddComponent<TextComponent>(MakeText("SCORE 0", m_Font, FONT_HUD, COLOR_TEXT, false));

		m_WaveText = m_GameScene->CreateEntity("WaveHud");
		m_WaveText.GetComponent<TransformComponent>().Position = { 0.0f, m_Context.HalfHeight - 1.1f, 0.0f };
		m_WaveText.AddComponent<TextComponent>(MakeText("WAVE 1", m_Font, FONT_HUD, COLOR_TEXT, true));

		// Right-aligned each frame in UpdateHud.
		m_LivesText = m_GameScene->CreateEntity("LivesHud");
		m_LivesText.GetComponent<TransformComponent>().Position = { m_Context.HalfWidth - 0.5f, m_Context.HalfHeight - 1.1f, 0.0f };
		m_LivesText.AddComponent<TextComponent>(MakeText("LIVES 3", m_Font, FONT_HUD, COLOR_TEXT, false));
	}

	void GameLayer::SpawnPlayer()
	{
		Entity player = m_GameScene->CreateEntity("Player");
		auto& transform = player.GetComponent<TransformComponent>();
		transform.Position = { 0.0f, PLAYER_Y, 0.0f };
		transform.Size = PLAYER_SIZE;
		player.AddComponent<SpriteRendererComponent>(COLOR_PLAYER);
		player.AddScript<PlayerScript>(&m_Context);

		m_Context.Player = player;
	}

	void GameLayer::SpawnShields()
	{
		const float spacing = (2.0f * m_Context.HalfWidth) / (SHIELD_COUNT + 1);
		for (int i = 0; i < SHIELD_COUNT; i++)
		{
			const float cx = -m_Context.HalfWidth + spacing * (i + 1);
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

				Entity block = m_GameScene->CreateEntity("ShieldBlock");
				auto& transform = block.GetComponent<TransformComponent>();
				transform.Position = {
					center.x + (c - halfCols) * SHIELD_BLOCK,
					center.y + (halfRows - r) * SHIELD_BLOCK,
					0.0f
				};
				transform.Size = { SHIELD_BLOCK, SHIELD_BLOCK };
				block.AddComponent<SpriteRendererComponent>(COLOR_SHIELD);
				block.AddScript<ShieldScript>();
			}
		}
	}

	void GameLayer::UpdateHud()
	{
		if (m_ScoreText.IsValid())
			m_ScoreText.GetComponent<TextComponent>().Text = std::format("SCORE {}", m_Context.Score);

		if (m_WaveText.IsValid())
			m_WaveText.GetComponent<TextComponent>().Text = std::format("WAVE {}", m_Context.Wave);

		if (m_LivesText.IsValid())
		{
			auto& text = m_LivesText.GetComponent<TextComponent>();
			text.Text = std::format("LIVES {}", std::max(0, m_Context.Lives));

			// Right-align against the right playfield edge.
			const float width = m_Font->GetStringWidth(text.Text, text.Size);
			m_LivesText.GetComponent<TransformComponent>().Position.x = m_Context.HalfWidth - 0.5f - width;
		}
	}

	void GameLayer::EndGame()
	{
		if (m_GameOverScoreText.IsValid())
			m_GameOverScoreText.GetComponent<TextComponent>().Text = std::format("SCORE {}", m_Context.Score);

		m_SceneManager.SetActiveScene("GameOver");
	}

}
