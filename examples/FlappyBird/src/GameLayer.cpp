#include "GameLayer.h"

#include <glm/gtc/matrix_transform.hpp>

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

		m_BackgroundTexture = Texture::CreateFromFile("assets/sprites/background-day.png");
		m_BirdTexture = Texture::CreateFromFile("assets/sprites/yellowbird-midflap.png");

		m_BirdY = 0.0f;
		m_BirdVelocity = 0.0f; 
	}

	void GameLayer::OnDetach()
	{
		if (m_BirdTexture)
		{
			delete m_BirdTexture;
			m_BirdTexture = nullptr;
		}

		if (m_BackgroundTexture)
		{
			delete m_BackgroundTexture;
			m_BackgroundTexture = nullptr;
		}
	}

	void GameLayer::OnUpdate(float deltaTime)
	{
		float aspectRatio = (float)Application::Get().GetWindow().GetWidth() / (float)Application::Get().GetWindow().GetHeight();
		//float sizeX = (float)Application::Get().GetWindow().GetWidth() / m_BackgroundTexture->GetWidth();
		float width = m_OrthographicSize * aspectRatio;
		float height = m_OrthographicSize;

		// --- Bird physics update ---
		m_BirdVelocity += m_Gravity * deltaTime; // Apply gravity
		m_BirdY += m_BirdVelocity * deltaTime;   // Update position

		// Handle jump input (spacebar)
		if (Input::IsKeyPressed(Key::Space) && m_BirdVelocity <= 0)
		{
			m_BirdVelocity = m_JumpVelocity;
		}

		// Optional: Clamp bird to bottom of screen
		float minY = -height * 0.5f + 0.25f; // Adjust for bird size
		if (m_BirdY < minY)
		{
			m_BirdY = minY;
			m_BirdVelocity = 0.0f;
		}

		Renderer2D& renderer = Application::Get().GetRenderer2D();

		renderer.BeginScene(m_ProjectionViewMatrix);
		renderer.Clear(glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
		renderer.DrawQuad(glm::vec2(0.0f, 0.0f), glm::vec2(width, height), m_BackgroundTexture);

		// Bird aspect ratio
		float birdAspectRatio = (float)m_BirdTexture->GetWidth() / (float)m_BirdTexture->GetHeight();
		float birdHeight = 0.5f; // desired height in world units
		float birdWidth = birdHeight * birdAspectRatio;

		renderer.DrawQuad(glm::vec2(0.0f, m_BirdY), glm::vec2(birdWidth, birdHeight), m_BirdTexture);

		renderer.EndScene();
	}

#if DE_DEBUG
	void GameLayer::OnImGuiRender()
	{}
#endif

}
