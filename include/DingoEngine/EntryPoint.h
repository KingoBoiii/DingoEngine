#pragma once
#include "DingoEngine/Core/Application.h"
#include "Log.h"

extern Dingo::Application* Dingo::CreateApplication(Dingo::ApplicationCommandLineArgs args);

namespace Dingo
{

	int Main(int argc, char** argv)
	{
		Dingo::Log::Initialize();

		Dingo::ApplicationCommandLineArgs args = { argc, argv };

		do
		{
			Dingo::Application* app = Dingo::CreateApplication(args);
			app->Run();
			bool shouldRestart = Dingo::Application::HasPendingRestart();
			delete app;

			if (!shouldRestart)
				break;
		}
		while (true);

		Dingo::Log::Shutdown();

		return EXIT_SUCCESS;
	}

}

#if DE_DISTRIBUTION && DE_PLATFORM_WINDOWS

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
	return Dingo::Main(__argc, __argv);
}

#else

int main(int argc, char** argv)
{
	return Dingo::Main(argc, argv);
}

#endif // DE_PLATFORM_WINDOWS
