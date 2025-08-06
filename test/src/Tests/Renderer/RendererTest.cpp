#include "RendererTest.h"

namespace Dingo
{

	void RendererTest::Resize(uint32_t width, uint32_t height)
	{
		if (width == m_Renderer->GetTargetFramebuffer()->GetParams().Width && height == m_Renderer->GetTargetFramebuffer()->GetParams().Height)
		{
			return;
		}

		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
	}

}
