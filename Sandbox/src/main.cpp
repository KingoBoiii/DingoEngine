#include <iostream>
#include <DingoEngine.h>

int main()
{
	DingoEngine::Log::Initialize();

	DE_TRACE("TRACE");
	DE_INFO("INFO");
	DE_WARN("WARN");
	DE_ERROR("ERROR");
	DE_FATAL("FATAL");

	DingoEngine::Window* window = new DingoEngine::Window();
	window->Initialize();

	while (window->IsRunning())
	{
		window->Update();
	}

	window->Shutdown();
	delete window;

	DingoEngine::Log::Shutdown();

	std::cout << "Hello, world!" << std::endl;

	return 0;
}
