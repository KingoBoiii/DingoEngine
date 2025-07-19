#include "ClearColorTest.h"

namespace Dingo
{

	void ClearColorTest::Initialize()
	{
		m_Framebuffer = Framebuffer::Create(FramebufferParams()
			.SetDebugName("ClearColorTestFramebuffer")
			.SetWidth(800)
			.SetHeight(600)
			.AddAttachment({ TextureFormat::RGBA8_UNORM } ));
		m_Framebuffer->Initialize();

		CommandListParams commandListParams = CommandListParams()
			.SetTargetFramebuffer(m_Framebuffer);

		m_CommandList = CommandList::Create(commandListParams);
		m_CommandList->Initialize();
	}

	void ClearColorTest::Update(float deltaTime)
	{
		m_CommandList->Begin();
		m_CommandList->Clear();
		m_CommandList->End();
	}

	void ClearColorTest::Cleanup()
	{
		m_CommandList->Destroy();

		if (m_Framebuffer)
		{
			m_Framebuffer->Destroy();
			m_Framebuffer = nullptr;
		}
	}

	void ClearColorTest::Resize(uint32_t width, uint32_t height)
	{
		if(width != m_Framebuffer->GetParams().Width || height != m_Framebuffer->GetParams().Height)
		{
			Application::Get().SubmitPostExecution([this, width, height]()
			{
				m_Framebuffer->Resize(width, height);
			});
		}
	}

}
