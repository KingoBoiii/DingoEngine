#include <DingoEngine/EntryPoint.h>

#include "GameLayer.h"

namespace Dingo
{

	class FlappyBirdApplication : public Application
	{
	public:
		FlappyBirdApplication(const Dingo::ApplicationParams& params)
			: Application(params)
		{}
		virtual ~FlappyBirdApplication() = default;

	protected:
		virtual void OnInitialize() override
		{
			PushLayer(new GameLayer());
		}
	};

}


Dingo::Application* Dingo::CreateApplication(int argc, char** argv)
{
	ApplicationParams params = ApplicationParams{
		.Window = {
			.Title = "[Example Game] Flappy Bird - Dingo Engine",		// Window title
			.Width = 1600,												// Window width
			.Height = 900,												// Window height
			.VSync = true,												// Enable VSync by default
			.Resizable = true,											// Make the window resizable by default
		},
		.Graphics = {
			.GraphicsAPI = GraphicsAPI::Vulkan,							// Default to Vulkan
			.FramesInFlight = 3,										// Number of frames in flight for Rendering
		},
#if DE_DEBUG
		.EnableImGui = true,											// Enable ImGui by default
		.ImGui = {
			.EnableDocking = true,										// Enable docking by default
			.EnableViewports = false,									// Disable viewports by default
		}
#endif
	};

	FlappyBirdApplication* app = new FlappyBirdApplication(params);
	app->Initialize();
	return app;
}
