#include "GraphicsTest.h"

namespace Dingo
{


	void GraphicsTest::Initialize()
	{
		m_Framebuffer = Framebuffer::Create(FramebufferParams()
		.SetDebugName("ClearColorTestFramebuffer")
		.SetWidth(800)
		.SetHeight(600)
		.AddAttachment({ TextureFormat::RGBA8_UNORM }));
		m_Framebuffer->Initialize();

		m_AspectRatio = static_cast<float>(m_Framebuffer->GetParams().Width) / static_cast<float>(m_Framebuffer->GetParams().Height);

		CommandListParams commandListParams = CommandListParams()
			.SetTargetFramebuffer(m_Framebuffer);

		m_CommandList = CommandList::Create(commandListParams);
		m_CommandList->Initialize();

		InitializeGraphics();
	}

	void GraphicsTest::Cleanup()
	{
		CleanupGraphics();

		m_CommandList->Destroy();

		if (m_Framebuffer)
		{
			m_Framebuffer->Destroy();
			m_Framebuffer = nullptr;
		}
	}

	void GraphicsTest::Resize(uint32_t width, uint32_t height)
	{
		if (width != m_Framebuffer->GetParams().Width || height != m_Framebuffer->GetParams().Height)
		{
			Application::Get().SubmitPostExecution([this, width, height]()
			{
				m_Framebuffer->Resize(width, height);

				m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
			});
		}
	}

}
