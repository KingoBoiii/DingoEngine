#include <DingoEngine/EntryPoint.h>

#include "TestLayer.h"

namespace Dingo
{

	class TestFrameworkApplication : public Application
	{
	public:
		TestFrameworkApplication(const Dingo::ApplicationParams& params)
			: Application(params)
		{}
		virtual ~TestFrameworkApplication() = default;

	protected:
		virtual void OnInitialize() override
		{
			PushLayer(new TestLayer());
		}
	};

}

Dingo::Application* Dingo::CreateApplication(ApplicationCommandLineArgs args)
{
	GraphicsAPI graphicsAPI = GraphicsAPI::Vulkan;
	if (Application::HasPendingRestart())
	{
		graphicsAPI = Application::ConsumePendingRestart();
	}
	else if (auto value = args.Get("graphics"))
	{
		if (*value == "dx12" || *value == "directx12")  graphicsAPI = GraphicsAPI::DirectX12;
		else if (*value == "dx11" || *value == "directx11") graphicsAPI = GraphicsAPI::DirectX11;
	}

	ApplicationParams params = ApplicationParams{
		.CommandLineArgs = args,
		.Window = {
			.Title = "Dingo Test Framework",
			.Width = 1600,
			.Height = 900,
			.VSync = true,
			.Resizable = true,
		},
		.Graphics = {
			.GraphicsAPI = graphicsAPI,
			.FramesInFlight = 3,
		},
		.Assets = {
			.EnableHotReload = true,
		},
		.EnableUI = true,
		.UI = {
			.EnableDocking = true,
			.EnableViewports = false,
		}
	};

	TestFrameworkApplication* app = new TestFrameworkApplication(params);
	app->Initialize();
	return app;
}
