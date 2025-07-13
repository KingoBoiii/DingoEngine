#include <DingoEngine/EntryPoint.h>

#include "SandboxApplication.h"

DingoEngine::Application* DingoEngine::CreateApplication(int argc, char** argv)
{
	ApplicationParams params = ApplicationParams();
	params.EnableImGui = true; // Enable ImGui by default
	params.ImGui = {
		.EnableDocking = true, // Enable docking by default
	};

	SandboxApplication* app = new SandboxApplication(params);
	app->Initialize();
	return app;
}
