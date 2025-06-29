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

	DingoEngine::WindowParams windowParams = DingoEngine::WindowParams()
		.SetResizable(false);

	DingoEngine::Window* window = new DingoEngine::Window(windowParams);
	window->Initialize();

	DingoEngine::Renderer* renderer = DingoEngine::Renderer::Create(window->GetSwapChain());
	renderer->Initialize();

	DingoEngine::ShaderParams shaderParams = DingoEngine::ShaderParams()
		.SetName("StaticTriangle")
		.AddShaderType(DingoEngine::ShaderType::Vertex, "assets/shaders/static_triangle.vert.spv")
		.AddShaderType(DingoEngine::ShaderType::Fragment, "assets/shaders/static_triangle.frag.spv");

	DingoEngine::Shader* shader = DingoEngine::Shader::Create(shaderParams);
	shader->Initialize();

	DingoEngine::PipelineParams pipelineParams = DingoEngine::PipelineParams()
		.SetShader(shader)
		.SetFramebuffer(window->GetSwapChain()->GetFramebuffer(0))
		.SetFillMode(DingoEngine::FillMode::Solid)
		.SetCullMode(DingoEngine::CullMode::BackAndFront);

	DingoEngine::Pipeline* pipeline = DingoEngine::Pipeline::Create(pipelineParams);
	pipeline->Initialize();

	DingoEngine::CommandList* commandList = DingoEngine::CommandList::Create();
	commandList->Initialize();

	while (window->IsRunning())
	{
		window->Update();

		renderer->BeginFrame();

		commandList->Begin(window->GetSwapChain()->GetCurrentFramebuffer(), pipeline);
		commandList->Draw();
		commandList->End();

		renderer->EndFrame();
		renderer->Present();
		renderer->WaitAndClear();
	}

	commandList->Destroy();

	pipeline->Destroy();

	shader->Destroy();

	renderer->Destroy();
	delete renderer;

	window->Shutdown();
	delete window;

	DingoEngine::Log::Shutdown();

	std::cout << "Hello, world!" << std::endl;

	return 0;
}
