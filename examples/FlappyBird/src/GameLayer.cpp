#include "GameLayer.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath> // For std::fmod

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

		m_BirdY = 0.0f;
		m_BirdVelocity = 0.0f;
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

		renderer.BeginScene(m_ProjectionViewMatrix);
		renderer.Clear(glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));

		UpdateScrollingBackground(deltaTime, renderer);

		switch(m_GameState)
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

		// --- Ground rendering ---
		float groundY = -m_Height * 0.5f + m_GroundHeight * 0.5f; // Position at bottom of screen

		// --- Ground offset update ---
		m_GroundOffset -= m_PipeSpeed * deltaTime;

		// Calculate ground quad width based on texture aspect ratio
		float groundAspectRatio = (float)m_GroundTexture->GetWidth() / (float)m_GroundTexture->GetHeight();
		float groundQuadWidth = m_GroundHeight * groundAspectRatio;

		// Wrap offset to tile seamlessly
		m_GroundOffset = std::fmod(m_GroundOffset, groundQuadWidth);
		if (m_GroundOffset < 0.0f)
		{
			m_GroundOffset += groundQuadWidth;
		}

		// Tile the ground texture horizontally, using ground offset
		for (float x = -m_Width * 0.5f - groundQuadWidth; x < m_Width * 0.5f + groundQuadWidth; x += groundQuadWidth)
		{
			renderer.DrawQuad(glm::vec2(x + m_GroundOffset, groundY), glm::vec2(groundQuadWidth, m_GroundHeight), m_GroundTexture);
		}

		// --- Bird physics update ---
		m_BirdVelocity += m_Gravity * deltaTime; // Apply gravity
		m_BirdY += m_BirdVelocity * deltaTime;   // Update position

		// Handle jump input (spacebar)
		if (Input::IsKeyPressed(Key::Space))
		{
			m_BirdVelocity = m_JumpVelocity;
		}

		// Bird aspect ratio
		float birdAspectRatio = (float)m_BirdTexture->GetWidth() / (float)m_BirdTexture->GetHeight();
		float birdHeight = 0.5f; // desired height in world units
		float birdWidth = birdHeight * birdAspectRatio;

		// --- Ground collision detection ---
		float groundTop = groundY + m_GroundHeight * 0.5f;
		float birdBottom = m_BirdY - birdHeight * 0.5f;

		if (birdBottom <= groundTop)
		{
			m_BirdY = groundTop + birdHeight * 0.5f;
			m_BirdVelocity = 0.0f;
		}

		renderer.DrawQuad(glm::vec2(0.0f, m_BirdY), glm::vec2(birdWidth, birdHeight), m_BirdTexture);

		// Bird's horizontal position (assuming center of screen)
		float birdX = 0.0f;

		// Scoring: when pipe passes bird and hasn't been scored yet
		for (auto& pipe : m_Pipes)
		{
			if (!pipe.scored && pipe.x + pipeWidth * 0.5f < birdX - birdWidth * 0.5f)
			{
				pipe.scored = true;
				m_Score++;
			}
		}

		renderer.DrawText(std::to_string(m_Score), m_Font, glm::vec2(0.0f, m_Height * 0.5f - 0.8f));

		renderer.EndScene();
	}

	void GameLayer::UpdateGameStateMenu(float deltaTime, Renderer2D& renderer)
	{
		// --- Menu state rendering ---
		float aspectRatio = (float)m_MenuTexture->GetWidth() / (float)m_MenuTexture->GetHeight();
		float menuWidth = m_Height * aspectRatio; // Scale to fit height
		renderer.DrawQuad(glm::vec2(0.0f, 0.0f), glm::vec2(menuWidth, m_Height), m_MenuTexture);
	}

	void GameLayer::UpdateGameStateGame(float deltaTime, Renderer2D& renderer)
	{}

	void GameLayer::UpdateGameStateDead(float deltaTime, Renderer2D& renderer)
	{}

	void GameLayer::UpdateScrollingBackground(float deltaTime, Renderer2D & renderer)
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

}
