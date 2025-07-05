#include "SandboxApplication.h"

#include <glm/vec3.hpp> // glm::vec3
#include <glm/vec4.hpp> // glm::vec4
#include <glm/mat4x4.hpp> // glm::mat4
#include <glm/ext/matrix_transform.hpp> // glm::translate, glm::rotate, glm::scale
#include <glm/ext/matrix_clip_space.hpp> // glm::perspective
#include <glm/ext/scalar_constants.hpp> // glm::pi

extern DingoEngine::Application* DingoEngine::CreateApplication()
{
	ApplicationParams params = ApplicationParams();

	SandboxApplication* app = new SandboxApplication(params);
	app->Initialize();
	return app;
}

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

	//DE_CORE_TRACE_TAG("TAG", "ENGINE {} {}", "TRACE", glm::vec2(69.0f, 420.0f));
	//DE_CORE_INFO_TAG("TAG", "ENGINE {}", "INFO");
	//DE_CORE_WARN_TAG("TAG", "ENGINE {}", "WARN");
	//DE_CORE_ERROR_TAG("TAG", "ENGINE {}", "ERROR");
	//DE_CORE_FATAL_TAG("TAG", "ENGINE {}", "FATAL");

	//DE_TRACE_TAG("TAG", "TRACE");
	//DE_INFO_TAG("TAG", "INFO");
	//DE_WARN_TAG("TAG", "WARN");
	//DE_ERROR_TAG("TAG", "ERROR");
	//DE_FATAL_TAG("TAG", "FATAL");

	DingoEngine::Application* app = DingoEngine::CreateApplication();
	app->Run();
	delete app;

	DingoEngine::Log::Shutdown();

	return 0;
}
