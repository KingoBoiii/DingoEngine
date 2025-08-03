#include "TextureQuadTest.h"

namespace Dingo
{

	void TextureQuadTest::Initialize()
	{
		Renderer2DTest::Initialize();

		uint32_t width, height, channels;
		const uint8_t* textureData = Dingo::FileSystem::ReadImage("assets/textures/container.jpg", &width, &height, &channels, true, true);

		Dingo::TextureParams textureParams = {
			.DebugName = "Wooden Container",
			.Width = width,
			.Height = height,
			.Format = channels == 4 ? Dingo::TextureFormat::RGBA : Dingo::TextureFormat::RGB,
			.Dimension = Dingo::TextureDimension::Texture2D,
		};

		m_Texture = Dingo::Texture::Create(textureParams);
		m_Texture->Initialize();
		m_Texture->Upload(textureData, width * channels);
	}

	void TextureQuadTest::Cleanup()
	{
		if (m_Texture)
		{
			m_Texture->Destroy();
			m_Texture = nullptr;
		}

		Renderer2DTest::Cleanup();
	}

	void TextureQuadTest::Update(float deltaTime)
	{
		m_Renderer->BeginScene(m_ProjectionViewMatrix);
		m_Renderer->Clear(m_ClearColor);
		m_Renderer->DrawQuad({ -1.0f, 0.0f }, { 0.95f, 0.95f }, { 1.0f, 0.0f, 0.0f, 1.0f });
		m_Renderer->DrawQuad({ 0.0f, 0.0f }, { 0.95f, 0.95f }, { 0.0f, 1.0f, 0.0f, 1.0f });
		m_Renderer->DrawQuad({ 1.0f, 0.0f }, { 0.95f, 0.95f }, { 0.0f, 0.0f, 1.0f, 1.0f });
		m_Renderer->DrawQuad({ 0.0f, 1.0f }, { 0.95f, 0.95f }, m_Texture);
		m_Renderer->EndScene();
	}

}
