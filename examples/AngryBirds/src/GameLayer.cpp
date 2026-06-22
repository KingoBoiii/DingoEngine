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
		constexpr int LEVEL_COUNT = 2;

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

		RigidBody2DComponent MakeBody(RigidBody2DComponent::BodyType type)
		{
			return RigidBody2DComponent(type);
		}
	}

	// ----------------------------------------------------------------------
	// Lifecycle
	// ----------------------------------------------------------------------

	void GameLayer::OnAttach()
	{
		const float aspect = (float)Application::Get().GetWindow().GetWidth() / (float)Application::Get().GetWindow().GetHeight();
		m_Context.HalfHeight = ORTHO_HEIGHT * 0.5f;
		m_Context.HalfWidth = m_Context.HalfHeight * aspect;
		m_ViewProjection = glm::ortho(-m_Context.HalfWidth, m_Context.HalfWidth, -m_Context.HalfHeight, m_Context.HalfHeight, -1.0f, 1.0f);

		m_SlingPos = { -m_Context.HalfWidth + SLING_INSET_X, GROUND_TOP_Y + SLING_HEIGHT };

		m_Font = Font::Create("assets/fonts/arialbd.ttf");

		m_MenuScene = m_SceneManager.CreateScene("Menu");
		m_GameScene = m_SceneManager.CreateScene("Game");
		m_GameOverScene = m_SceneManager.CreateScene("GameOver");

		for (Scene* scene : { m_MenuScene, m_GameScene, m_GameOverScene })
		{
			scene->SetViewProjection(m_ViewProjection);
			scene->SetClearColor(COLOR_SKY);
		}

		BuildMenuScene();

		// GameOver scaffold — the result/score strings are filled in on the transition.
		{
			Entity title = m_GameOverScene->CreateEntity("Title");
			title.GetComponent<TransformComponent>().Position = { 0.0f, 3.0f, 0.0f };
			m_ResultText = title;
			title.AddComponent<TextComponent>(MakeText("", m_Font, FONT_TITLE, COLOR_TEXT, true));

			Entity score = m_GameOverScene->CreateEntity("FinalScore");
			score.GetComponent<TransformComponent>().Position = { 0.0f, 0.2f, 0.0f };
			m_ResultScoreText = score;
			score.AddComponent<TextComponent>(MakeText("", m_Font, FONT_SUB, COLOR_TEXT, true));

			Entity prompt = m_GameOverScene->CreateEntity("Prompt");
			prompt.GetComponent<TransformComponent>().Position = { 0.0f, -2.2f, 0.0f };
			prompt.AddComponent<TextComponent>(MakeText("Press SPACE to return to the menu", m_Font, FONT_SMALL, COLOR_TEXT, true));
		}

		m_SceneManager.SetActiveScene("Menu");
	}

	void GameLayer::OnDetach()
	{
		// Scenes are owned by m_SceneManager and destroyed with it (which stops their
		// physics worlds via Scene::Clear).
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
			if (Input::IsKeyDown(Key::Space) || Input::IsKeyDown(Key::Enter) || Input::IsMouseButtonDown(MouseButton::Left))
				StartGame();
		}
		else if (active == "Game")
		{
			UpdateGame(deltaTime);
		}
		else if (active == "GameOver")
		{
			if (Input::IsKeyDown(Key::Space) || Input::IsKeyDown(Key::Enter) || Input::IsMouseButtonDown(MouseButton::Left))
				m_SceneManager.SetActiveScene("Menu");
		}

		// The Game scene draws its own physics overlay; the others are plain.
		if (m_SceneManager.GetActiveSceneName() == "Game")
			RenderGame();
		else
			m_SceneManager.OnRender(Application::Get().GetRenderer2D());
	}

	// ----------------------------------------------------------------------
	// Menu + flow
	// ----------------------------------------------------------------------

	void GameLayer::BuildMenuScene()
	{
		m_MenuScene->Clear();

		Entity title = m_MenuScene->CreateEntity("Title");
		title.GetComponent<TransformComponent>().Position = { 0.0f, 4.0f, 0.0f };
		title.AddComponent<TextComponent>(MakeText("ANGRY BIRDS", m_Font, FONT_TITLE, { 0.85f, 0.20f, 0.15f, 1.0f }, true));

		Entity prompt = m_MenuScene->CreateEntity("Prompt");
		prompt.GetComponent<TransformComponent>().Position = { 0.0f, 0.8f, 0.0f };
		prompt.AddComponent<TextComponent>(MakeText("Click to play", m_Font, FONT_SUB, COLOR_TEXT, true));

		Entity controls = m_MenuScene->CreateEntity("Controls");
		controls.GetComponent<TransformComponent>().Position = { 0.0f, -1.2f, 0.0f };
		controls.AddComponent<TextComponent>(MakeText("Drag the bird back with the mouse and release to launch", m_Font, FONT_SMALL, COLOR_TEXT, true));

		Entity footer = m_MenuScene->CreateEntity("Footer");
		footer.GetComponent<TransformComponent>().Position = { 0.0f, -3.0f, 0.0f };
		footer.AddComponent<TextComponent>(MakeText("Topple the towers and pop every pig - a Dingo Engine physics example", m_Font, FONT_SMALL, { 0.20f, 0.22f, 0.30f, 1.0f }, true));
	}

	void GameLayer::StartGame()
	{
		m_Context.Score = 0;
		StartLevel(1);
	}

	void GameLayer::StartLevel(int level)
	{
		m_Context.Level = level;
		m_Context.BirdsLeft = BIRD_COUNT;
		m_Context.PigsLeft = 0;
		m_Context.Won = false;

		m_GameScene->Clear();        // also stops the previous level's physics world
		m_GameScene->SetGravity(GRAVITY);

		SpawnGround();
		SpawnStructure(level);       // increments m_Context.PigsLeft per pig
		BuildHud();

		// Build the physics world for the static + structure bodies now in the scene.
		// The waiting bird is spawned afterwards and stays body-less until launched.
		m_GameScene->OnPhysicsStart();

		m_BirdInFlight = false;
		m_Dragging = false;
		SpawnWaitingBird();

		m_SceneManager.SetActiveScene("Game");
	}

	void GameLayer::BuildHud()
	{
		const float top = m_Context.HalfHeight - 1.0f;

		m_ScoreText = m_GameScene->CreateEntity("ScoreHud");
		m_ScoreText.GetComponent<TransformComponent>().Position = { -m_Context.HalfWidth + 0.5f, top, 0.0f };
		m_ScoreText.AddComponent<TextComponent>(MakeText("SCORE 0", m_Font, FONT_HUD, COLOR_TEXT, false));

		m_LevelText = m_GameScene->CreateEntity("LevelHud");
		m_LevelText.GetComponent<TransformComponent>().Position = { 0.0f, top, 0.0f };
		m_LevelText.AddComponent<TextComponent>(MakeText("LEVEL 1", m_Font, FONT_HUD, COLOR_TEXT, true));

		m_BirdsText = m_GameScene->CreateEntity("BirdsHud");
		m_BirdsText.GetComponent<TransformComponent>().Position = { -m_Context.HalfWidth + 0.5f, top - 0.9f, 0.0f };
		m_BirdsText.AddComponent<TextComponent>(MakeText("BIRDS 0", m_Font, FONT_SMALL, COLOR_TEXT, false));

		m_PigsText = m_GameScene->CreateEntity("PigsHud");
		m_PigsText.GetComponent<TransformComponent>().Position = { -m_Context.HalfWidth + 0.5f, top - 1.6f, 0.0f };
		m_PigsText.AddComponent<TextComponent>(MakeText("PIGS 0", m_Font, FONT_SMALL, COLOR_TEXT, false));

		m_HintText = m_GameScene->CreateEntity("HintHud");
		m_HintText.GetComponent<TransformComponent>().Position = { 0.0f, -m_Context.HalfHeight + 0.6f, 0.0f };
		m_HintText.AddComponent<TextComponent>(MakeText("Drag the bird back with the mouse and release to launch", m_Font, FONT_SMALL, { 0.20f, 0.22f, 0.30f, 1.0f }, true));
	}

	void GameLayer::EndGame(bool won)
	{
		if (m_ResultText.IsValid())
		{
			auto& text = m_ResultText.GetComponent<TextComponent>();
			text.Text = won ? "YOU WIN!" : "GAME OVER";
			text.Color = won ? glm::vec4{ 0.20f, 0.55f, 0.25f, 1.0f } : glm::vec4{ 0.75f, 0.20f, 0.18f, 1.0f };
		}

		if (m_ResultScoreText.IsValid())
			m_ResultScoreText.GetComponent<TextComponent>().Text = std::format("SCORE {}", m_Context.Score);

		m_SceneManager.SetActiveScene("GameOver");
	}

	// ----------------------------------------------------------------------
	// Level content
	// ----------------------------------------------------------------------

	void GameLayer::SpawnGround()
	{
		Entity ground = m_GameScene->CreateEntity("Ground");
		{
			auto& transform = ground.GetComponent<TransformComponent>();
			transform.Position = { 0.0f, GROUND_TOP_Y - GROUND_THICKNESS * 0.5f, 0.0f };
			transform.Size = { m_Context.HalfWidth * 2.0f + 4.0f, GROUND_THICKNESS };
		}
		ground.AddComponent<SpriteRendererComponent>(COLOR_GROUND);
		ground.AddComponent<RigidBody2DComponent>(MakeBody(RigidBody2DComponent::BodyType::Static));
		ground.AddComponent<BoxCollider2DComponent>(); // default { 0.5, 0.5 } => full quad

		// Invisible containment walls just outside the view.
		for (float x : { -m_Context.HalfWidth - 0.25f, m_Context.HalfWidth + 0.25f })
		{
			Entity wall = m_GameScene->CreateEntity("Wall");
			auto& transform = wall.GetComponent<TransformComponent>();
			transform.Position = { x, 0.0f, 0.0f };
			transform.Size = { 0.5f, m_Context.HalfHeight * 2.0f };
			wall.AddComponent<RigidBody2DComponent>(MakeBody(RigidBody2DComponent::BodyType::Static));
			wall.AddComponent<BoxCollider2DComponent>();
		}
	}

	Entity GameLayer::SpawnBlock(const glm::vec2& center, const glm::vec2& size, const glm::vec4& color)
	{
		Entity block = m_GameScene->CreateEntity("Block");
		{
			auto& transform = block.GetComponent<TransformComponent>();
			transform.Position = { center.x, center.y, 0.0f };
			transform.Size = size;
		}
		block.AddComponent<SpriteRendererComponent>(color);
		block.AddComponent<RigidBody2DComponent>(MakeBody(RigidBody2DComponent::BodyType::Dynamic));

		auto& collider = block.AddComponent<BoxCollider2DComponent>();
		collider.Density = BLOCK_DENSITY;
		collider.Friction = BLOCK_FRICTION;
		collider.Restitution = 0.05f;
		return block;
	}

	Entity GameLayer::SpawnPig(const glm::vec2& center)
	{
		Entity pig = m_GameScene->CreateEntity("Pig");
		{
			auto& transform = pig.GetComponent<TransformComponent>();
			transform.Position = { center.x, center.y, 0.05f };
			transform.Size = { PIG_RADIUS * 2.0f, PIG_RADIUS * 2.0f };
		}
		pig.AddComponent<CircleRendererComponent>().Color = COLOR_PIG;
		pig.AddComponent<RigidBody2DComponent>(MakeBody(RigidBody2DComponent::BodyType::Dynamic));

		auto& collider = pig.AddComponent<CircleCollider2DComponent>();
		collider.Radius = 0.5f; // * Transform.Size.x => PIG_RADIUS
		collider.Density = PIG_DENSITY;
		collider.Friction = 0.6f;
		collider.Restitution = 0.1f;

		pig.AddScript<PigScript>(&m_Context);
		m_Context.PigsLeft++;
		return pig;
	}

	void GameLayer::SpawnStructure(int level)
	{
		// A simple "table": two legs + a beam, with a pig perched on top. The legs
		// rest on the ground (a small gap lets the solver settle them cleanly).
		const auto buildTable = [&](float cx)
		{
			const glm::vec2 legSize{ 0.6f, 2.4f };
			const glm::vec2 beamSize{ 3.4f, 0.5f };
			const float legY = GROUND_TOP_Y + legSize.y * 0.5f + 0.02f;
			const float beamY = GROUND_TOP_Y + legSize.y + beamSize.y * 0.5f + 0.04f;

			SpawnBlock({ cx - 1.3f, legY }, legSize, COLOR_WOOD);
			SpawnBlock({ cx + 1.3f, legY }, legSize, COLOR_WOOD);
			SpawnBlock({ cx, beamY }, beamSize, COLOR_WOOD_2);
			SpawnPig({ cx, beamY + beamSize.y * 0.5f + PIG_RADIUS + 0.05f });
		};

		const float pigGround = GROUND_TOP_Y + PIG_RADIUS + 0.05f;

		if (level <= 1)
		{
			buildTable(6.0f);
			SpawnPig({ 11.5f, pigGround });
		}
		else
		{
			buildTable(4.5f);
			buildTable(10.0f);
			SpawnPig({ 7.25f, pigGround });

			// A little stack of crates as extra cover on the far right.
			SpawnBlock({ 13.5f, GROUND_TOP_Y + 0.65f, }, { 1.3f, 1.3f }, COLOR_WOOD);
			SpawnBlock({ 13.5f, GROUND_TOP_Y + 1.98f }, { 1.3f, 1.3f }, COLOR_WOOD_2);
		}
	}

	void GameLayer::SpawnWaitingBird()
	{
		m_Bird = m_GameScene->CreateEntity("Bird");
		auto& transform = m_Bird.GetComponent<TransformComponent>();
		transform.Position = { m_SlingPos.x, m_SlingPos.y, 0.1f };
		transform.Size = { BIRD_RADIUS * 2.0f, BIRD_RADIUS * 2.0f };

		m_Bird.AddComponent<CircleRendererComponent>().Color = COLOR_BIRD_WAIT;

		m_Aiming = true;
		m_Dragging = false;
		m_BirdInFlight = false;
		m_LaunchVelocity = glm::vec2(0.0f);
	}

	// ----------------------------------------------------------------------
	// Per-frame game logic
	// ----------------------------------------------------------------------

	void GameLayer::UpdateGame(float deltaTime)
	{
		if (m_Aiming)
			HandleAiming();

		// Steps the physics world and runs the pig behaviours (which pop on impact).
		m_GameScene->OnUpdate(deltaTime);

		// Level cleared?
		if (m_Context.PigsLeft <= 0)
		{
			if (m_Context.Level >= LEVEL_COUNT)
			{
				m_Context.Score += m_Context.BirdsLeft * BIRD_BONUS; // unused-bird bonus
				EndGame(true);
			}
			else
			{
				StartLevel(m_Context.Level + 1);
			}
			return;
		}

		if (m_BirdInFlight)
		{
			m_FlightTimer += deltaTime;

			const glm::vec3 position = m_Bird.IsValid() ? m_Bird.GetComponent<TransformComponent>().Position : glm::vec3(0.0f);
			const float speed = glm::length(m_GameScene->GetLinearVelocity(m_Bird));

			if (speed < SETTLE_SPEED)
				m_SettleTimer += deltaTime;
			else
				m_SettleTimer = 0.0f;

			const bool offWorld = position.y < KILL_Y
				|| position.x > m_Context.HalfWidth + 2.0f
				|| position.x < -m_Context.HalfWidth - 2.0f;

			if (m_SettleTimer > SETTLE_TIME || m_FlightTimer > FLIGHT_TIMEOUT || offWorld)
				ResolveBird();
		}

		UpdateHud();
	}

	glm::vec2 GameLayer::ScreenToWorld(const glm::vec2& screen) const
	{
		// The camera is a centered, symmetric ortho projection, so NDC maps straight
		// onto the world half-extents (screen Y is flipped — pixels grow downward).
		const float width = (float)Application::Get().GetWindow().GetWidth();
		const float height = (float)Application::Get().GetWindow().GetHeight();
		const float ndcX = (screen.x / width) * 2.0f - 1.0f;
		const float ndcY = 1.0f - (screen.y / height) * 2.0f;
		return { ndcX * m_Context.HalfWidth, ndcY * m_Context.HalfHeight };
	}

	void GameLayer::HandleAiming()
	{
		if (!m_Bird.IsValid())
			return;

		const glm::vec2 mouse = ScreenToWorld(Input::GetMousePosition());

		// A drag begins only if the press starts on/near the resting bird.
		if (!m_Dragging && Input::IsMouseButtonDown(MouseButton::Left))
		{
			if (glm::distance(mouse, m_SlingPos) <= GRAB_RADIUS)
				m_Dragging = true;
		}

		if (!m_Dragging)
			return;

		// Pull the bird back from the slingshot, clamped to MAX_PULL.
		glm::vec2 pull = mouse - m_SlingPos;
		float pullLength = glm::length(pull);
		if (pullLength > MAX_PULL)
		{
			pull *= MAX_PULL / pullLength;
			pullLength = MAX_PULL;
		}

		// Keep the pulled-back bird inside the play field. The slingshot sits close to
		// the left containment wall and not far above the ground, so a full pull -
		// especially down-left, which is exactly when the cursor leaves the window -
		// would otherwise drag the bird past the wall or below the ground. The launched
		// body (built from this transform) would then spawn inside static geometry and
		// be blocked or flung off-world instead of into play. Velocity is still derived
		// from the full pull below, so aim and power are unchanged; only the spawn
		// position is bounded.
		glm::vec2 aimPosition = m_SlingPos + pull;
		aimPosition.x = std::clamp(aimPosition.x, -m_Context.HalfWidth + BIRD_RADIUS + 0.1f, m_Context.HalfWidth - BIRD_RADIUS - 0.1f);
		aimPosition.y = std::max(aimPosition.y, GROUND_TOP_Y + BIRD_RADIUS + 0.1f);
		m_Bird.GetComponent<TransformComponent>().Position = { aimPosition.x, aimPosition.y, 0.1f };

		// Launch is opposite the pull; speed scales with how far it was pulled.
		const float speed = (pullLength / MAX_PULL) * MAX_LAUNCH_SPEED;
		m_LaunchVelocity = (pullLength > 0.0001f) ? (-pull / pullLength) * speed : glm::vec2(0.0f);

		// Release the button to fire. A tiny pull just cancels and resets the bird.
		if (!Input::IsMouseButtonPressed(MouseButton::Left))
		{
			if (pullLength >= PULL_DEADZONE)
			{
				LaunchBird(m_LaunchVelocity);
			}
			else
			{
				m_Bird.GetComponent<TransformComponent>().Position = { m_SlingPos.x, m_SlingPos.y, 0.1f };
				m_Dragging = false;
				m_LaunchVelocity = glm::vec2(0.0f);
			}
		}
	}

	void GameLayer::LaunchBird(const glm::vec2& velocity)
	{
		if (!m_Bird.IsValid())
			return;

		// Give the bird a dynamic body at its current (pulled-back) position, then
		// fling it. CreateRigidBody builds the Box2D body from the bird's transform.
		m_Bird.AddComponent<RigidBody2DComponent>(MakeBody(RigidBody2DComponent::BodyType::Dynamic));

		auto& collider = m_Bird.AddComponent<CircleCollider2DComponent>();
		collider.Radius = 0.5f;
		collider.Density = BIRD_DENSITY;
		collider.Friction = BIRD_FRICTION;
		collider.Restitution = BIRD_RESTITUTION;

		m_Bird.GetComponent<CircleRendererComponent>().Color = COLOR_BIRD;

		m_GameScene->CreateRigidBody(m_Bird);
		m_GameScene->SetLinearVelocity(m_Bird, velocity);

		m_Aiming = false;
		m_Dragging = false;
		m_BirdInFlight = true;
		m_FlightTimer = 0.0f;
		m_SettleTimer = 0.0f;
	}

	void GameLayer::ResolveBird()
	{
		if (m_Bird.IsValid())
			m_Bird.Destroy();
		m_Bird = {};

		m_BirdInFlight = false;
		m_Context.BirdsLeft = std::max(0, m_Context.BirdsLeft - 1);

		if (m_Context.BirdsLeft <= 0)
		{
			EndGame(false); // out of birds with pigs still standing
			return;
		}

		SpawnWaitingBird();
	}

	void GameLayer::UpdateHud()
	{
		if (m_ScoreText.IsValid())
			m_ScoreText.GetComponent<TextComponent>().Text = std::format("SCORE {}", m_Context.Score);
		if (m_LevelText.IsValid())
			m_LevelText.GetComponent<TextComponent>().Text = std::format("LEVEL {}", m_Context.Level);
		if (m_BirdsText.IsValid())
			m_BirdsText.GetComponent<TextComponent>().Text = std::format("BIRDS {}", m_Context.BirdsLeft);
		if (m_PigsText.IsValid())
			m_PigsText.GetComponent<TextComponent>().Text = std::format("PIGS {}", m_Context.PigsLeft);
	}

	// ----------------------------------------------------------------------
	// Rendering
	// ----------------------------------------------------------------------

	void GameLayer::RenderGame()
	{
		Renderer2D& renderer = Application::Get().GetRenderer2D();

		renderer.BeginScene(m_ViewProjection);
		renderer.Clear(m_GameScene->GetClearColor());

		// Slingshot post (drawn behind the bird/structure quads).
		renderer.DrawQuad(
			glm::vec2(m_SlingPos.x, (GROUND_TOP_Y + m_SlingPos.y) * 0.5f),
			glm::vec2(0.3f, m_SlingPos.y - GROUND_TOP_Y),
			COLOR_SLING);

		m_GameScene->RenderEntities(renderer);

		// Aiming overlay: the slingshot band + trajectory preview, shown while dragging.
		if (m_Aiming && m_Dragging && m_Bird.IsValid())
		{
			const glm::vec3 birdPos = m_Bird.GetComponent<TransformComponent>().Position;
			const glm::vec2 start = { birdPos.x, birdPos.y };

			// Rubber band: a few dots from the sling fork to the pulled-back bird.
			for (int i = 1; i <= 5; i++)
			{
				const glm::vec2 p = glm::mix(m_SlingPos, start, i / 6.0f);
				renderer.DrawQuad(p, glm::vec2(BAND_DOT_SIZE), COLOR_SLING);
			}

			// Trajectory preview launched from the bird's pulled-back position.
			for (int i = 1; i <= AIM_DOTS; i++)
			{
				const float t = i * AIM_DOT_STEP;
				const glm::vec2 p = start + m_LaunchVelocity * t + 0.5f * GRAVITY * (t * t);
				if (p.y < GROUND_TOP_Y)
					break;

				const float alpha = COLOR_AIM.a * (1.0f - (float)i / (AIM_DOTS + 1));
				renderer.DrawQuad(p, glm::vec2(AIM_DOT_SIZE), { COLOR_AIM.r, COLOR_AIM.g, COLOR_AIM.b, alpha });
			}
		}

		renderer.EndScene();
	}

}
