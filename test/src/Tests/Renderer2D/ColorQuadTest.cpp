#include "ColorQuadTest.h"

namespace Dingo
{

	void ColorQuadTest::Update(float deltaTime)
	{
		m_Renderer->BeginScene(m_ProjectionViewMatrix);
		m_Renderer->Clear(m_ClearColor);
		m_Renderer->DrawQuad({ -1.0f, 0.0f }, { 0.95f, 0.95f }, { 1.0f, 0.0f, 0.0f, 1.0f });
		m_Renderer->DrawQuad({ 0.0f, 0.0f }, { 0.95f, 0.95f }, { 0.0f, 1.0f, 0.0f, 1.0f });
		m_Renderer->DrawQuad({ 1.0f, 0.0f }, { 0.95f, 0.95f }, { 0.0f, 0.0f, 1.0f, 1.0f });
		m_Renderer->DrawQuad({ 0.0f, 1.0f }, { 0.95f, 0.95f }, { 1.0f, 1.0f, 1.0f, 0.5f });
		m_Renderer->EndScene();
	}

}
