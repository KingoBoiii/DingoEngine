#include "depch.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Windowing/Window.h"
#include "DingoEngine/Events/WindowEvents.h"

#include <glfw/glfw3.h>

namespace Dingo
{

	static void GLFWErrorCallback(int error, const char* description)
	{
		DE_CORE_ERROR_TAG("GLFW", "GLFW Error ({0}): {1}", error, description);
	}

	Window::Window(const WindowParams& params)
		: m_Params(params), m_Data({ .Width = m_Params.Width, .Height = m_Params.Height })
	{}

	void Window::Initialize()
	{
		DE_CORE_VERIFY(glfwInit(), "Failed to initialize GLFW library.");

		glfwSetErrorCallback(GLFWErrorCallback);

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, m_Params.Resizable ? GLFW_TRUE : GLFW_FALSE);

		m_WindowHandle = glfwCreateWindow(m_Data.Width, m_Data.Height, m_Params.Title.c_str(), nullptr, nullptr);
		DE_CORE_VERIFY(m_WindowHandle, "Failed to create GLFW window.");
		if (!m_WindowHandle)
		{
			glfwTerminate();
		}

		glfwSetWindowUserPointer(m_WindowHandle, &m_Data);

		SetupGLFWCallbacks();
	}

	void Window::Shutdown()
	{
		glfwDestroyWindow(m_WindowHandle);
		glfwTerminate();
	}

	void Window::Update()
	{
		glfwPollEvents();
	}

	bool Window::IsRunning() const
	{
		return glfwWindowShouldClose(m_WindowHandle) == GLFW_FALSE;
	}

	void Window::SetupGLFWCallbacks() const
	{
		glfwSetWindowCloseCallback(m_WindowHandle, [](GLFWwindow* window)
		{
			const WindowData& windowData = *((WindowData*)glfwGetWindowUserPointer(window));

			WindowCloseEvent closeEvent;
			windowData.EventCallback(closeEvent);
		});

		glfwSetWindowSizeCallback(m_WindowHandle, [](GLFWwindow* window, int width, int height)
		{
			const WindowData& windowData = *((WindowData*)glfwGetWindowUserPointer(window));

			WindowResizeEvent resizeEvent(width, height);
			windowData.EventCallback(resizeEvent);
		});
	}

}
