#include <DingoEngine/EntryPoint.h>
#include <DingoPackKey.h>

#include "GameLayer.h"

namespace Dingo
{

	class SpaceInvadersApplication : public Application
	{
	public:
		SpaceInvadersApplication(const Dingo::ApplicationParams& params)
			: Application(params)
		{}
		virtual ~SpaceInvadersApplication() = default;

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
			.Title = "[Example Game] Space Invaders - Dingo Engine",
			.Width = 1600,
			.Height = 900,
			.VSync = true,
			.Resizable = false,
		},
		.Assets = Dingo::AssetManagerParams().SetPackageKey(Dingo::Generated::GetPackageKey()),
		.EnableImGui = false,
	};

	return new SpaceInvadersApplication(params);
}
