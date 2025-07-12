#include "SandboxApplication.h"

extern DingoEngine::Application* DingoEngine::CreateApplication()
{
	ApplicationParams params = ApplicationParams();
	params.EnableImGui = true; // Enable ImGui for the sandbox application
	params.ImGuiParams = {
		.EnableDocking = true, // Enable docking by default
	};

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
