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

	DingoEngine::Shader* shader = DingoEngine::Shader::Create("assets/shaders/static_triangle.vert.spv", "assets/shaders/static_triangle.frag.spv");
	shader->Initialize();

	DingoEngine::Pipeline* pipeline = DingoEngine::Pipeline::Create(shader, window->GetSwapChain()->GetFramebuffer(0));
	pipeline->Initialize();

	DingoEngine::CommandList* commandList = DingoEngine::CommandList::Create();
	commandList->Initialize();

	while (window->IsRunning())
	{
		window->Update();

		window->GetSwapChain()->BeginFrame();

		commandList->Begin(pipeline);
		commandList->Clear(window->GetSwapChain()->GetFramebuffer(0));
		commandList->End();

		window->GetSwapChain()->Present();
	}

	commandList->Destroy();

	pipeline->Destroy();

	shader->Destroy();

	window->Shutdown();
	delete window;

	DingoEngine::Log::Shutdown();

	std::cout << "Hello, world!" << std::endl;

	return 0;
}
