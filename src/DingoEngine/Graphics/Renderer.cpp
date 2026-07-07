#include "depch.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/Material.h"
#include "DingoEngine/Graphics/SwapChain.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include <thread>
#include <mutex>
#include <condition_variable>

namespace Dingo
{

	struct RendererData
	{
		SwapChain*   SwapChain      = nullptr;
		CommandList* CommandList    = nullptr;
		Framebuffer* RenderTarget   = nullptr; // null = use swap chain

		std::thread             RenderThread;
		std::mutex              Mutex;
		std::condition_variable FrameReadyCV;
		std::condition_variable FrameConsumedCV;
		bool HasFrame        = false;
		bool FrameConsumed   = true;
		bool Running         = false;
		bool HasPendingFrame = false;

		// Written by the main thread (resize events), consumed by the render thread
		// between Present and the next AcquireNextImage. Guarded by Mutex.
		bool    HasPendingResize    = false;
		int32_t PendingResizeWidth  = 0;
		int32_t PendingResizeHeight = 0;

		Texture* WhiteTexture = nullptr;
		Sampler* ClampSampler = nullptr;
		Sampler* PointSampler = nullptr;
	};

	RendererData* Renderer::s_Data = nullptr;

	/**************************************************
	***		LIFECYCLE								***
	**************************************************/

	void Renderer::Initialize(SwapChain* swapChain)
	{
		s_Data = new RendererData();
		s_Data->SwapChain   = swapChain;
		s_Data->CommandList = CommandList::Create();

		uint32_t whiteTextureData = 0xffffffff;
		s_Data->WhiteTexture = Texture::CreateFromData(1, 1, &whiteTextureData, TextureFormat::RGBA, "White Texture");

		s_Data->ClampSampler = Sampler::Create(SamplerParams());
		s_Data->ClampSampler->Initialize();

		s_Data->PointSampler = Sampler::Create(SamplerParams()
			.SetMinFilter(false)
			.SetMagFilter(false)
			.SetMipFilter(false));
		s_Data->PointSampler->Initialize();

		swapChain->AcquireNextImage();
		s_Data->Running      = true;
		s_Data->RenderThread = std::thread(&Renderer::RenderThreadLoop);
	}

	void Renderer::Shutdown()
	{
		{
			std::lock_guard<std::mutex> lock(s_Data->Mutex);
			s_Data->Running  = false;
			s_Data->HasFrame = true; // sentinel: wake the render thread
		}
		s_Data->FrameReadyCV.notify_one();

		if (s_Data->RenderThread.joinable())
			s_Data->RenderThread.join();

		// If the render thread exited before executing the last closed frame,
		// submit it now to break the NVRHI CommandList <-> TrackedCommandBuffer cycle.
		if (s_Data->HasPendingFrame)
			Execute();

		if (s_Data->CommandList)
		{
			s_Data->CommandList->Destroy();
			s_Data->CommandList = nullptr;
		}

		if (s_Data->WhiteTexture) { s_Data->WhiteTexture->Destroy(); s_Data->WhiteTexture = nullptr; }
		if (s_Data->ClampSampler) { s_Data->ClampSampler->Destroy(); s_Data->ClampSampler = nullptr; }
		if (s_Data->PointSampler) { s_Data->PointSampler->Destroy(); s_Data->PointSampler = nullptr; }

		delete s_Data;
		s_Data = nullptr;
	}

	/**************************************************
	***		FRAME MANAGEMENT						***
	**************************************************/

	void Renderer::BeginFrame()
	{
		std::unique_lock<std::mutex> lock(s_Data->Mutex);
		s_Data->FrameConsumedCV.wait(lock, [] { return s_Data->FrameConsumed; });
		s_Data->FrameConsumed = false;
		Begin();
	}

	void Renderer::EndFrame()
	{
		Close();
		{
			std::lock_guard<std::mutex> lock(s_Data->Mutex);
			s_Data->HasFrame        = true;
			s_Data->HasPendingFrame = true;
		}
		s_Data->FrameReadyCV.notify_one();
	}

	void Renderer::QueueResize(int32_t width, int32_t height)
	{
		// A (0,0) size means the window is minimized -- nothing to recreate; the swap chain
		// keeps skipping presents until a real size arrives.
		if (!s_Data || width <= 0 || height <= 0)
			return;

		std::lock_guard<std::mutex> lock(s_Data->Mutex);
		s_Data->HasPendingResize    = true;
		s_Data->PendingResizeWidth  = width;
		s_Data->PendingResizeHeight = height;
	}

	void Renderer::RenderThreadLoop()
	{
		while (true)
		{
			bool running;
			{
				std::unique_lock<std::mutex> lock(s_Data->Mutex);
				s_Data->FrameReadyCV.wait(lock, [] { return s_Data->HasFrame; });
				running          = s_Data->Running;
				s_Data->HasFrame = false;
			}

			if (!running)
				break;

			Execute();
			s_Data->SwapChain->Present();
			GraphicsContext::Get().RunGarbageCollection();

			// Apply a queued resize here: the presented frame is complete and no image is
			// acquired yet, so the swap chain (and its framebuffers, which the main thread
			// records against) can be recreated without racing either thread.
			{
				bool    resize = false;
				int32_t width = 0, height = 0;
				{
					std::lock_guard<std::mutex> lock(s_Data->Mutex);
					resize = s_Data->HasPendingResize;
					width  = s_Data->PendingResizeWidth;
					height = s_Data->PendingResizeHeight;
					s_Data->HasPendingResize = false;
				}
				if (resize)
					s_Data->SwapChain->Resize(width, height);
			}

			s_Data->SwapChain->AcquireNextImage();

			{
				std::lock_guard<std::mutex> lock(s_Data->Mutex);
				s_Data->FrameConsumed = true;
			}
			s_Data->FrameConsumedCV.notify_one();
		}
	}

	/**************************************************
	***		COMMAND LIST MANAGEMENT					***
	**************************************************/

	void Renderer::Begin()
	{
		s_Data->CommandList->Begin();
	}

	void Renderer::Close()
	{
		s_Data->CommandList->Close();
	}

	void Renderer::Execute()
	{
		s_Data->HasPendingFrame = false;
		s_Data->CommandList->Execute();
	}

	/**************************************************
	***		RENDER TARGET OVERRIDE					***
	**************************************************/

	Framebuffer* Renderer::GetCurrentTarget()
	{
		return s_Data->RenderTarget
			? s_Data->RenderTarget
			: s_Data->SwapChain->GetCurrentFramebuffer();
	}

	void Renderer::SetRenderTarget(Framebuffer* framebuffer)
	{
		s_Data->RenderTarget = framebuffer;
	}

	void Renderer::ResetRenderTarget()
	{
		s_Data->RenderTarget = nullptr;
	}

	/**************************************************
	***		RESOURCE UPLOAD							***
	**************************************************/

	void Renderer::Upload(GraphicsBuffer* buffer)
	{
		s_Data->CommandList->UploadBuffer(buffer, buffer->GetData(), buffer->GetByteSize());
	}

	void Renderer::Upload(GraphicsBuffer* buffer, const void* data, uint64_t size)
	{
		s_Data->CommandList->UploadBuffer(buffer, data, size);
	}

	/**************************************************
	***		CLEAR									***
	**************************************************/

	void Renderer::Clear(Framebuffer* framebuffer, const glm::vec4& clearColor)
	{
		s_Data->CommandList->Clear(framebuffer, 0, clearColor);
	}

	void Renderer::Clear(const glm::vec4& clearColor)
	{
		Framebuffer* target = GetCurrentTarget();
		s_Data->CommandList->SetFramebuffer(target);
		s_Data->CommandList->Clear(target, 0, clearColor);
	}

	/**************************************************
	***		DRAW — explicit Pipeline				***
	**************************************************/

	void Renderer::Draw(Pipeline* pipeline, uint32_t vertexCount, uint32_t instanceCount)
	{
		Framebuffer* target = GetCurrentTarget();
		s_Data->CommandList->SetPipeline(pipeline);
		s_Data->CommandList->SetFramebuffer(target);
		s_Data->CommandList->Draw(vertexCount, instanceCount);
	}

	void Renderer::Draw(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, uint32_t vertexCount, uint32_t instanceCount)
	{
		Framebuffer* target = GetCurrentTarget();
		s_Data->CommandList->SetPipeline(pipeline);
		s_Data->CommandList->SetFramebuffer(target);
		s_Data->CommandList->AddVertexBuffer(vertexBuffer, 0);
		s_Data->CommandList->Draw(vertexCount, instanceCount);
	}

	void Renderer::DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, uint32_t indexCount)
	{
		if (indexCount == 0)
			indexCount = static_cast<uint32_t>(indexBuffer->GetByteSize() / sizeof(uint16_t));

		Framebuffer* target = GetCurrentTarget();
		s_Data->CommandList->SetPipeline(pipeline);
		s_Data->CommandList->SetFramebuffer(target);
		s_Data->CommandList->AddVertexBuffer(vertexBuffer, 0);
		s_Data->CommandList->SetIndexBuffer(indexBuffer, 0);
		s_Data->CommandList->DrawIndexed(indexCount, 1);
	}

	/**************************************************
	***		DRAW — explicit RenderPass				***
	**************************************************/

	void Renderer::DrawIndexed(RenderPass* renderPass, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, uint32_t indexCount)
	{
		if (indexCount == 0)
			indexCount = static_cast<uint32_t>(indexBuffer->GetByteSize() / sizeof(uint16_t));

		Framebuffer* target = GetCurrentTarget();
		s_Data->CommandList->SetRenderPass(renderPass);
		s_Data->CommandList->SetFramebuffer(target);
		s_Data->CommandList->AddVertexBuffer(vertexBuffer, 0);
		s_Data->CommandList->SetIndexBuffer(indexBuffer, 0);
		s_Data->CommandList->DrawIndexed(indexCount, 1);
	}

	/**************************************************
	***		DRAW — Material							***
	**************************************************/

	void Renderer::DrawIndexed(Material* material, const VertexLayout& layout, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, uint32_t indexCount)
	{
		if (indexCount == 0)
			indexCount = static_cast<uint32_t>(indexBuffer->GetByteSize() / sizeof(uint16_t));

		Framebuffer* target = GetCurrentTarget();

		// Upload uniform data to GPU if it changed since the last draw.
		if (material->IsUniformDirty() && material->GetUniformBuffer())
		{
			const auto& cpu = material->GetUniformCPUData();
			s_Data->CommandList->UploadBuffer(material->GetUniformBuffer(), cpu.data(), cpu.size());
			material->ClearUniformDirty();
		}

		RenderPass* renderPass = material->GetOrCreateRenderPass(layout, target);
		s_Data->CommandList->SetRenderPass(renderPass);
		s_Data->CommandList->SetFramebuffer(target);
		s_Data->CommandList->AddVertexBuffer(vertexBuffer, 0);
		s_Data->CommandList->SetIndexBuffer(indexBuffer, 0);
		s_Data->CommandList->DrawIndexed(indexCount, 1);
	}

	/**************************************************
	***		QUERIES									***
	**************************************************/

	CommandList* Renderer::GetCommandList()
	{
		return s_Data->CommandList;
	}

	Framebuffer* Renderer::GetSwapChainFramebuffer()
	{
		return s_Data->SwapChain->GetCurrentFramebuffer();
	}

	/**************************************************
	***		STATIC RESOURCES						***
	**************************************************/

	Texture* Renderer::GetWhiteTexture()
	{
		return s_Data->WhiteTexture;
	}

	Sampler* Renderer::GetClampSampler()
	{
		return s_Data->ClampSampler;
	}

	Sampler* Renderer::GetPointSampler()
	{
		return s_Data->PointSampler;
	}

}
