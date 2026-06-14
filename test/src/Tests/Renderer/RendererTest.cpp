#include "RendererTest.h"

namespace Dingo
{

	void RendererTest::Resize(uint32_t width, uint32_t height)
	{
		if (width == Renderer::GetSwapChainFramebuffer()->GetParams().Width &&
		    height == Renderer::GetSwapChainFramebuffer()->GetParams().Height)
		{
			return;
		}

		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
	}

}
