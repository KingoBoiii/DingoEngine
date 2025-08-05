#include "TextureQuadTest.h"

namespace Dingo
{

	void TextureQuadTest::Initialize()
	{
		Renderer2DTest::Initialize();

		m_Texture = Texture::CreateFromFile("assets/textures/container.jpg", "Wooden container");
		m_HD2Texture = Texture::CreateFromFile("assets/textures/hd2.png", "HD2 Icon");
	}

	void TextureQuadTest::Cleanup()
	{
		if (m_Texture)
		{
			m_Texture->Destroy();
			m_Texture = nullptr;
		}

		if (m_HD2Texture)
		{
			m_HD2Texture->Destroy();
			m_HD2Texture = nullptr;
		}

		Renderer2DTest::Cleanup();
	}

	void TextureQuadTest::Update(float deltaTime)
	{
		m_Renderer->BeginScene(m_ProjectionViewMatrix);
		m_Renderer->Clear(m_ClearColor);
		m_Renderer->DrawQuad({ -1.55f, 0.0f }, { 2.0f, 2.0f }, glm::vec4(1.0f));
		m_Renderer->DrawQuad({ 1.55f, 0.0f }, { 2.0f, 2.0f }, m_Texture);
		m_Renderer->DrawQuad({ 0.0f, 1.55f }, { 2.0f, 2.0f }, m_HD2Texture);
		m_Renderer->EndScene();
	}

}
