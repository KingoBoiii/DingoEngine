#include "TextTest.h"

namespace Dingo
{

	void TextTest::Initialize()
	{
		m_ArialFont = Font::Create("assets/fonts/ArialBD.ttf");
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
		m_Renderer->DrawQuad(glm::vec2(0.0f, 1.0f), glm::vec2(3.0f, 3.0f), m_ArialFont->GetAtlasTexture());
		m_Renderer->DrawText("Hello, World!", m_ArialFont, glm::vec2(-2.75f, 0.0f));
		// Escaped because this project does not compile with /utf-8. Line 1 is UTF-8 Latin-1,
		// dashes, curly quotes, ellipsis and euro; line 2 a raw Latin-1 byte and an unbaked CJK
		// codepoint, which draw as "e-acute" and '?'.
		m_Renderer->DrawText("Caf" "\xC3\xA9 " "\xE2\x80\x94 " "\xE2\x80\x9C" "quoted" "\xE2\x80\x9D" "\xE2\x80\xA6 5 " "\xE2\x82\xAC", m_ArialFont, glm::vec2(-2.75f, -0.9f), 0.5f);
		m_Renderer->DrawText("Latin-1 byte: " "\xE9" "  CJK: " "\xE4\xB8\xAD", m_ArialFont, glm::vec2(-2.75f, -1.5f), 0.5f);
		m_Renderer->EndScene();
	}

}
