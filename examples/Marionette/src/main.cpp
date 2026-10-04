#include <DingoEngine/EntryPoint.h>
#include <DingoEngine/Core/Platform.h>

#include "LaunchOptions.h"
#include "MarionetteLayer.h"

#include <optional>
#include <string_view>

namespace Dingo
{

	class MarionetteApplication : public Application
	{
	public:
		MarionetteApplication(const Dingo::ApplicationParams& params)
			: Application(params)
		{}
		virtual ~MarionetteApplication() = default;

	protected:
		virtual void OnInitialize() override
		{
			PushLayer(new MarionetteLayer());
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

static bool ParseVSync(const Dingo::ApplicationCommandLineArgs& args)
{
	if (const std::optional<std::string_view> value = args.Get("vsync"))
	{
		if (*value == "off" || *value == "0" || *value == "false")
			return false;
		if (!value->empty() && *value != "on" && *value != "1" && *value != "true")
			DE_WARN("Marionette: ignoring --vsync={} (expected on/1/true or off/0/false)", *value);
	}
	return true;
}

Dingo::Application* Dingo::CreateApplication(Dingo::ApplicationCommandLineArgs args)
{
	ApplicationParams params = ApplicationParams{
		.CommandLineArgs = args,
		.Window = {
			.Title = "[Example] Marionette (Skeletal Animation) - Dingo Engine",
			.Width = 1600,
			.Height = 900,
			.VSync = ParseVSync(args),
			.Resizable = false,
		},
		.Graphics = {
			.GraphicsAPI = ParseGraphicsAPI(args),
			.FramesInFlight = 3,
		},
		.Assets = AssetManagerParams()
			.SetRootDirectory(Platform::FindDirectoryUpward("assets").value_or("assets"))
			.SetEnableHotReload(ParseLaunchOptions(args).HotReload),
		.EnableUI = false,
	};

	MarionetteApplication* app = new MarionetteApplication(params);
	app->Initialize();
	return app;
}
