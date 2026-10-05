#include <DingoEngine/EntryPoint.h>
#include <DingoEngine/Core/Platform.h>

#include "LaunchOptions.h"
#include "LiveEdit.h"
#include "MarionetteLayer.h"

#include <filesystem>
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

static bool ParseVSync(const Dingo::ApplicationCommandLineArgs& args, bool defaultValue)
{
	if (const std::optional<std::string_view> value = args.Get("vsync"))
	{
		if (*value == "off" || *value == "0" || *value == "false")
			return false;
		if (*value == "on" || *value == "1" || *value == "true")
			return true;
		if (!value->empty())
			DE_WARN("Marionette: ignoring --vsync={} (expected on/1/true or off/0/false)", *value);
	}
	return defaultValue;
}

Dingo::Application* Dingo::CreateApplication(Dingo::ApplicationCommandLineArgs args)
{
	const LaunchOptions& options = ParseLaunchOptions(args);
	// Nobody watches a scripted run, and the long checks in OnAttach leave the window unfocused, which would pause it.
	// A hot-reload run is edited from another window, so it is unfocused exactly when the edit lands.
	const bool background = IsScripted(options) || options.HotReload;

	// The live-edit demo rewrites a .events file, so it reads a copy of the assets and never the repository's own.
	std::filesystem::path assetRoot = Platform::FindDirectoryUpward("assets").value_or("assets");
	if (options.LiveEditDemo && PrepareLiveEditAssets(assetRoot))
		assetRoot = GetLiveEditRoot();

	ApplicationParams params = ApplicationParams{
		.CommandLineArgs = args,
		.Window = {
			.Title = "[Example] Marionette (Skeletal Animation) - Dingo Engine",
			.Width = 1600,
			.Height = 900,
			.VSync = ParseVSync(args, options.Tournament == 0),
			.Resizable = false,
		},
		.Graphics = {
			.GraphicsAPI = ParseGraphicsAPI(args),
			.FramesInFlight = 3,
		},
		.Assets = AssetManagerParams()
			.SetRootDirectory(assetRoot)
			.SetEnableHotReload(options.HotReload),
		.EnableUI = false,
		.UpdateInBackground = background,
	};

	MarionetteApplication* app = new MarionetteApplication(params);
	app->Initialize();
	return app;
}
