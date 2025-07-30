#include "Renderer2DTest.h"

#include <imgui.h>

namespace Dingo
{

	void Renderer2DTest::Initialize()
	{
		m_Renderer = new Renderer2D(Renderer2DParams{});
		m_Renderer->Initialize();
	}

	void Renderer2DTest::Cleanup()
	{
		if (m_Renderer)
		{
			m_Renderer->Shutdown();
			delete m_Renderer;
			m_Renderer = nullptr;
		}
	}

	void Renderer2DTest::Resize(uint32_t width, uint32_t height)
	{
		if (width == m_Renderer->GetViewportSize().x && height == m_Renderer->GetViewportSize().y)
		{
			return;
		}

		Application::Get().SubmitPostExecution([this, width, height]()
		{
			if (m_Renderer)
			{
				m_Renderer->Resize(width, height);
			}
		});
	}

	void Renderer2DTest::ImGuiRender()
	{
		ClearColorTest::ImGuiRender();
	}

}
