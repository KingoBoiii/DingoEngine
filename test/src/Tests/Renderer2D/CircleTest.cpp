#include "CircleTest.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Dingo
{

	void CircleTest::Update(float deltaTime)
	{
		static glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), { 1.0f, 1.0f, 1.0f });

		m_Renderer->BeginScene(m_ProjectionViewMatrix);
		m_Renderer->Clear(m_ClearColor);
		m_Renderer->DrawQuad({ -1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 1.0f, 1.0f });
		m_Renderer->DrawCircle(transform, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
		m_Renderer->EndScene();
	}

}
