#include "depch.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

namespace DingoEngine
{

	Renderer* Renderer::Create(SwapChain* swapChain)
	{
		return new Renderer(swapChain);
	}

	Renderer::Renderer(SwapChain* swapChain)
		: m_SwapChain(swapChain), m_NvrhiDevice(GraphicsContext::GetDeviceHandle())
	{}

	void Renderer::Initialize()
	{}

	void Renderer::Destroy()
	{
		m_NvrhiDevice->waitForIdle();

		m_NvrhiDevice->runGarbageCollection();
	}

	void Renderer::BeginFrame()
	{
		m_SwapChain->AcquireNextImage();
	}

	void Renderer::EndFrame()
	{}

	void Renderer::Present()
	{
		m_SwapChain->Present();
	}

	void Renderer::WaitAndClear()
	{
		m_NvrhiDevice->waitForIdle();

		m_NvrhiDevice->runGarbageCollection();
	}

}


