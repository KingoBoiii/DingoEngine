#include <DingoEngine/EntryPoint.h>

#include "TestLayer.h"

namespace Dingo
{

	class TestFrameworkApplication : public Application
	{
	public:
		TestFrameworkApplication(const Dingo::ApplicationParams& params)
			: Application(params)
		{}
		virtual ~TestFrameworkApplication() = default;

	protected:
		virtual void OnInitialize() override
		{
			PushLayer(new TestLayer());
		}
	};

}


Dingo::Application* Dingo::CreateApplication(int argc, char** argv)
{
	ApplicationParams params = ApplicationParams{
		.Window = {
			.Title = "Dingo Test Framework",
			.Width = 1600,
			.Height = 900,
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

	TestFrameworkApplication* app = new TestFrameworkApplication(params);
	app->Initialize();
	return app;
}
