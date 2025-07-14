#pragma once
#include "DingoEngine/Graphics/SwapChain.h"

namespace Dingo
{

	struct AppRendererParams
	{
		SwapChain* SwapChain = nullptr;			// Pointer to the swap chain used for rendering
	};

	class AppRenderer
	{
	private:
		static void Initialize(const AppRendererParams& params);
		static void Shutdown();

		static void BeginFrame();
		static void EndFrame();
		static void Present();
		static void RunGarbageCollection();

	private:
		friend class Application;
	};

}
