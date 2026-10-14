#include <DingoEngine/EntryPoint.h>
#include <DingoPackKey.h>

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
		.Assets = Dingo::AssetManagerParams().SetPackageKey(Dingo::Generated::GetPackageKey()),
		.EnableImGui = false,
	};

	return new DungeonCrawler3DApplication(params);
}
