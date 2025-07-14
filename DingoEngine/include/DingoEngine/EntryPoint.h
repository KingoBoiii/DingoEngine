#pragma once
#include "DingoEngine/Core/Application.h"
#include "Log.h"

extern Dingo::Application* Dingo::CreateApplication(int argc, char** argv);

namespace Dingo
{

	int Main(int argc, char** argv)
	{
		Dingo::Log::Initialize();

		Dingo::Application* app = Dingo::CreateApplication(argc, argv);
		app->Run();
		delete app;

		Dingo::Log::Shutdown();

		return EXIT_SUCCESS;
	}

}

#ifdef DE_PLATFORM_WINDOWS 

int main(int argc, char** argv)
{
	return Dingo::Main(argc, argv);
}


#endif // DE_PLATFORM_WINDOWS
