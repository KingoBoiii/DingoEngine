#include "depch.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/Material.h"
#include "DingoEngine/Graphics/SwapChain.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/GpuTimers.h"
#include "DingoEngine/Graphics/PostProcess.h"
#include "DingoEngine/Core/Profiler.h"
#include "DingoEngine/Core/Timer.h"

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace Dingo
{

	namespace
	{
		// Resolves the "whole buffer" default. The stride has to come from the buffer's own
		// format: assuming uint16 on a uint32 index buffer asks for twice the indices that
		// exist. Unknown falls back to uint16, which is what CreateIndexBuffer defaults to.
		uint32_t ResolveIndexCount(GraphicsBuffer* indexBuffer, uint32_t indexCount)
		{
			if (indexCount != 0)
				return indexCount;

			const uint64_t stride = indexBuffer->GetFormat() == GraphicsFormat::Uint32 ? sizeof(uint32_t) : sizeof(uint16_t);
			return static_cast<uint32_t>(indexBuffer->GetByteSize() / stride);
		}
	}

	struct RendererData
	{
		SwapChain*   SwapChain      = nullptr;
		CommandList* CommandList    = nullptr;
		Framebuffer* RenderTarget   = nullptr; // null = use swap chain
		bool         HasViewport    = false;
		Viewport     ViewportOverride;
		uint64_t     FrameIndex     = 0;       // bumped per command-list Begin, so never 0 while recording
		bool         FrameSkipped   = false;   // main thread only: from SkipFrame, or a BeginFrame without an image, to the next BeginFrame

		std::thread             RenderThread;
		std::mutex              Mutex;
		std::condition_variable FrameReadyCV;
		std::condition_variable FrameConsumedCV;
		bool HasFrame        = false;
		bool FrameConsumed   = true;
		bool Running         = false;
		bool HasPendingFrame = false;

		// Written by the main thread (resize events), consumed by the render thread
		// between Present and the next AcquireNextImage, or by a BeginFrame without an image.
		// Guarded by Mutex.
		bool    HasPendingResize    = false;
		int32_t PendingResizeWidth  = 0;
		int32_t PendingResizeHeight = 0;

		// Main thread only: run at the next BeginFrame or SkipFrame, or at Shutdown.
		std::vector<std::function<void()>> AfterFrame;
		// Main thread only: from BeginFrame or SkipFrame to EndFrame, and before the first frame, the
		// render thread is parked and the main thread may submit command lists of its own.
		bool RenderThreadParked = true;

		Texture* WhiteTexture = nullptr;
		Sampler* ClampSampler = nullptr;
		Sampler* PointSampler = nullptr;

		Internal::GpuTimers GpuTimers;
		PostProcessStack PostProcess;
		std::atomic<float> RenderThreadMs = 0.0f;
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

		s_Data->PointSampler = Sampler::Create(SamplerParams()
			.SetMinFilter(false)
			.SetMagFilter(false)
			.SetMipFilter(false));

		swapChain->AcquireNextImage();
		s_Data->Running      = true;
		s_Data->RenderThread = std::thread(&Renderer::RenderThreadLoop);
	}

	void Renderer::Shutdown()
	{
		if (!s_Data)
			return;

		{
			std::lock_guard<std::mutex> lock(s_Data->Mutex);
			s_Data->Running  = false;
			s_Data->HasFrame = true; // sentinel: wake the render thread
		}
		s_Data->FrameReadyCV.notify_one();

		if (s_Data->RenderThread.joinable())
			s_Data->RenderThread.join();
		s_Data->RenderThreadParked = true;

		// If the render thread exited before executing the last closed frame,
		// submit it now to break the NVRHI CommandList <-> TrackedCommandBuffer cycle.
		if (s_Data->HasPendingFrame)
			Execute();
		RunPendingAfterFrame();

		DestroyAndDelete(s_Data->CommandList);
	}

	void Renderer::Destroy()
	{
		if (!s_Data)
			return;

		s_Data->PostProcess.Shutdown();
		DestroyAndDelete(s_Data->WhiteTexture);
		DestroyAndDelete(s_Data->ClampSampler);
		DestroyAndDelete(s_Data->PointSampler);

		delete s_Data;
		s_Data = nullptr;
	}

	/**************************************************
	***		FRAME MANAGEMENT						***
	**************************************************/

	void Renderer::BeginFrame()
	{
		bool acquire = false;
		bool resize = false;
		int32_t width = 0, height = 0;
		{
			std::unique_lock<std::mutex> lock(s_Data->Mutex);
			s_Data->FrameConsumedCV.wait(lock, [] { return s_Data->FrameConsumed; });
			s_Data->FrameConsumed = false;
			s_Data->FrameSkipped = false;
			s_Data->RenderThreadParked = true;

			// The render thread acquires after each present, which gets no image while the window is
			// minimized. The first frame after the restore would draw into a stale one, so the resize
			// that restored the window is applied and an image acquired below, while that thread is parked.
			acquire = !s_Data->SwapChain->IsImageAcquired();
			if (acquire)
			{
				resize = s_Data->HasPendingResize;
				width = s_Data->PendingResizeWidth;
				height = s_Data->PendingResizeHeight;
				s_Data->HasPendingResize = false;
			}
		}

		RunPendingAfterFrame();

		if (acquire)
		{
			if (resize)
				s_Data->SwapChain->Resize(width, height);
			s_Data->SwapChain->AcquireNextImage();
			s_Data->FrameSkipped = !s_Data->SwapChain->IsImageAcquired();
		}

		Begin();
		s_Data->GpuTimers.BeginFrame(s_Data->FrameIndex);
		BeginGpuTimer("Frame");
	}

	void Renderer::SkipFrame()
	{
		{
			std::unique_lock<std::mutex> lock(s_Data->Mutex);
			s_Data->FrameConsumedCV.wait(lock, [] { return s_Data->FrameConsumed; });
			s_Data->RenderThreadParked = true;
		}
		RunPendingAfterFrame();
		s_Data->FrameSkipped = true;

		// The render thread collects after each present and stays parked until the next
		// EndFrame, so the uploads made meanwhile (async asset loads) are collected here.
		GraphicsContext::Get().RunGarbageCollection();
	}

	bool Renderer::IsFrameSkipped()
	{
		return s_Data && s_Data->FrameSkipped;
	}

	void Renderer::RunAfterFrame(std::function<void()> fn)
	{
		s_Data->AfterFrame.push_back(std::move(fn));
	}

	void Renderer::RunPendingAfterFrame()
	{
		// A callback may queue more for the frame after.
		std::vector<std::function<void()>> pending;
		pending.swap(s_Data->AfterFrame);
		for (std::function<void()>& fn : pending)
			fn();
	}

	void Renderer::EndFrame()
	{
		s_Data->GpuTimers.EndFrame(s_Data->FrameSkipped ? nullptr : s_Data->CommandList);
		Close();
		{
			std::lock_guard<std::mutex> lock(s_Data->Mutex);
			s_Data->HasFrame        = true;
			s_Data->HasPendingFrame = true;
			s_Data->RenderThreadParked = false;
		}
		s_Data->FrameReadyCV.notify_one();
	}

	bool Renderer::IsRenderThreadParked()
	{
		return s_Data && s_Data->RenderThreadParked;
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
		DE_PROFILE_THREAD("Render");

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

			Timer timer;
			{
				DE_PROFILE_SCOPE("Renderer::Execute");
				Execute();
			}
			{
				DE_PROFILE_SCOPE("SwapChain::Present");
				s_Data->SwapChain->Present();
			}
			s_Data->RenderThreadMs.store(timer.ElapsedMillis(), std::memory_order_relaxed);
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
	***		GPU TIMERS								***
	**************************************************/

	void Renderer::BeginGpuTimer(const char* name)
	{
		CommandList* list = s_Data->FrameSkipped ? nullptr : TryGetRecordingCommandList();
		s_Data->GpuTimers.Begin(list, name);
	}

	void Renderer::EndGpuTimer()
	{
		CommandList* list = s_Data->FrameSkipped ? nullptr : TryGetRecordingCommandList();
		s_Data->GpuTimers.End(list);
	}

	const std::vector<GpuTimerStats>& Renderer::GetGpuTimers()
	{
		return s_Data->GpuTimers.GetStats();
	}

	float Renderer::GetRenderThreadMilliseconds()
	{
		return s_Data ? s_Data->RenderThreadMs.load(std::memory_order_relaxed) : 0.0f;
	}

	/**************************************************
	***		COMMAND LIST MANAGEMENT					***
	**************************************************/

	void Renderer::Begin()
	{
		++s_Data->FrameIndex;
		s_Data->CommandList->Begin();
	}

	void Renderer::Close()
	{
		s_Data->CommandList->Close();
	}

	void Renderer::Execute()
	{
		s_Data->HasPendingFrame = false;
		s_Data->SwapChain->QueueImageWait();
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
		s_Data->HasViewport = false;
	}

	void Renderer::ResetRenderTarget()
	{
		s_Data->RenderTarget = nullptr;
		s_Data->HasViewport = false;
	}

	void Renderer::SetViewport(const Viewport& viewport)
	{
		s_Data->ViewportOverride = viewport;
		s_Data->HasViewport = true;
	}

	void Renderer::ResetViewport()
	{
		s_Data->HasViewport = false;
	}

	void Renderer::BindTarget(Framebuffer* target)
	{
		s_Data->CommandList->SetFramebuffer(target);
		if (s_Data->HasViewport)
			s_Data->CommandList->SetViewport(s_Data->ViewportOverride);
	}

	Framebuffer* Renderer::GetRenderTarget()
	{
		return s_Data->RenderTarget;
	}

	/**************************************************
	***		RESOURCE UPLOAD							***
	**************************************************/

	void Renderer::Upload(GraphicsBuffer* buffer)
	{
		if (s_Data->FrameSkipped)
			return;

		s_Data->CommandList->UploadBuffer(buffer, buffer->GetData(), buffer->GetByteSize());
	}

	void Renderer::Upload(GraphicsBuffer* buffer, const void* data, uint64_t size)
	{
		if (s_Data->FrameSkipped)
			return;

		s_Data->CommandList->UploadBuffer(buffer, data, size);
	}

	/**************************************************
	***		CLEAR									***
	**************************************************/

	void Renderer::Clear(Framebuffer* framebuffer, const glm::vec4& clearColor)
	{
		if (s_Data->FrameSkipped)
			return;

		s_Data->CommandList->Clear(framebuffer, 0, clearColor);
	}

	void Renderer::Clear(const glm::vec4& clearColor)
	{
		if (s_Data->FrameSkipped)
			return;

		Framebuffer* target = GetCurrentTarget();
		s_Data->CommandList->SetFramebuffer(target);
		s_Data->CommandList->Clear(target, 0, clearColor);
	}

	/**************************************************
	***		DRAW — explicit Pipeline				***
	**************************************************/

	void Renderer::Draw(Pipeline* pipeline, uint32_t vertexCount, uint32_t instanceCount)
	{
		if (s_Data->FrameSkipped)
			return;

		Framebuffer* target = GetCurrentTarget();
		if (!s_Data->CommandList->SetPipeline(pipeline))
			return;

		BindTarget(target);
		s_Data->CommandList->Draw(vertexCount, instanceCount);
	}

	void Renderer::Draw(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, uint32_t vertexCount, uint32_t instanceCount)
	{
		if (s_Data->FrameSkipped)
			return;

		Framebuffer* target = GetCurrentTarget();
		if (!s_Data->CommandList->SetPipeline(pipeline))
			return;

		BindTarget(target);
		s_Data->CommandList->AddVertexBuffer(vertexBuffer, 0);
		s_Data->CommandList->Draw(vertexCount, instanceCount);
	}

	void Renderer::DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, uint32_t indexCount)
	{
		if (s_Data->FrameSkipped)
			return;

		indexCount = ResolveIndexCount(indexBuffer, indexCount);

		Framebuffer* target = GetCurrentTarget();
		if (!s_Data->CommandList->SetPipeline(pipeline))
			return;

		BindTarget(target);
		s_Data->CommandList->AddVertexBuffer(vertexBuffer, 0);
		s_Data->CommandList->SetIndexBuffer(indexBuffer, 0);
		s_Data->CommandList->DrawIndexed(indexCount, 1);
	}

	/**************************************************
	***		DRAW — explicit RenderPass				***
	**************************************************/

	void Renderer::DrawIndexed(RenderPass* renderPass, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, uint32_t indexCount, uint32_t instanceCount)
	{
		if (s_Data->FrameSkipped)
			return;

		indexCount = ResolveIndexCount(indexBuffer, indexCount);

		Framebuffer* target = GetCurrentTarget();
		if (!s_Data->CommandList->SetRenderPass(renderPass))
			return;

		BindTarget(target);
		s_Data->CommandList->AddVertexBuffer(vertexBuffer, 0);
		s_Data->CommandList->SetIndexBuffer(indexBuffer, 0);
		s_Data->CommandList->DrawIndexed(indexCount, instanceCount);
	}

	void Renderer::Draw(RenderPass* renderPass, uint32_t vertexCount, uint32_t instanceCount)
	{
		if (s_Data->FrameSkipped)
			return;

		Framebuffer* target = GetCurrentTarget();
		if (!s_Data->CommandList->SetRenderPass(renderPass))
			return;

		BindTarget(target);
		s_Data->CommandList->Draw(vertexCount, instanceCount);
	}

	/**************************************************
	***		DRAW — Material							***
	**************************************************/

	RenderPass* Renderer::PrepareMaterial(Material* material, const VertexLayout& layout, Framebuffer* target)
	{
		// The UBO is volatile: it must be written into every frame that binds it, not only when it changed.
		if (material->GetUniformBuffer() && material->NeedsUniformUpload(s_Data->FrameIndex))
		{
			const auto& cpu = material->GetUniformCPUData();
			s_Data->CommandList->UploadBuffer(material->GetUniformBuffer(), cpu.data(), cpu.size());
			material->MarkUniformUploaded(s_Data->FrameIndex);
		}

		RenderPass* renderPass = material->GetOrCreateRenderPass(layout, target);
		return s_Data->CommandList->SetRenderPass(renderPass) ? renderPass : nullptr;
	}

	void Renderer::DrawIndexed(Material* material, const VertexLayout& layout, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, uint32_t indexCount, uint32_t instanceCount)
	{
		if (s_Data->FrameSkipped)
			return;

		indexCount = ResolveIndexCount(indexBuffer, indexCount);

		Framebuffer* target = GetCurrentTarget();
		if (!PrepareMaterial(material, layout, target))
			return;

		BindTarget(target);
		s_Data->CommandList->AddVertexBuffer(vertexBuffer, 0);
		s_Data->CommandList->SetIndexBuffer(indexBuffer, 0);
		s_Data->CommandList->DrawIndexed(indexCount, instanceCount);
	}

	void Renderer::Draw(Material* material, uint32_t vertexCount, uint32_t instanceCount)
	{
		if (s_Data->FrameSkipped)
			return;

		Framebuffer* target = GetCurrentTarget();
		if (!PrepareMaterial(material, VertexLayout(), target))
			return;

		BindTarget(target);
		s_Data->CommandList->Draw(vertexCount, instanceCount);
	}

	/**************************************************
	***		QUERIES									***
	**************************************************/

	CommandList* Renderer::GetCommandList()
	{
		DE_CORE_ASSERT(s_Data, "Renderer used after Renderer::Destroy()");
		return s_Data ? s_Data->CommandList : nullptr;
	}

	CommandList* Renderer::TryGetRecordingCommandList()
	{
		if (!s_Data || !s_Data->CommandList || !s_Data->CommandList->IsRecording())
			return nullptr;

		return s_Data->CommandList;
	}

	Framebuffer* Renderer::GetSwapChainFramebuffer()
	{
		DE_CORE_ASSERT(s_Data, "Renderer used after Renderer::Destroy()");
		return s_Data ? s_Data->SwapChain->GetCurrentFramebuffer() : nullptr;
	}

	bool Renderer::IsSwapChainFramebuffer(const Framebuffer* framebuffer)
	{
		return s_Data && framebuffer && s_Data->SwapChain && s_Data->SwapChain->OwnsFramebuffer(framebuffer);
	}

	uint64_t Renderer::GetSwapChainResizeGeneration()
	{
		return (s_Data && s_Data->SwapChain) ? s_Data->SwapChain->GetResizeGeneration() : 0;
	}

	uint64_t Renderer::GetFrameIndex()
	{
		return s_Data ? s_Data->FrameIndex : 0;
	}

	/**************************************************
	***		STATIC RESOURCES						***
	**************************************************/

	Texture* Renderer::GetWhiteTexture()
	{
		DE_CORE_ASSERT(s_Data, "Renderer used after Renderer::Destroy()");
		return s_Data ? s_Data->WhiteTexture : nullptr;
	}

	Sampler* Renderer::GetClampSampler()
	{
		DE_CORE_ASSERT(s_Data, "Renderer used after Renderer::Destroy()");
		return s_Data ? s_Data->ClampSampler : nullptr;
	}

	Sampler* Renderer::GetPointSampler()
	{
		DE_CORE_ASSERT(s_Data, "Renderer used after Renderer::Destroy()");
		return s_Data ? s_Data->PointSampler : nullptr;
	}

	PostProcessStack& Renderer::GetPostProcessStack()
	{
		DE_CORE_ASSERT(s_Data, "Renderer used after Renderer::Destroy()");
		return s_Data->PostProcess;
	}

}
