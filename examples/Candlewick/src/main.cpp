#include <DingoEngine/EntryPoint.h>

#include "CandlewickLayer.h"

namespace Dingo
{

	class CandlewickApplication : public Application
	{
	public:
		CandlewickApplication(const Dingo::ApplicationParams& params)
			: Application(params)
		{}
		virtual ~CandlewickApplication() = default;

	protected:
		virtual void OnInitialize() override
		{
			PushLayer(new CandlewickLayer());
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
			.Title = "[Example] Candlewick (Point + Spot Lighting) - Dingo Engine",
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

	CandlewickApplication* app = new CandlewickApplication(params);
	app->Initialize();
	return app;
}
