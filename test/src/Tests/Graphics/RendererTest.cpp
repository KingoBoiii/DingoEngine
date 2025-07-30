#include "RendererTest.h"

namespace Dingo
{


	void RendererTest::Initialize()
	{
		m_Renderer = Renderer::Create(RendererParams{ .TargetSwapChain = false });
		m_Renderer->Initialize();

		InitializeGraphics();
	}

	void RendererTest::Cleanup()
	{
		CleanupGraphics();

		if (m_Renderer)
		{
			m_Renderer->Shutdown();
			delete m_Renderer;
			m_Renderer = nullptr;
		}
	}

	void RendererTest::Resize(uint32_t width, uint32_t height)
	{
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

}
