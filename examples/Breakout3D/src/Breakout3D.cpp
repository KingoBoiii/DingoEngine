#include <DingoEngine/EntryPoint.h>

#include "BreakoutLayer.h"

namespace Dingo
{

	class Breakout3DApplication : public Application
	{
	public:
		Breakout3DApplication(const ApplicationParams& params)
			: Application(params)
		{}

	protected:
		void OnInitialize() override
		{
			PushLayer(new BreakoutLayer());
		}
	};

}

static Dingo::GraphicsAPI ParseGraphicsAPI(const Dingo::ApplicationCommandLineArgs& args)
{
	if (auto val = args.Get("graphics"))
	{
		if (*val == "vulkan")  return Dingo::GraphicsAPI::Vulkan;
		if (*val == "dx11")    return Dingo::GraphicsAPI::DirectX11;
		if (*val == "dx12")    return Dingo::GraphicsAPI::DirectX12;
	}
	return Dingo::GraphicsAPI::Vulkan;
}

Dingo::Application* Dingo::CreateApplication(Dingo::ApplicationCommandLineArgs args)
{
	Dingo::ApplicationParams params = Dingo::ApplicationParams{
		.CommandLineArgs = args,
		.Window = {
			.Title = "[Example Game] Breakout 3D - DingoEngine",
			.Width = 1280,
			.Height = 720,
			.VSync = true,
			.Resizable = false,
		},
		.Graphics = {
			.GraphicsAPI = ParseGraphicsAPI(args),
			.FramesInFlight = 3,
		},
		.EnableImGui = true,
	};

	Dingo::Breakout3DApplication* app = new Dingo::Breakout3DApplication(params);
	app->Initialize();
	return app;
}
