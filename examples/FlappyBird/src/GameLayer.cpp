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
	}

	void GameLayer::OnDetach()
	{
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

		Renderer2D& renderer = Application::Get().GetRenderer2D();

		renderer.BeginScene(m_ProjectionViewMatrix);
		renderer.Clear(glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
		renderer.DrawQuad(glm::vec2(0.0f, 0.0f), glm::vec2(width, height), m_BackgroundTexture);
		renderer.EndScene();
	}

#if DE_DEBUG
	void GameLayer::OnImGuiRender()
	{}
#endif

}
