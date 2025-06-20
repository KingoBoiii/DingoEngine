#pragma once
#include "DingoEngine/Common.h"

struct GLFWwindow;

namespace DingoEngine
{

	struct WindowOptions
	{
		std::string Title;
		int32_t Width;
		int32_t Height;
		GraphicsAPI GraphicsAPI = GraphicsAPI::Vulkan;

		WindowOptions(std::string title = "DingoEngine Application", int32_t width = 1600, int32_t height = 900)
			: Title(title), Width(width), Height(height)
		{}
	};

	class Window
	{
	public:
		Window(const WindowOptions& options = {});
		~Window() = default;
	public:
		void Initialize();
		void Shutdown();

		void Update();

		bool IsRunning() const;
	private:
		WindowOptions m_Options;
		GLFWwindow* m_WindowHandle = nullptr;
	};

}
