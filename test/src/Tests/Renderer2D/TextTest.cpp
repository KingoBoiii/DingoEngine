#include "TextTest.h"

namespace Dingo
{

	void TextTest::Initialize()
	{
		m_ArialFont = Font::Create("C:/Windows/Fonts/Arial.ttf");
	}

	void TextTest::Cleanup()
	{
		if (m_ArialFont)
		{
			m_ArialFont->Destroy();
			m_ArialFont = nullptr;
		}
	}

	void TextTest::Update(float deltaTime)
	{
		m_Renderer->BeginScene(m_ProjectionViewMatrix);
		m_Renderer->Clear(m_ClearColor);
		m_Renderer->DrawText("Hello, World!", m_ArialFont, glm::vec2(-2.5f, 0.0f));
		m_Renderer->EndScene();
	}

}
