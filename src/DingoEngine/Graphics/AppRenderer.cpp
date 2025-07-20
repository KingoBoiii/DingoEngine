#include "depch.h"
#include "AppRenderer.h"

namespace Dingo
{

	void AppRenderer::Initialize()
	{
		CommandListParams commandListParams = CommandListParams()
			.SetTargetSwapChain(true);

		m_CommandList = CommandList::Create(commandListParams);
		m_CommandList->Initialize();
	}

	void AppRenderer::Shutdown()
	{
		if (m_CommandList)
		{
			m_CommandList->Destroy();
			m_CommandList = nullptr;
		}
	}

	void AppRenderer::BeginFrame()
	{
		m_SwapChain->AcquireNextImage();
	}

	void AppRenderer::EndFrame()
	{
		m_SwapChain->Present();
	}

	void AppRenderer::Begin()
	{
		m_CommandList->Begin();
	}

	void AppRenderer::End()
	{
		m_CommandList->End();
	}

	void AppRenderer::Resize(uint32_t width, uint32_t height)
	{}

	void AppRenderer::Clear(const glm::vec4& clearColor)
	{
		m_CommandList->Clear(0, clearColor);
	}

	Texture* AppRenderer::GetOutput() const
	{
		return m_SwapChain->GetCurrentFramebuffer()->GetAttachment(0);
	}

}
