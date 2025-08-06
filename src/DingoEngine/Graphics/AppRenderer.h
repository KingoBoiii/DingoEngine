#pragma once
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/SwapChain.h"
#include "DingoEngine/Graphics/CommandList.h"

namespace Dingo
{

	class AppRenderer : public Renderer
	{
	public:
		static AppRenderer* Create(SwapChain* swapChain);

	public:
		AppRenderer(SwapChain* swapChain)
			: Renderer(RendererParams { .TargetFramebuffer = swapChain->GetCurrentFramebuffer() }), m_SwapChain(swapChain)
		{}
		virtual ~AppRenderer() = default;

		void BeginFrame();
		void EndFrame();

	private:
		SwapChain* m_SwapChain = nullptr;


	};

}
