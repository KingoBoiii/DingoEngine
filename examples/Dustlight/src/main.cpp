#include <DingoEngine/EntryPoint.h>
#include <DingoPackKey.h>
#include <DingoEngine/Core/Platform.h>

#include "DustlightLayer.h"
#include "LaunchOptions.h"

#include <optional>
#include <string_view>

namespace Dingo
{

	class DustlightApplication : public Application
	{
	public:
		DustlightApplication(const Dingo::ApplicationParams& params)
			: Application(params)
		{}
		virtual ~DustlightApplication() = default;

	protected:
		virtual void OnInitialize() override
		{
			PushLayer(new DustlightLayer());
		}
	};

}

Dingo::Application* Dingo::CreateApplication(Dingo::ApplicationCommandLineArgs args)
{
	ApplicationParams params = ApplicationParams{
		.CommandLineArgs = args,
		.Window = {
			.Title = "[Example] Dustlight (PBR, IBL and Volumetric Fog) - Dingo Engine",
			.Width = 1600,
			.Height = 900,
			.VSync = true,
			.Resizable = false,
		},
		.Assets = AssetManagerParams()
			.SetPackageKey(Dingo::Generated::GetPackageKey())
			.SetRootDirectory(Platform::FindDirectoryUpward("assets").value_or("assets")),
		.EnableImGui = true,
	};
	params.Renderer3D.Capabilities.Volumetrics = true;
	params.Renderer3D.Capabilities.MaxFogVolumes = 8;
	params.Renderer3D.Shadows.MaxDistance = 30.0f;

	const LaunchOptions options = ParseLaunchOptions(args);
	params.UpdateInBackground = options.Check || options.Freeze;

	return new DustlightApplication(params);
}
