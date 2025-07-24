#include "QuadTest.h"

namespace Dingo
{

	void QuadTest::Initialize()
	{
		m_Renderer = new Renderer2D(Renderer2DParams{});
		m_Renderer->Initialize();
	}

	void QuadTest::Update(float deltaTime)
	{
		m_Renderer->Begin2D();
		m_Renderer->Clear({ 0.2f, 0.3f, 0.3f, 1.0f });
		m_Renderer->DrawQuad({ -0.5f, -0.5f }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f });
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
		Application::Get().SubmitPostExecution([this, width, height]()
		{
			m_Renderer->Resize(width, height);
		});
	}

}
