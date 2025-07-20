#include "GraphicsTest.h"

namespace Dingo
{


	void GraphicsTest::Initialize()
	{
		m_Renderer = new TestRenderer();
		m_Renderer->Initialize();

		m_AspectRatio = 800.0f / 600.0f;

		InitializeGraphics();
	}

	void GraphicsTest::Cleanup()
	{
		CleanupGraphics();

		if (m_Renderer)
		{
			m_Renderer->Shutdown();
			delete m_Renderer;
			m_Renderer = nullptr;
		}
	}

	void GraphicsTest::Resize(uint32_t width, uint32_t height)
	{
		m_Renderer->Resize(width, height);
	}

}
