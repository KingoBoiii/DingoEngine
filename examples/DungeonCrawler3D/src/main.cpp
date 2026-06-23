#include <DingoEngine/EntryPoint.h>

#include "DungeonLayer.h"

namespace Dingo
{

	class DungeonCrawler3DApplication : public Application
	{
	public:
		DungeonCrawler3DApplication(const Dingo::ApplicationParams& params)
			: Application(params)
		{}
		virtual ~DungeonCrawler3DApplication() = default;

	protected:
		virtual void OnInitialize() override
		{
			PushLayer(new DungeonLayer());
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
			.Title = "[Example] Dungeon Crawler 3D (Jolt + 3D Scene) - Dingo Engine",
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

	DungeonCrawler3DApplication* app = new DungeonCrawler3DApplication(params);
	app->Initialize();
	return app;
}
