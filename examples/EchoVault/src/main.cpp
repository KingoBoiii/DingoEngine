#include <DingoEngine/EntryPoint.h>
#include <DingoPackKey.h>

#include "EchoVaultLayer.h"

namespace Dingo
{

	class EchoVaultApplication : public Application
	{
	public:
		EchoVaultApplication(const Dingo::ApplicationParams& params)
			: Application(params)
		{}
		virtual ~EchoVaultApplication() = default;

	protected:
		virtual void OnInitialize() override
		{
			PushLayer(new EchoVaultLayer());
		}
	};

}

Dingo::Application* Dingo::CreateApplication(Dingo::ApplicationCommandLineArgs args)
{
	ApplicationParams params = ApplicationParams{
		.CommandLineArgs = args,
		.Window = {
			.Title = "[Example] EchoVault (Character Controller + 3D Audio) - Dingo Engine",
			.Width = 1600,
			.Height = 900,
			.VSync = true,
			.Resizable = false,
		},
		.Assets = Dingo::AssetManagerParams().SetPackageKey(Dingo::Generated::GetPackageKey()),
		.EnableImGui = false,
	};

	return new EchoVaultApplication(params);
}
