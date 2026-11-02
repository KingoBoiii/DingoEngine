#include <DingoEngine/EntryPoint.h>
#include <DingoPackKey.h>

#include <cstdlib>

#include "GameLayer.h"

namespace Dingo
{

	class FlappyBirdApplication : public Application
	{
	public:
		FlappyBirdApplication(const Dingo::ApplicationParams& params)
			: Application(params)
		{}
		virtual ~FlappyBirdApplication() = default;

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
			.Title = "[Example Game] Flappy Bird - Dingo Engine",
			.Width = 1600,
			.Height = 900,
			.VSync = true,
			.Resizable = false,
		},
		.Assets = Dingo::AssetManagerParams().SetPackageKey(Dingo::Generated::GetPackageKey()),
		.EnableImGui = false,
	};

	// A real game ships its DSN and turns Enabled on from the player's saved choice.
	if (const auto dsn = args.Get("sentry-dsn"))
		params.CrashReporting.Dsn = std::string(*dsn);
	else if (const char* dsn = std::getenv("DE_SENTRY_DSN"))
		params.CrashReporting.Dsn = dsn;
	params.CrashReporting.Enabled = !params.CrashReporting.Dsn.empty();
	params.CrashReporting.Environment = "example";
	params.CrashReporting.SendAttachments = args.Get("sentry-attachments").has_value();

	return new FlappyBirdApplication(params);
}
