#include <DingoEngine/EntryPoint.h>
#include <DingoPackKey.h>

#include "GameLayer.h"

#include <format>

namespace Dingo
{

	class ArenaShooterApplication : public Application
	{
	public:
		ArenaShooterApplication(const ApplicationParams& params)
			: Application(params)
		{}
		virtual ~ArenaShooterApplication() = default;

	protected:
		virtual void OnInitialize() override
		{
			PushLayer(new GameLayer());
		}
	};

}

Dingo::Application* Dingo::CreateApplication(Dingo::ApplicationCommandLineArgs args)
{
	const uint32_t gameVersion = GameLayer::GetGameVersion();
	ApplicationParams params = ApplicationParams{
		.CommandLineArgs = args,
		.Window = {
			.Title = std::format("[Example Game] Arena Shooter - Dingo Engine (v{}.{}.{})",
				DE_VERSION_MAJOR(gameVersion), DE_VERSION_MINOR(gameVersion), DE_VERSION_PATCH(gameVersion)),
			.Width = 1600,
			.Height = 900,
			.VSync = true,
			.Resizable = false,
		},
		.Assets = AssetManagerParams()
			.SetPackageKey(Dingo::Generated::GetPackageKey())
			.SetRootDirectory("assets")
			.SetEnableHotReload(true),
		.EnableImGui = false,
	};

	return new ArenaShooterApplication(params);
}
