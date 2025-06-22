#include "depch.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Windowing/Window.h"

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

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		m_WindowHandle = glfwCreateWindow(m_Options.Width, m_Options.Height, m_Options.Title.c_str(), nullptr, nullptr);
		if (!m_WindowHandle)
		{
			glfwTerminate();
			throw new std::exception("Failed to create GLFW window.");
		}

		m_GraphicsContext = GraphicsContext::Create(GraphicsAPI::Vulkan, m_WindowHandle);
		m_GraphicsContext->Initialize();

		//const SwapChainOptions swapChainOptions = {
		//	.NativeWindowHandle = m_WindowHandle,
		//	.Width = m_Options.Width,
		//	.Height = m_Options.Height
		//};
		//m_SwapChain = SwapChain::Create(swapChainOptions);
		//m_SwapChain->Initialize();

		//glfwMakeContextCurrent(m_WindowHandle);
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
