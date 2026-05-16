#pragma once
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/SwapChain.h"
#include "DingoEngine/Graphics/CommandList.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace Dingo
{

	// AppRenderer owns the render thread. The main thread records commands
	// (Begin → draw calls → Close) and signals the render thread at EndFrame.
	// The render thread executes the sealed command list and presents the frame.
	class AppRenderer : public Renderer
	{
	public:
		static AppRenderer* Create(SwapChain* swapChain);

	public:
		AppRenderer(SwapChain* swapChain)
			: Renderer(RendererParams{ .TargetFramebuffer = swapChain->GetCurrentFramebuffer() }, true), m_SwapChain(swapChain)
		{}
		virtual ~AppRenderer() = default;

		virtual void Initialize() override;
		virtual void Shutdown() override;

		// Overridden to close-only: execution is deferred to the render thread.
		virtual void End() override;

		// Wait for the render thread to release the previous frame, then acquire the next swap chain image.
		void BeginFrame();

		// Signal the render thread to execute the sealed command list and present.
		void EndFrame();

	private:
		void RenderThreadLoop();

		SwapChain* m_SwapChain = nullptr;

		std::thread m_RenderThread;
		std::mutex m_Mutex;
		std::condition_variable m_FrameReadyCV;    // render thread waits here
		std::condition_variable m_FrameConsumedCV; // main thread waits here in BeginFrame
		bool m_HasFrame = false;       // set by main in EndFrame; cleared by render thread
		bool m_FrameConsumed = true;   // set by render thread after all GPU work + acquire; cleared by BeginFrame
		bool m_Running = false;
	};

}
