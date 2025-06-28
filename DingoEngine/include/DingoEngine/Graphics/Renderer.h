#pragma once
#include <nvrhi/nvrhi.h>

#include "SwapChain.h"
#include "GraphicsContext.h"

namespace DingoEngine
{

	class Renderer
	{
	public:
		static Renderer* Create(SwapChain* swapChain);
	public:
		Renderer(SwapChain* swapChain);
		~Renderer() = default;

	public:
		void Initialize();
		void Destroy();

		void BeginFrame();
		void EndFrame();
		void Present();
		void WaitAndClear();

	private:
		SwapChain* m_SwapChain = nullptr; // non-owning pointer, managed by the Window class
		nvrhi::IDevice* m_NvrhiDevice = nullptr; // non-owning pointer, managed by the GraphicsContext class
	};

}
