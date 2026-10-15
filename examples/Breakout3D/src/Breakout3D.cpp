#include <DingoEngine/EntryPoint.h>
#include <DingoPackKey.h>

#include "BreakoutLayer.h"

namespace Dingo
{

	class Breakout3DApplication : public Application
	{
	public:
		Breakout3DApplication(const ApplicationParams& params)
			: Application(params)
		{}

	protected:
		void OnInitialize() override
		{
			PushLayer(new BreakoutLayer());
		}
	};

}

Dingo::Application* Dingo::CreateApplication(Dingo::ApplicationCommandLineArgs args)
{
	Dingo::ApplicationParams params = Dingo::ApplicationParams{
		.CommandLineArgs = args,
		.Window = {
			.Title = "[Example Game] Breakout 3D - DingoEngine",
			.Width = 1280,
			.Height = 720,
			.VSync = true,
			.Resizable = true,
		},
		.Assets = Dingo::AssetManagerParams().SetPackageKey(Dingo::Generated::GetPackageKey()),
		.EnableImGui = true,
	};

	return new Dingo::Breakout3DApplication(params);
}
