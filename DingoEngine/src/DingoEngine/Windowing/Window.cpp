#include "depch.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Windowing/Window.h"

#include <glfw/glfw3.h>

namespace Dingo
{

	Window::Window(const WindowParams& params)
		: m_Params(params), m_Data({ .Width = m_Params.Width, .Height = m_Params.Height })
	{}

	void Window::Initialize()
	{
		DE_CORE_ASSERT(glfwInit(), "Failed to initialize GLFW library.");

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, m_Params.Resizable ? GLFW_TRUE : GLFW_FALSE);

		m_WindowHandle = glfwCreateWindow(m_Data.Width, m_Data.Height, m_Params.Title.c_str(), nullptr, nullptr);
		DE_CORE_ASSERT(m_WindowHandle, "Failed to create GLFW window.");
		if (!m_WindowHandle)
		{
			glfwTerminate();
		}

		m_GraphicsContext = GraphicsContext::Create(m_Params.GraphicsAPI);
		m_GraphicsContext->Initialize();

		const SwapChainParams swapChainOptions = {
			.NativeWindowHandle = m_WindowHandle,
			.Width = m_Data.Width,
			.Height = m_Data.Height
		};
		m_SwapChain = SwapChain::Create(swapChainOptions);
		m_SwapChain->Initialize();

		glfwSetWindowUserPointer(m_WindowHandle, this);

		SetupGLFWCallbacks();
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
		glfwPollEvents();
	}

	bool Window::IsRunning() const
	{
		return glfwWindowShouldClose(m_WindowHandle) == GLFW_FALSE;
	}

	void Window::SetupGLFWCallbacks() const
	{
		glfwSetWindowSizeCallback(m_WindowHandle, [](GLFWwindow* window, int width, int height)
		{
			Window& w = *((Window*)glfwGetWindowUserPointer(window));
			w.m_Data.Width = width;
			w.m_Data.Height = height;

			// Priorite to resizing the swap chain
			w.m_SwapChain->Resize(width, height);
		});
	}

}
