#include "depch.h"
#include "DingoEngine/Core/Application.h"
#include "DingoEngine/Core/LogLifecycle.h"
#include "DingoEngine/Core/CrashReporter.h"

namespace Dingo::Internal
{

	int Main(int argc, char** argv)
	{
		InitializeLog();

		const ApplicationCommandLineArgs args = { argc, argv };
		while (true)
		{
			Application* app = CreateApplication(args);
			app->Initialize();
			app->Run();
			const bool restart = Application::s_PendingRestart;
			delete app;

			if (!restart)
				break;
		}

		CrashReporter::Shutdown();
		ShutdownLog();
		return EXIT_SUCCESS;
	}

}
