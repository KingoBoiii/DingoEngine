#include <iostream>
#include <DingoEngine.h>

#include <glm/vec3.hpp> // glm::vec3
#include <glm/vec4.hpp> // glm::vec4
#include <glm/mat4x4.hpp> // glm::mat4
#include <glm/ext/matrix_transform.hpp> // glm::translate, glm::rotate, glm::scale
#include <glm/ext/matrix_clip_space.hpp> // glm::perspective
#include <glm/ext/scalar_constants.hpp> // glm::pi

glm::mat4 camera(float Translate, glm::vec2 const& Rotate)
{
	glm::mat4 Projection = glm::perspective(glm::pi<float>() * 0.25f, 4.0f / 3.0f, 0.1f, 100.f);
	glm::mat4 View = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -Translate));
	View = glm::rotate(View, Rotate.y, glm::vec3(-1.0f, 0.0f, 0.0f));
	View = glm::rotate(View, Rotate.x, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 Model = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
	return Projection * View * Model;
}

int main()
{
	const auto& cam = camera(5.0f, glm::vec2(0.0f, 0.0f));

	DingoEngine::Log::Initialize();

	DE_TRACE("TRACE");
	DE_INFO("INFO");
	DE_WARN("WARN");
	DE_ERROR("ERROR");
	DE_FATAL("FATAL");

	DingoEngine::Window* window = new DingoEngine::Window();
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
