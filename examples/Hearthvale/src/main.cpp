#include <DingoEngine/EntryPoint.h>
#include <DingoPackKey.h>
#include <DingoEngine/Core/Platform.h>

#include "HearthvaleLayer.h"
#include "LaunchOptions.h"

#include <optional>
#include <string_view>

namespace Dingo
{

	class HearthvaleApplication : public Application
	{
	public:
		HearthvaleApplication(const Dingo::ApplicationParams& params)
			: Application(params)
		{}
		virtual ~HearthvaleApplication() = default;

	protected:
		virtual void OnInitialize() override
		{
			PushLayer(new HearthvaleLayer());
		}
	};

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
			DE_WARN("Hearthvale: ignoring --vsync={} (expected on/1/true or off/0/false)", *value);
	}
	return defaultValue;
}

Dingo::Application* Dingo::CreateApplication(Dingo::ApplicationCommandLineArgs args)
{
	const LaunchOptions& options = ParseLaunchOptions(args);
	// Nobody watches a scripted run, and an unfocused window would pause it.
	const bool background = IsScripted(options);

	ApplicationParams params = ApplicationParams{
		.CommandLineArgs = args,
		.Window = {
			.Title = "[Example] Hearthvale (Farming and Building) - Dingo Engine",
			.Width = 1600,
			.Height = 900,
			.VSync = ParseVSync(args, !(options.Autoplay || options.Perf)),
			.Resizable = false,
		},
		.Assets = AssetManagerParams()
			.SetPackageKey(Dingo::Generated::GetPackageKey())
			.SetRootDirectory(Platform::FindDirectoryUpward("assets").value_or("assets")),
		.EnableImGui = false,
		.UpdateInBackground = background,
	};

	return new HearthvaleApplication(params);
}
