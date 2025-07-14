#include "depch.h"
#include "AppRenderer.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include <nvrhi/nvrhi.h>

namespace Dingo
{

	struct AppRendererData
	{
		AppRendererParams Params;				// Parameters for the AppRenderer

		SwapChain* SwapChain = nullptr;			// Pointer to the swap chain used for rendering
		nvrhi::DeviceHandle Device;				// Graphics device
	};

	static AppRendererData* s_AppRendererData = nullptr;

	void AppRenderer::Initialize(const AppRendererParams& params)
	{
		s_AppRendererData = new AppRendererData{
			.Params = params,
			.SwapChain = params.SwapChain,
			.Device = GraphicsContext::Get().GetDeviceHandle()
		};
	}

	void AppRenderer::Shutdown()
	{}

	void AppRenderer::BeginFrame()
	{
		s_AppRendererData->SwapChain->AcquireNextImage();
	}

	void AppRenderer::EndFrame()
	{
	}

	void AppRenderer::Present()
	{
		s_AppRendererData->SwapChain->Present();
	}

	void AppRenderer::RunGarbageCollection()
	{
		s_AppRendererData->Device->runGarbageCollection();
	}

}
