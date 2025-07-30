#include "depch.h"
#include "DingoEngine/Graphics/Renderer.h"

#include "DingoEngine/Core/Application.h"

namespace Dingo
{

	Renderer* Renderer::Create(const RendererParams& params)
	{
		return new Renderer(params);
	}

	void Renderer::Initialize()
	{
		if (!m_Params.TargetSwapChain)
		{
			m_TargetFramebuffer = Framebuffer::Create(FramebufferParams()
				.SetDebugName(m_Params.FramebufferName)
				.SetWidth(800)
				.SetHeight(600)
				.AddAttachment({ TextureFormat::RGBA8_UNORM }));
			m_TargetFramebuffer->Initialize();
		}

		CommandListParams commandListParams = CommandListParams()
			.SetTargetSwapChain(m_Params.TargetSwapChain)
			.SetTargetFramebuffer(m_Params.TargetSwapChain ? nullptr : m_TargetFramebuffer);

		m_CommandList = CommandList::Create(commandListParams);
		m_CommandList->Initialize();
	}

	void Renderer::Shutdown()
	{
		if (m_CommandList)
		{
			m_CommandList->Destroy();
			delete m_CommandList;
			m_CommandList = nullptr;
		}

		if (m_TargetFramebuffer && !m_Params.TargetSwapChain)
		{
			m_TargetFramebuffer->Destroy();
			//delete m_TargetFramebuffer;
			m_TargetFramebuffer = nullptr;
		}
	}

	void Renderer::BeginRenderPass(RenderPass* renderPass)
	{
		if (m_Params.TargetSwapChain)
		{
			m_TargetFramebuffer = Application::Get().GetSwapChain()->GetCurrentFramebuffer();
		}
		else
		{
			m_TargetFramebuffer = renderPass->GetTargetFramebuffer();
		}

		m_CommandList->Begin(m_TargetFramebuffer);
		m_CommandList->SetRenderPass(renderPass);
	}

	void Renderer::EndRenderPass()
	{
		m_CommandList->End();

		m_TargetFramebuffer = nullptr;
	}

	void Renderer::Begin()
	{
		m_CommandList->Begin(m_TargetFramebuffer);
	}

	void Renderer::End()
	{
		m_CommandList->End();
	}

	void Renderer::Clear(Framebuffer* framebuffer, const glm::vec4& clearColor)
	{
		m_CommandList->Clear(framebuffer, 0, clearColor);
	}

	void Renderer::Resize(uint32_t width, uint32_t height)
	{
		if (m_Params.TargetSwapChain)
		{
			return;
		}

		if (width == m_TargetFramebuffer->GetParams().Width && height == m_TargetFramebuffer->GetParams().Height)
		{
			return;
		}

		Application::Get().SubmitPostExecution([this, width, height]()
		{
			m_TargetFramebuffer->Resize(width, height);
		});
	}

	void Renderer::Clear(const glm::vec4& clearColor)
	{
		m_CommandList->Clear(m_TargetFramebuffer, 0, clearColor);
	}

	void Renderer::Upload(GraphicsBuffer* buffer)
	{
		m_CommandList->UploadBuffer(buffer, buffer->GetData(), buffer->GetByteSize());
	}

	void Renderer::Upload(GraphicsBuffer* buffer, const void* data, uint64_t size)
	{
		m_CommandList->UploadBuffer(buffer, data, size);
	}

	void Renderer::Draw(Pipeline* pipeline, uint32_t vertexCount, uint32_t instanceCount)
	{
		m_CommandList->SetPipeline(pipeline);
		m_CommandList->Draw(vertexCount, instanceCount);
	}

	void Renderer::Draw(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, uint32_t vertexCount, uint32_t instanceCount)
	{
		m_CommandList->SetPipeline(pipeline);
		m_CommandList->AddVertexBuffer(vertexBuffer, 0);
		m_CommandList->Draw(vertexCount, instanceCount);
	}

	void Renderer::DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer)
	{
		m_CommandList->SetPipeline(pipeline);
		m_CommandList->AddVertexBuffer(vertexBuffer, 0);
		m_CommandList->SetIndexBuffer(indexBuffer, 0);
		m_CommandList->DrawIndexed(indexBuffer->GetByteSize() / sizeof(uint16_t), 1); // Assuming 16-bit indices
	}

	void Renderer::DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, GraphicsBuffer* uniformBuffer)
	{
		m_CommandList->UploadBuffer(uniformBuffer, uniformBuffer->GetData(), uniformBuffer->GetByteSize());

		m_CommandList->SetPipeline(pipeline);
		m_CommandList->AddVertexBuffer(vertexBuffer, 0);
		m_CommandList->SetIndexBuffer(indexBuffer, 0);
		m_CommandList->DrawIndexed(indexBuffer->GetByteSize() / sizeof(uint16_t), 1); // Assuming 16-bit indices
	}

	Texture* Renderer::GetOutput() const
	{
		return m_TargetFramebuffer->GetAttachment(0);
	}

}
