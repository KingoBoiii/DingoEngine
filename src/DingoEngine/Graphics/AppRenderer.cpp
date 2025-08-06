#include "depch.h"
#include "AppRenderer.h"

namespace Dingo
{

	AppRenderer* AppRenderer::Create(SwapChain* swapChain)
	{
		AppRenderer* appRenderer = new AppRenderer(swapChain);
		appRenderer->Initialize();
		return appRenderer;
	}

	void AppRenderer::BeginFrame()
	{
		m_SwapChain->AcquireNextImage();
	}

	void AppRenderer::EndFrame()
	{
		m_SwapChain->Present();
	}

}
