#include "depch.h"
#include "AppRenderer.h"

namespace Dingo
{

	AppRenderer* AppRenderer::Create(SwapChain* swapChain)
	{
		return new AppRenderer(swapChain);
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
