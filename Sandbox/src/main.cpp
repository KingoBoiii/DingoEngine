#include "SandboxApplication.h"

extern DingoEngine::Application* DingoEngine::CreateApplication()
{
	ApplicationParams params = ApplicationParams();

	SandboxApplication* app = new SandboxApplication(params);
	app->Initialize();
	return app;
}

int main()
{
	DingoEngine::Log::Initialize();

	DingoEngine::Application* app = DingoEngine::CreateApplication();
	app->Run();
	delete app;

	DingoEngine::Log::Shutdown();

	return 0;
}
