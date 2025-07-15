#include <DingoEngine/EntryPoint.h>

#include "SandboxApplication.h"

Dingo::Application* Dingo::CreateApplication(int argc, char** argv)
{
	ApplicationParams params = ApplicationParams{
		.Window = {
			.Title = "DingoEngine Sandbox",
			.Width = 1600,
			.Height = 900,
			.GraphicsAPI = GraphicsAPI::Vulkan,		// Default to Vulkan
			.VSync = true,							// Enable VSync by default
			.Resizable = true,						// Make the window resizable by default
		},
		.Graphics = {
			.GraphicsAPI = GraphicsAPI::Vulkan,		// Default to Vulkan
			.FramesInFlight = 3,					// Number of frames in flight for Rendering
		},
		.EnableImGui = true,						// Enable ImGui by default
		.ImGui = {
			.EnableDocking = true,					// Enable docking by default
			.EnableViewports = false,				// Disable viewports by default
		}
	};

	SandboxApplication* app = new SandboxApplication(params);
	app->Initialize();
	return app;
}
