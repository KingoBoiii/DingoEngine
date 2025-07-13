#pragma once
#include "DingoEngine/Core/Application.h"
#include "Log.h"

extern DingoEngine::Application* DingoEngine::CreateApplication(int argc, char** argv);

namespace DingoEngine
{

	int Main(int argc, char** argv)
	{
		DingoEngine::Log::Initialize();

		DingoEngine::Application* app = DingoEngine::CreateApplication(argc, argv);
		app->Run();
		delete app;

		DingoEngine::Log::Shutdown();

		return EXIT_SUCCESS;
	}

}

#ifdef DE_PLATFORM_WINDOWS 

int main(int argc, char** argv)
{
	return DingoEngine::Main(argc, argv);
}


#endif // DE_PLATFORM_WINDOWS
