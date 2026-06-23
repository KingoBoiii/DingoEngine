#include <DingoEngine/EntryPoint.h>

#include "PhysicsLayer.h"

namespace Dingo
{

	class Physics3DApplication : public Application
	{
	public:
		Physics3DApplication(const Dingo::ApplicationParams& params)
			: Application(params)
		{}
		virtual ~Physics3DApplication() = default;

	protected:
		virtual void OnInitialize() override
		{
			PushLayer(new PhysicsLayer());
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
	ApplicationParams params = ApplicationParams{
		.CommandLineArgs = args,
		.Window = {
			.Title = "[Example] 3D Physics (Jolt) - Dingo Engine",
			.Width = 1600,
			.Height = 900,
			.VSync = true,
			.Resizable = false,
		},
		.Graphics = {
			.GraphicsAPI = ParseGraphicsAPI(args),
			.FramesInFlight = 3,
		},
		.EnableUI = false,
	};

	Physics3DApplication* app = new Physics3DApplication(params);
	app->Initialize();
	return app;
}
