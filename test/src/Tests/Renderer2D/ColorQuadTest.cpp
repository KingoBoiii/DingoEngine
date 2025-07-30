#include "ColorQuadTest.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>

namespace Dingo
{

	void ColorQuadTest::Update(float deltaTime)
	{
		const glm::mat4 projectionViewMatrix = glm::perspective(glm::radians(45.0f), m_Renderer->GetViewportSize().x / m_Renderer->GetViewportSize().y, 0.1f, 100.0f) * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -5.0f));

		m_Renderer->BeginScene(projectionViewMatrix);
		m_Renderer->Clear(m_ClearColor);
		m_Renderer->DrawQuad({ -1.0f, 0.0f }, { 0.95f, 0.95f }, { 1.0f, 0.0f, 0.0f, 1.0f });
		m_Renderer->DrawQuad({ 0.0f, 0.0f }, { 0.95f, 0.95f }, { 0.0f, 1.0f, 0.0f, 1.0f });
		m_Renderer->DrawQuad({ 1.0f, 0.0f }, { 0.95f, 0.95f }, { 0.0f, 0.0f, 1.0f, 1.0f });
		m_Renderer->DrawQuad({ 0.0f, 1.0f }, { 0.95f, 0.95f }, { 1.0f, 1.0f, 1.0f, 0.5f });
		m_Renderer->EndScene();
	}

}
