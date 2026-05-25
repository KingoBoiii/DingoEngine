#include "depch.h"
#include "AppRenderer.h"

#include "DingoEngine/Graphics/GraphicsContext.h"

namespace Dingo
{

	AppRenderer* AppRenderer::Create(SwapChain* swapChain)
	{
		AppRenderer* appRenderer = new AppRenderer(swapChain);
		appRenderer->Initialize();
		return appRenderer;
	}

	void AppRenderer::Initialize()
	{
		Renderer::Initialize();

		m_SwapChain->AcquireNextImage();

		m_Running = true;
		m_RenderThread = std::thread(&AppRenderer::RenderThreadLoop, this);
	}

	void AppRenderer::Shutdown()
	{
		// Signal the render thread to stop and wait for it to exit cleanly.
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_Running = false;
			m_HasFrame = true; // sentinel: wake the thread even if no real frame is pending
		}
		m_FrameReadyCV.notify_one();

		if (m_RenderThread.joinable())
			m_RenderThread.join();

		Renderer::Shutdown();
	}

	// Close the command list on the main thread. The render thread will execute it.
	void AppRenderer::End()
	{
		Close();
	}

	void AppRenderer::BeginFrame()
	{
		// Block until the render thread has finished Execute + Present + GC + AcquireNextImage.
		// m_FrameConsumed is set inside the lock right before notify, so this cannot
		// return early due to a spurious wakeup while the render thread is still working.
		std::unique_lock<std::mutex> lock(m_Mutex);
		m_FrameConsumedCV.wait(lock, [this] { return m_FrameConsumed; });
		m_FrameConsumed = false;

		// Open the command list once for the entire frame so multiple renderers
		// (e.g. Renderer 3D + Renderer2D overlay) can record into the same list
		// without reopening it (which would reset all previously recorded commands).
		Begin();
	}

	void AppRenderer::EndFrame()
	{
		// Seal the command list before handing it to the render thread.
		End(); // AppRenderer::End() == Close() only; Execute() happens on the render thread.

		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_HasFrame = true;
		}
		m_FrameReadyCV.notify_one();
	}

	void AppRenderer::RenderThreadLoop()
	{
		while (true)
		{
			// Wait for the main thread to signal a frame is ready.
			bool running;
			{
				std::unique_lock<std::mutex> lock(m_Mutex);
				m_FrameReadyCV.wait(lock, [this] { return m_HasFrame; });

				running = m_Running;
				m_HasFrame = false;
			}

			if (!running)
				break;

			// GPU submission, present, GC, and next-image acquisition all happen on
			// the render thread so the main thread never touches the NVRHI device
			// concurrently with these operations.
			Execute();
			m_SwapChain->Present();
			GraphicsContext::Get().RunGarbageCollection();
			m_SwapChain->AcquireNextImage();

			// Set m_FrameConsumed inside the lock so BeginFrame's predicate can only
			// become true after all GPU work and AcquireNextImage are fully done.
			{
				std::lock_guard<std::mutex> lock(m_Mutex);
				m_FrameConsumed = true;
			}
			m_FrameConsumedCV.notify_one();
		}
	}

}
