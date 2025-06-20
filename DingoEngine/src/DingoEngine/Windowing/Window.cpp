#include "depch.h"
#include "DingoEngine/Windowing/Window.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include <glfw/glfw3.h>

namespace DingoEngine
{

	Window::Window(const WindowOptions& options)
		: m_Options(options)
	{}

	void Window::Initialize()
	{
		if (glfwInit() == GLFW_FALSE)
		{
			throw new std::exception("Failed to initialize GLFW.");
		}

		m_WindowHandle = glfwCreateWindow(m_Options.Width, m_Options.Height, m_Options.Title.c_str(), nullptr, nullptr);
		if (!m_WindowHandle)
		{
			glfwTerminate();
			throw new std::exception("Failed to create GLFW window.");
		}

		GraphicsContext* gfxCtx = GraphicsContext::Create(GraphicsAPI::Vulkan);
		gfxCtx->Initialize();

		glfwMakeContextCurrent(m_WindowHandle);
	}

	void Window::Shutdown()
	{
		glfwDestroyWindow(m_WindowHandle);
		glfwTerminate();
	}

	void Window::Update()
	{
		glfwSwapBuffers(m_WindowHandle);
		glfwPollEvents();
	}

	bool Window::IsRunning() const
	{
		return glfwWindowShouldClose(m_WindowHandle) == GLFW_FALSE;
	}

}
