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
		DE_CORE_ASSERT(glfwInit(), "Failed to initialize GLFW library.");

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		m_WindowHandle = glfwCreateWindow(m_Options.Width, m_Options.Height, m_Options.Title.c_str(), nullptr, nullptr);
		DE_CORE_ASSERT(m_WindowHandle, "Failed to create GLFW window.");
		if (!m_WindowHandle)
		{
			glfwTerminate();
		}

		m_GraphicsContext = GraphicsContext::Create(m_Options.GraphicsAPI);
		m_GraphicsContext->Initialize();

		const SwapChainOptions swapChainOptions = {
			.NativeWindowHandle = m_WindowHandle,
			.Width = m_Options.Width,
			.Height = m_Options.Height
		};
		m_SwapChain = SwapChain::Create(swapChainOptions);
		m_SwapChain->Initialize();

		//glfwMakeContextCurrent(m_WindowHandle);
	}

	void Window::Shutdown()
	{
		m_SwapChain->Destroy();

		m_GraphicsContext->Shutdown();

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
