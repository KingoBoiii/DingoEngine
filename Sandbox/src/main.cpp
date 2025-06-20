#include <iostream>
#include <DingoEngine.h>

int main()
{
	DingoEngine::Window* window = new DingoEngine::Window();
	window->Initialize();

	while (window->IsRunning())
	{
		window->Update();
	}

	window->Shutdown();
	delete window;

	std::cout << "Hello, world!" << std::endl;

	return 0;
}
