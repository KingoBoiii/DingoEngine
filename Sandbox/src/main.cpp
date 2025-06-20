#include <iostream>
#include <DingoEngine.h>

int main()
{
	DingoEngine::Window* window = new DingoEngine::Window();
	window->Initialize();

	window->Shutdown();
	delete window;

	std::cout << "Hello, world!" << std::endl;

	return 0;
}
