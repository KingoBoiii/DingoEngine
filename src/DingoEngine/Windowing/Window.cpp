#include "depch.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Windowing/Window.h"
#include "DingoEngine/Core/Input.h"

#include "DingoEngine/Events/WindowEvents.h"
#include "DingoEngine/Events/KeyEvents.h"
#include "DingoEngine/Events/MouseEvents.h"
#include "DingoEngine/Events/GamepadEvents.h"

#include <glfw/glfw3.h>

namespace Dingo
{

	static void GLFWErrorCallback(int error, const char* description)
	{
		DE_CORE_ERROR_TAG("GLFW", "GLFW Error ({0}): {1}", error, description);
	}

	// The joystick callback is global (not tied to a window), so the primary
	// window's event sink is stashed here for it. Assumes a single Window whose
	// Shutdown() runs before destruction (a second Window would clobber this,
	// and skipping Shutdown() would leave GLFW a dangling callback).
	static const Window::EventCallbackFn* s_JoystickEventCallback = nullptr;

	Window::Window(const WindowParams& params)
		: m_Params(params), m_Data({ .Width = m_Params.Width, .Height = m_Params.Height })
	{}

	void Window::Initialize()
	{
		DE_CORE_VERIFY(glfwInit(), "Failed to initialize GLFW library.");

		glfwSetErrorCallback(GLFWErrorCallback);

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, m_Params.Resizable ? GLFW_TRUE : GLFW_FALSE);

		m_WindowedWidth = m_Params.Width;
		m_WindowedHeight = m_Params.Height;

		GLFWmonitor* monitor = nullptr;
		if (m_Params.Fullscreen)
		{
			monitor = glfwGetPrimaryMonitor();
			const GLFWvidmode* videoMode = glfwGetVideoMode(monitor);

			// Matching the desktop video mode gives borderless ("windowed") fullscreen.
			m_Data.Width = videoMode->width;
			m_Data.Height = videoMode->height;

			// No windowed position exists yet; center the restore rect on this monitor.
			int32_t monitorX = 0, monitorY = 0;
			glfwGetMonitorPos(monitor, &monitorX, &monitorY);
			m_WindowedPosX = monitorX + (videoMode->width - m_WindowedWidth) / 2;
			m_WindowedPosY = monitorY + (videoMode->height - m_WindowedHeight) / 2;
		}

		m_WindowHandle = glfwCreateWindow(m_Data.Width, m_Data.Height, m_Params.Title.c_str(), monitor, nullptr);
		DE_CORE_VERIFY(m_WindowHandle, "Failed to create GLFW window.");
		if (!m_WindowHandle)
		{
			glfwTerminate();
		}

		glfwSetWindowUserPointer(m_WindowHandle, &m_Data);

		double cursorX = 0.0, cursorY = 0.0;
		glfwGetCursorPos(m_WindowHandle, &cursorX, &cursorY);
		Input::SeedMousePosition(static_cast<float>(cursorX), static_cast<float>(cursorY));

		SetupGLFWCallbacks();
	}

	void Window::Shutdown()
	{
		glfwSetJoystickCallback(nullptr);
		s_JoystickEventCallback = nullptr;

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

	bool Window::IsFullscreen() const
	{
		return glfwGetWindowMonitor(m_WindowHandle) != nullptr;
	}

	// The monitor the window overlaps most, falling back to the primary monitor.
	// glfwGetWindowMonitor only reports a monitor for fullscreen windows, so a windowed
	// window about to GO fullscreen needs this overlap test to pick the right display.
	static GLFWmonitor* FindWindowMonitor(GLFWwindow* window)
	{
		int32_t windowX = 0, windowY = 0, windowWidth = 0, windowHeight = 0;
		glfwGetWindowPos(window, &windowX, &windowY);
		glfwGetWindowSize(window, &windowWidth, &windowHeight);

		int32_t monitorCount = 0;
		GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

		GLFWmonitor* best = glfwGetPrimaryMonitor();
		int32_t bestOverlap = 0;

		for (int32_t i = 0; i < monitorCount; i++)
		{
			const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
			int32_t monitorX = 0, monitorY = 0;
			glfwGetMonitorPos(monitors[i], &monitorX, &monitorY);

			// (std::min) parentheses dodge the Windows.h min/max macros (engine TUs lack NOMINMAX).
			const int32_t overlapWidth = (std::min)(windowX + windowWidth, monitorX + mode->width) - (std::max)(windowX, monitorX);
			const int32_t overlapHeight = (std::min)(windowY + windowHeight, monitorY + mode->height) - (std::max)(windowY, monitorY);
			const int32_t overlap = (std::max)(overlapWidth, 0) * (std::max)(overlapHeight, 0);

			if (overlap > bestOverlap)
			{
				bestOverlap = overlap;
				best = monitors[i];
			}
		}

		return best;
	}

	void Window::SetFullscreen(bool fullscreen)
	{
		if (fullscreen == IsFullscreen())
			return;

		if (fullscreen)
		{
			glfwGetWindowPos(m_WindowHandle, &m_WindowedPosX, &m_WindowedPosY);
			glfwGetWindowSize(m_WindowHandle, &m_WindowedWidth, &m_WindowedHeight);

			GLFWmonitor* monitor = FindWindowMonitor(m_WindowHandle);
			const GLFWvidmode* videoMode = glfwGetVideoMode(monitor);
			glfwSetWindowMonitor(m_WindowHandle, monitor, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate);
		}
		else
		{
			glfwSetWindowMonitor(m_WindowHandle, nullptr, m_WindowedPosX, m_WindowedPosY, m_WindowedWidth, m_WindowedHeight, 0);
		}
		// The size callback fires from glfwSetWindowMonitor, so the resize event
		// (and thus the swap-chain recreation) follows the normal path.
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
			WindowData& windowData = *((WindowData*)glfwGetWindowUserPointer(window));

			// Keep GetWidth()/GetHeight()/GetAspectRatio() truthful after resizes.
			windowData.Width = width;
			windowData.Height = height;

			WindowResizeEvent resizeEvent(width, height);
			windowData.EventCallback(resizeEvent);
		});

		glfwSetKeyCallback(m_WindowHandle, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			if (key < 0 || key > GLFW_KEY_LAST) // GLFW_KEY_UNKNOWN
				return;

			KeyCode keyCode = static_cast<KeyCode>(key);

			Input::UpdateKeyState(keyCode, action != GLFW_RELEASE);

			const WindowData& windowData = *((WindowData*)glfwGetWindowUserPointer(window));

			switch (action)
			{
				case GLFW_PRESS:
				{
					KeyPressedEvent keyPressedEvent(keyCode, 0);
					windowData.EventCallback(keyPressedEvent);
					break;
				}
				case GLFW_RELEASE:
				{
					KeyReleasedEvent keyReleasedEvent(keyCode);
					windowData.EventCallback(keyReleasedEvent);
					break;
				}
				case GLFW_REPEAT:
				{
					KeyPressedEvent keyPressedRepeatEvent(keyCode, 1);
					windowData.EventCallback(keyPressedRepeatEvent);
					break;
				}
			}
		});

		glfwSetMouseButtonCallback(m_WindowHandle, [](GLFWwindow* window, int button, int action, int mods)
		{
			MouseButton mouseButton = static_cast<MouseButton>(button);

			Input::UpdateMouseButtonState(mouseButton, action == GLFW_PRESS);

			const WindowData& windowData = *((WindowData*)glfwGetWindowUserPointer(window));

			switch (action)
			{
				case GLFW_PRESS:
				{
					MouseButtonPressedEvent mouseButtonPressedEvent(mouseButton);
					windowData.EventCallback(mouseButtonPressedEvent);
					break;
				}
				case GLFW_RELEASE:
				{
					MouseButtonReleasedEvent mouseButtonReleasedEvent(mouseButton);
					windowData.EventCallback(mouseButtonReleasedEvent);
					break;
				}
			}
		});

		glfwSetCursorPosCallback(m_WindowHandle, [](GLFWwindow* window, double x, double y)
		{
			Input::UpdateMousePosition(static_cast<float>(x), static_cast<float>(y));

			const WindowData& windowData = *((WindowData*)glfwGetWindowUserPointer(window));

			MouseMovedEvent mouseMovedEvent(static_cast<float>(x), static_cast<float>(y));
			windowData.EventCallback(mouseMovedEvent);
		});

		glfwSetScrollCallback(m_WindowHandle, [](GLFWwindow* window, double xOffset, double yOffset)
		{
			Input::AccumulateMouseScroll(static_cast<float>(xOffset), static_cast<float>(yOffset));

			const WindowData& windowData = *((WindowData*)glfwGetWindowUserPointer(window));

			MouseScrolledEvent mouseScrolledEvent(static_cast<float>(xOffset), static_cast<float>(yOffset));
			windowData.EventCallback(mouseScrolledEvent);
		});

		s_JoystickEventCallback = &m_Data.EventCallback;
		glfwSetJoystickCallback([](int jid, int event)
		{
			if (!s_JoystickEventCallback || !(*s_JoystickEventCallback))
				return;

			if (event == GLFW_CONNECTED && glfwJoystickIsGamepad(jid))
			{
				std::string name;
				const GamepadType type = Input::RegisterGamepadConnection(static_cast<uint32_t>(jid), name);

				GamepadConnectedEvent connectedEvent(static_cast<uint32_t>(jid), type, name);
				(*s_JoystickEventCallback)(connectedEvent);
			}
			else if (event == GLFW_DISCONNECTED && Input::IsGamepadConnected(static_cast<uint32_t>(jid)))
			{
				std::string name;
				const GamepadType type = Input::UnregisterGamepadConnection(static_cast<uint32_t>(jid), name);

				GamepadDisconnectedEvent disconnectedEvent(static_cast<uint32_t>(jid), type, name);
				(*s_JoystickEventCallback)(disconnectedEvent);
			}
		});
	}

}
