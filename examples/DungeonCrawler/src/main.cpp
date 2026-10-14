#include <DingoEngine/EntryPoint.h>
#include <DingoPackKey.h>

#include "GameLayer.h"

namespace Dingo
{

	class DungeonCrawlerApplication : public Application
	{
	public:
		DungeonCrawlerApplication(const Dingo::ApplicationParams& params)
			: Application(params)
		{}
		virtual ~DungeonCrawlerApplication() = default;

	protected:
		virtual void OnInitialize() override
		{
			PushLayer(new GameLayer());
		}
	};

}


Dingo::Application* Dingo::CreateApplication(Dingo::ApplicationCommandLineArgs args)
{
	ApplicationParams params = ApplicationParams{
		.CommandLineArgs = args,
		.Window = {
			.Title = "[Example Game] Dungeon Crawler - Dingo Engine",
			.Width = 1600,
			.Height = 900,
			.VSync = true,
			.Resizable = false,
		},
		.Assets = Dingo::AssetManagerParams().SetPackageKey(Dingo::Generated::GetPackageKey()),
		.EnableImGui = false,
	};

	return new DungeonCrawlerApplication(params);
}
