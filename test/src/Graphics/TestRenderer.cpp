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

	void TestRenderer::Draw(Pipeline* pipeline, uint32_t vertexCount, uint32_t instanceCount)
	{
		m_CommandList->Draw(pipeline, vertexCount, instanceCount);
	}

	void TestRenderer::Draw(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, uint32_t vertexCount, uint32_t instanceCount)
	{
		m_CommandList->Draw(pipeline, vertexBuffer, vertexCount, instanceCount);
	}

	void TestRenderer::DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer)
	{
		m_CommandList->DrawIndexed(pipeline, vertexBuffer, indexBuffer);
	}

	void TestRenderer::DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, GraphicsBuffer* uniformBuffer)
	{
		m_CommandList->DrawIndexed(pipeline, vertexBuffer, indexBuffer, uniformBuffer);
	}

}
