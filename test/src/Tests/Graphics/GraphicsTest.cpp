#include "GraphicsTest.h"

namespace Dingo
{


	void GraphicsTest::Initialize()
	{
		m_Renderer = Renderer::Create(RendererParams{ .TargetSwapChain = false });
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
		auto d = m_Renderer->GetTargetFramebuffer();

		if (width == m_Renderer->GetTargetFramebuffer()->GetParams().Width && height == m_Renderer->GetTargetFramebuffer()->GetParams().Height)
		{
			return;
		}

		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);

		Application::Get().SubmitPostExecution([this, width, height]()
		{
			m_Renderer->Resize(width, height);
		});
	}

	void GraphicsTest::ImGuiRender()
	{
		ClearColorTest::ImGuiRender();
	}

}
