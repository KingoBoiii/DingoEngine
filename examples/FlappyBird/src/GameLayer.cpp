#include "GameLayer.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath> // For std::fmod

#define DEFAULT_FONT_SIZE 0.5f
#define DEFAULT_TITLE_FONT_SIZE 0.9f

namespace Dingo
{

	void GameLayer::OnAttach()
	{
		float aspectRatio = (float)Application::Get().GetWindow().GetWidth() / (float)Application::Get().GetWindow().GetHeight();

		float orthoLeft = -m_OrthographicSize * aspectRatio * 0.5f;
		float orthoRight = m_OrthographicSize * aspectRatio * 0.5f;
		float orthoBottom = -m_OrthographicSize * 0.5f;
		float orthoTop = m_OrthographicSize * 0.5f;

		m_ProjectionViewMatrix = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, m_OrthographicNear, m_OrthographicFar);

		m_MenuTexture = Texture::CreateFromFile("assets/sprites/message.png");

		m_BackgroundTexture = Texture::CreateFromFile("assets/sprites/background-day.png");
		m_GroundTexture = Texture::CreateFromFile("assets/sprites/base.png");
		m_PipeTexture = Texture::CreateFromFile("assets/sprites/pipe-green.png");
		m_BirdTexture = Texture::CreateFromFile("assets/sprites/yellowbird-midflap.png");

		m_Font = Font::Create("assets/fonts/arialbd.ttf");

		// Ground aspect ratio
		float groundAspectRatio = (float)m_GroundTexture->GetWidth() / (float)m_GroundTexture->GetHeight();
		m_GroundQuadWidth = m_GroundHeight * groundAspectRatio;

		m_BirdY = -0.88f;
		m_BirdVelocity = 0.0f;

		// Bird aspect ratio
		float birdAspectRatio = (float)m_BirdTexture->GetWidth() / (float)m_BirdTexture->GetHeight();
		m_BirdHeight = 0.45f; // desired height in world units
		m_BirdWidth = m_BirdHeight * birdAspectRatio;
	}

	void GameLayer::OnDetach()
	{
		// Clean up font
		if (m_Font)
		{
			m_Font->Destroy();
			m_Font = nullptr;
		}

		// Clean up textures
		if (m_BirdTexture)
		{
			m_BirdTexture->Destroy();
			m_BirdTexture = nullptr;
		}

		if (m_PipeTexture)
		{
			m_PipeTexture->Destroy();
			m_PipeTexture = nullptr;
		}

		if (m_GroundTexture)
		{
			m_GroundTexture->Destroy();
			m_GroundTexture = nullptr;
		}

		if (m_BackgroundTexture)
		{
			m_BackgroundTexture->Destroy();
			m_BackgroundTexture = nullptr;
		}

		if (m_MenuTexture)
		{
			m_MenuTexture->Destroy();
			m_MenuTexture = nullptr;
		}
	}

	void GameLayer::OnUpdate(float deltaTime)
	{
		if (Input::IsKeyPressed(Key::Escape))
		{
			Application::Get().Close();
		}

		Renderer2D& renderer = Application::Get().GetRenderer2D();

		m_AspectRatio = (float)Application::Get().GetWindow().GetWidth() / (float)Application::Get().GetWindow().GetHeight();
		m_Width = m_OrthographicSize * m_AspectRatio;
		m_Height = m_OrthographicSize;

		m_GroundY = -m_Height * 0.5f + m_GroundHeight * 0.5f;

		renderer.BeginScene(m_ProjectionViewMatrix);
		renderer.Clear(glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));

		UpdateScrollingBackground(deltaTime, renderer);

		RenderBird(renderer);
		RenderPipes(renderer);
		RenderGround(renderer);

		switch (m_GameState)
		{
			case GameState::Menu:
				UpdateGameStateMenu(deltaTime, renderer);
				break;
			case GameState::Game:
				UpdateGameStateGame(deltaTime, renderer);
				break;
			case GameState::Dead:
				UpdateGameStateDead(deltaTime, renderer);
				break;
		}

		renderer.EndScene();
	}

	void GameLayer::UpdateGameStateMenu(float deltaTime, Renderer2D& renderer)
	{
		if(Input::IsKeyPressed(Key::Space))
		{
			m_GameState = GameState::Game;
			m_Score = 0; // Reset score when starting the game
			m_Pipes.clear(); // Clear any existing pipes
			m_BackgroundOffset = 0.0f; // Reset background offset
			m_BirdY = 0;
		}

		m_BackgroundScrollSpeed = 0.0f;

		RenderCenteredText(renderer, "Flappy Bird", DEFAULT_TITLE_FONT_SIZE, glm::vec2(0.0f, m_Height * 0.5f - 0.8f));
		RenderCenteredText(renderer, "Press space to flap", DEFAULT_FONT_SIZE);
	}

	void GameLayer::UpdateGameStateGame(float deltaTime, Renderer2D& renderer)
	{
		m_BackgroundScrollSpeed = BACKGROUND_SCROLLING_SPEED;

		UpdateBird(deltaTime);

		UpdateGround(deltaTime);
		UpdatePipes(deltaTime, renderer);

		// Bird's horizontal position (assuming center of screen)
		float birdX = 0.0f;

		// Scoring: when pipe passes bird and hasn't been scored yet
		for (auto& pipe : m_Pipes)
		{
			if (!pipe.scored && pipe.x + m_PipeWidth * 0.5f < birdX - m_BirdHeight * 0.5f)
			{
				pipe.scored = true;
				m_Score++;
			}
		}

		// --- Collision detection with pipes ---
		float birdLeft = birdX - m_BirdWidth * 0.5f;
		float birdRight = birdX + m_BirdWidth * 0.5f;
		float birdTop = m_BirdY + m_BirdHeight * 0.5f;
		float birdBottom = m_BirdY - m_BirdHeight * 0.5f;

		for (const auto& pipe : m_Pipes)
		{
			float pipeLeft = pipe.x - m_PipeWidth * 0.5f;
			float pipeRight = pipe.x + m_PipeWidth * 0.5f;

			float gapTop = pipe.gapY + m_PipeGapHeight * 0.5f;
			float gapBottom = pipe.gapY - m_PipeGapHeight * 0.5f;

			// Check horizontal overlap
			bool overlapsX = birdRight > pipeLeft && birdLeft < pipeRight;

			// Check vertical overlap with top pipe
			bool hitsTopPipe = birdTop > gapTop;
			// Check vertical overlap with bottom pipe
			bool hitsBottomPipe = birdBottom < gapBottom;

			if (overlapsX && (hitsTopPipe || hitsBottomPipe))
			{
				m_GameState = GameState::Dead;
				break;
			}
		}

		RenderCenteredText(renderer, std::to_string(m_Score), DEFAULT_TITLE_FONT_SIZE, glm::vec2(0.0f, m_Height * 0.5f - 0.8f));
	}

	void GameLayer::UpdateGameStateDead(float deltaTime, Renderer2D& renderer)
	{
		if (Input::IsKeyPressed(Key::Space))
		{
			m_GameState = GameState::Game;
			m_Score = 0; // Reset score when starting the game
			m_Pipes.clear(); // Clear any existing pipes
			m_BackgroundOffset = 0.0f; // Reset background offset
			m_BirdY = 0;
		}

		m_BackgroundScrollSpeed = 0.0f;

		RenderCenteredText(renderer, "Game over", DEFAULT_TITLE_FONT_SIZE, glm::vec2(0.0f, m_Height * 0.5f - 0.8f));
		RenderCenteredText(renderer, "Press space to retry", DEFAULT_FONT_SIZE);
	}

	void GameLayer::UpdateScrollingBackground(float deltaTime, Renderer2D& renderer)
	{
		// --- Parallax background update ---
		m_BackgroundOffset -= m_BackgroundScrollSpeed * deltaTime;

		// Calculate background quad width based on texture aspect ratio
		float bgAspectRatio = (float)m_BackgroundTexture->GetWidth() / (float)m_BackgroundTexture->GetHeight();
		float bgQuadWidth = m_Height * bgAspectRatio;

		// Wrap offset to tile seamlessly (handles both positive and negative)
		m_BackgroundOffset = std::fmod(m_BackgroundOffset, bgQuadWidth);
		if (m_BackgroundOffset < 0.0f)
		{
			m_BackgroundOffset += bgQuadWidth;
		}

		// Draw background as seamless tiles
		for (float x = -m_Width * 0.5f - bgQuadWidth; x < m_Width * 0.5f + bgQuadWidth; x += bgQuadWidth)
		{
			renderer.DrawQuad(glm::vec2(x + m_BackgroundOffset, 0.0f), glm::vec2(bgQuadWidth, m_Height), m_BackgroundTexture);
		}
	}

	void GameLayer::UpdatePipes(float deltaTime, Renderer2D& renderer)
	{
		// --- Pipe logic ---
		m_PipeSpawnTimer -= deltaTime;
		if (m_PipeSpawnTimer <= 0.0f)
		{
			Pipe pipe;
			pipe.x = m_Width * 0.5f + m_PipeWidth; // spawn just off the right edge
			pipe.gapHeight = m_PipeGapHeight;

			// Ensure the gap is always above the ground and below the top
			float margin = 0.5f; // extra margin from ground and top
			float minGapY = -m_Height * 0.5f + m_GroundHeight + pipe.gapHeight * 0.5f + margin;
			float maxGapY = m_Height * 0.5f - pipe.gapHeight * 0.5f - margin;
			pipe.gapY = minGapY + static_cast<float>(rand()) / RAND_MAX * (maxGapY - minGapY);

			m_Pipes.push_back(pipe);
			m_PipeSpawnTimer = m_PipeSpawnInterval;
		}

		// Move pipes and remove off-screen
		for (auto& pipe : m_Pipes)
		{
			pipe.x -= m_PipeSpeed * deltaTime;
		}
		while (!m_Pipes.empty() && m_Pipes.front().x < -m_Width * 0.5f - m_PipeWidth)
		{
			m_Pipes.erase(m_Pipes.begin());
		}
	}

	void GameLayer::RenderPipes(Renderer2D& renderer)
	{
		float pipeWidth = m_Width * 0.1f;   // 10% of screen width
		float pipeHeight = m_Height * 0.75f; // 75% of screen height

		// Render pipes
		for (const auto& pipe : m_Pipes)
		{
			// Top pipe: center at (pipe.x, topPipeCenterY)
			float topPipeCenterY = pipe.gapY + m_PipeGapHeight * 0.5f + pipeHeight * 0.5f;
			renderer.DrawQuad(glm::vec2(pipe.x, topPipeCenterY), glm::vec2(pipeWidth, pipeHeight), m_PipeTexture);

			// Bottom pipe: center at (pipe.x, bottomPipeCenterY)
			float bottomPipeCenterY = pipe.gapY - m_PipeGapHeight * 0.5f - pipeHeight * 0.5f;
			renderer.DrawQuad(glm::vec2(pipe.x, bottomPipeCenterY), glm::vec2(pipeWidth, pipeHeight), m_PipeTexture);
		}
	}

	void GameLayer::UpdateBird(float deltaTime)
	{
		// --- Bird physics update ---
		m_BirdVelocity += m_Gravity * deltaTime; // Apply gravity
		m_BirdY += m_BirdVelocity * deltaTime;   // Update position

		// Handle jump input (spacebar)
		if (Input::IsKeyPressed(Key::Space))
		{
			m_BirdVelocity = m_JumpVelocity;
		}

		// --- Ground collision detection ---
		float groundTop = m_GroundY + m_GroundHeight * 0.5f;
		float birdBottom = m_BirdY - m_BirdHeight * 0.5f;

		if (birdBottom <= groundTop)
		{
			m_BirdY = groundTop + m_BirdHeight * 0.5f;
			m_BirdVelocity = 0.0f;
			m_GameState = GameState::Dead; // Switch to dead state if bird hits the ground
		}
	}

	void GameLayer::RenderBird(Renderer2D& renderer)
	{
		renderer.DrawQuad(glm::vec2(0.0f, m_BirdY), glm::vec2(m_BirdWidth, m_BirdHeight), m_BirdTexture);
	}

	void GameLayer::UpdateGround(float deltaTime)
	{
		m_GroundOffset -= m_PipeSpeed * deltaTime;
	}

	void GameLayer::RenderGround(Renderer2D& renderer)
	{
		// --- Ground rendering ---

		// Wrap offset to tile seamlessly
		m_GroundOffset = std::fmod(m_GroundOffset, m_GroundQuadWidth);
		if (m_GroundOffset < 0.0f)
		{
			m_GroundOffset += m_GroundQuadWidth;
		}

		// Tile the ground texture horizontally, using ground offset
		for (float x = -m_Width * 0.5f - m_GroundQuadWidth; x < m_Width * 0.5f + m_GroundQuadWidth; x += m_GroundQuadWidth)
		{
			renderer.DrawQuad(glm::vec2(x + m_GroundOffset, m_GroundY), glm::vec2(m_GroundQuadWidth, m_GroundHeight), m_GroundTexture);
		}
	}

	void GameLayer::RenderCenteredText(Renderer2D& renderer, const std::string& text, float fontSize, const glm::vec2& offset) const
	{
		float textWidth = m_Font->GetStringWidth(text, fontSize);

		renderer.DrawText(text, m_Font, glm::vec2(-(textWidth * 0.5f) + offset.x, offset.y), fontSize);
	}

}
