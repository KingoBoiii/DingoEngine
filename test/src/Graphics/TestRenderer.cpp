#include "TestRenderer.h"

namespace Dingo
{

	void TestRenderer::Initialize()
	{
		m_Framebuffer = Framebuffer::Create(FramebufferParams()
			.SetDebugName("ClearColorTestFramebuffer")
			.SetWidth(800)
			.SetHeight(600)
			.AddAttachment({ TextureFormat::RGBA8_UNORM }));
		m_Framebuffer->Initialize();

		CommandListParams commandListParams = CommandListParams()
			.SetTargetFramebuffer(m_Framebuffer);

		m_CommandList = CommandList::Create(commandListParams);
		m_CommandList->Initialize();
	}

	void TestRenderer::Shutdown()
	{
		if (m_CommandList)
		{
			m_CommandList->Destroy();
			m_CommandList = nullptr;
		}

		if (m_Framebuffer)
		{
			m_Framebuffer->Destroy();
			m_Framebuffer = nullptr;
		}
	}

	void TestRenderer::Begin()
	{
		m_CommandList->Begin();
	}

	void TestRenderer::End()
	{
		m_CommandList->End();
	}

	void TestRenderer::Resize(uint32_t width, uint32_t height)
	{
		if (width != m_Framebuffer->GetParams().Width || height != m_Framebuffer->GetParams().Height)
		{
			Application::Get().SubmitPostExecution([this, width, height]()
			{
				m_Framebuffer->Resize(width, height);
			});
		}
	}

	void TestRenderer::Clear(const glm::vec4& clearColor)
	{
		m_CommandList->Clear(0, clearColor);
	}

}
