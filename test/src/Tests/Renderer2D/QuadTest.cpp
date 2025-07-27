#include "QuadTest.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Dingo
{

	void QuadTest::Initialize()
	{
		m_Renderer = new Renderer2D(Renderer2DParams{});
		m_Renderer->Initialize();
	}

	void QuadTest::Update(float deltaTime)
	{
		const glm::mat4 projectionViewMatrix = glm::perspective(glm::radians(45.0f), m_Renderer->GetViewportSize().x / m_Renderer->GetViewportSize().y, 0.1f, 100.0f) * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -5.0f));

		m_Renderer->Begin2D(projectionViewMatrix);
		m_Renderer->Clear({ 0.2f, 0.3f, 0.3f, 1.0f });
		m_Renderer->DrawQuad({ -1.0f, 0.0f }, { 0.95f, 0.95f }, { 1.0f, 0.0f, 0.0f, 1.0f });
		m_Renderer->DrawQuad({ 0.0f, 0.0f }, { 0.95f, 0.95f }, { 0.0f, 1.0f, 0.0f, 1.0f });
		m_Renderer->DrawQuad({ 1.0f, 0.0f }, { 0.95f, 0.95f }, { 0.0f, 0.0f, 1.0f, 1.0f });
		m_Renderer->DrawQuad({ 0.0f, 1.0f }, { 0.95f, 0.95f }, { 1.0f, 1.0f, 1.0f, 0.5f });
		m_Renderer->End2D();
	}

	void QuadTest::Cleanup()
	{
		if (m_Renderer)
		{
			m_Renderer->Shutdown();
			delete m_Renderer;
			m_Renderer = nullptr;
		}
	}

	void QuadTest::Resize(uint32_t width, uint32_t height)
	{
		if (width != m_Renderer->GetViewportSize().x || height != m_Renderer->GetViewportSize().y)
		{
			Application::Get().SubmitPostExecution([this, width, height]()
			{
				m_Renderer->Resize(width, height);
			});
		}
	}

}
