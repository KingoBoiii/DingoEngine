#include "depch.h"
#include "DingoEngine/Graphics/Renderer.h"

#include "DingoEngine/Core/Application.h"

namespace Dingo
{

	struct StaticResources
	{
		Texture* WhiteTexture = nullptr;
		Sampler* ClampSampler = nullptr;
		Sampler* PointSampler = nullptr;
	};

	static StaticResources* s_StaticResources = nullptr;

	Renderer* Renderer::Create(Framebuffer* framebuffer)
	{
		Renderer* renderer = new Renderer(RendererParams{ .TargetFramebuffer = framebuffer });
		renderer->Initialize();
		return renderer;
	}

	Renderer* Renderer::Create(const RendererParams& params)
	{
		Renderer* renderer = new Renderer(params);
		renderer->Initialize();
		return renderer;
	}

	void Renderer::Initialize()
	{
		m_TargetFramebuffer = m_Params.TargetFramebuffer;
		if (m_TargetFramebuffer == nullptr)
		{
			m_TargetFramebuffer = Application::Get().GetSwapChain()->GetCurrentFramebuffer();
		}

		m_CommandList = CommandList::Create();
	}

	void Renderer::Shutdown()
	{
		if (m_CommandList)
		{
			m_CommandList->Destroy();
			m_CommandList = nullptr;
		}
	}

	/**************************************************
	***		RENDER PASS								***
	**************************************************/

	void Renderer::BeginRenderPass(RenderPass* renderPass)
	{
		// TODO: find out if Render Pass needs to be rebaked, before setting it

		m_CommandList->SetRenderPass(renderPass);
	}

	void Renderer::EndRenderPass()
	{}

	/**************************************************
	***		DRAW CALLS								***
	**************************************************/

	void Renderer::DrawIndexed(GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, uint32_t indexCount)
	{
		if (indexCount == 0)
		{
			indexCount = indexBuffer->GetByteSize() / sizeof(uint16_t); // Assuming 16-bit indices
		}

		m_CommandList->AddVertexBuffer(vertexBuffer, 0);
		m_CommandList->SetIndexBuffer(indexBuffer, 0);
		m_CommandList->DrawIndexed(indexCount, 1); // Assuming 16-bit indices
	}

	void Renderer::DrawIndexed(GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, GraphicsBuffer* uniformBuffer, uint32_t indexCount)
	{
		if (indexCount == 0)
		{
			indexCount = indexBuffer->GetByteSize() / sizeof(uint16_t); // Assuming 16-bit indices
		}

		m_CommandList->AddVertexBuffer(vertexBuffer, 0);
		m_CommandList->SetIndexBuffer(indexBuffer, 0);
		m_CommandList->DrawIndexed(indexCount, 1); // Assuming 16-bit indices
	}

	/**************************************************
	***		GENERAL									***
	**************************************************/

	void Renderer::Begin()
	{
		m_CommandList->Begin();
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
#if 0
		if (m_Params.TargetSwapChain)
		{
			return;
		}
#endif

		if (width == m_TargetFramebuffer->GetParams().Width && height == m_TargetFramebuffer->GetParams().Height)
		{
			return;
		}

		m_TargetFramebuffer->Resize(width, height);
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

	/**************************************************
	***		STATIC RESOURCES 						***
	**************************************************/

	Texture* Renderer::GetWhiteTexture()
	{
		return s_StaticResources->WhiteTexture;
	}

	Sampler* Renderer::GetClampSampler()
	{
		return s_StaticResources->ClampSampler;
	}

	Sampler* Renderer::GetPointSampler()
	{
		return s_StaticResources->PointSampler;
	}

	void Renderer::InitializeStaticResources()
	{
		if (s_StaticResources)
		{
			return;
		}

		s_StaticResources = new StaticResources();

		uint32_t whiteTextureData = 0xffffffff;
		s_StaticResources->WhiteTexture = Texture::CreateFromData(1, 1, &whiteTextureData, TextureFormat::RGBA, "White Texture");

		s_StaticResources->ClampSampler = Sampler::Create(SamplerParams());
		s_StaticResources->ClampSampler->Initialize();

		s_StaticResources->PointSampler = Sampler::Create(SamplerParams()
			.SetMinFilter(false)
			.SetMagFilter(false)
			.SetMipFilter(false));
		s_StaticResources->PointSampler->Initialize();
	}

	void Renderer::DestroyStaticResources()
	{
		if (!s_StaticResources)
		{
			return;
		}

		if (s_StaticResources->WhiteTexture)
		{
			s_StaticResources->WhiteTexture->Destroy();
			s_StaticResources->WhiteTexture = nullptr;
		}

		if (s_StaticResources->ClampSampler)
		{
			s_StaticResources->ClampSampler->Destroy();
			s_StaticResources->ClampSampler = nullptr;
		}

		if (s_StaticResources->PointSampler)
		{
			s_StaticResources->PointSampler->Destroy();
			s_StaticResources->PointSampler = nullptr;
		}

		delete s_StaticResources;
	}

}
