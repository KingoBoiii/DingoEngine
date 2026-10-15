#include "depch.h"
#include "DingoEngine/Graphics/RendererBackend.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Windowing/Window.h"
#include "DingoEngine/Core/Input.h"

#include "DingoEngine/Events/WindowEvents.h"
#include "DingoEngine/Events/KeyEvents.h"
#include "DingoEngine/Events/MouseEvents.h"
#include "DingoEngine/Events/GamepadEvents.h"

#include <GLFW/glfw3.h>

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

	static DisplayMode ToDisplayMode(const GLFWvidmode& mode)
	{
		return DisplayMode{ mode.width, mode.height, mode.refreshRate };
	}

	static std::vector<DisplayMode> ListDisplayModes(GLFWmonitor* monitor)
	{
		int32_t count = 0;
		const GLFWvidmode* modes = glfwGetVideoModes(monitor, &count);
		std::vector<DisplayMode> result;
		result.reserve(count > 0 ? count : 0);
		for (int32_t i = 0; i < count; i++)
			result.push_back(ToDisplayMode(modes[i]));
		return SortDisplayModes(std::move(result));
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
			if (!mode)
				continue;
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

	Window::Window(const WindowParams& params)
		: m_Params(params)
	{
		m_Data.SetSize(m_Params.Width, m_Params.Height);
		m_Data.WindowedWidth = m_Params.Width;
		m_Data.WindowedHeight = m_Params.Height;
	}

	void Window::Initialize()
	{
		DE_CORE_VERIFY(glfwInit(), "Failed to initialize GLFW library.");

		glfwSetErrorCallback(GLFWErrorCallback);

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, m_Params.Resizable ? GLFW_TRUE : GLFW_FALSE);

		m_WindowedWidth = m_Params.Width;
		m_WindowedHeight = m_Params.Height;

		GLFWmonitor* monitor = nullptr;
		if (m_Params.Mode != WindowMode::Windowed)
		{
			monitor = glfwGetPrimaryMonitor();
			if (!monitor)
				DE_CORE_WARN("No monitor to go fullscreen on; starting windowed.");
		}

		const GLFWvidmode* desktopMode = monitor ? glfwGetVideoMode(monitor) : nullptr;
		if (monitor && !desktopMode)
		{
			DE_CORE_WARN("The monitor reports no video mode; starting windowed.");
			monitor = nullptr;
		}

		if (monitor)
		{
			m_DesktopMode = ToDisplayMode(*desktopMode);
			DisplayMode target = m_DesktopMode;
			if (m_Params.Mode == WindowMode::ExclusiveFullscreen)
			{
				target = ChooseDisplayMode(ListDisplayModes(monitor), m_Params.FullscreenMode.IsValid() ? m_Params.FullscreenMode : m_DesktopMode);
				m_FullscreenMode = target;
			}
			m_Mode = m_Params.Mode;
			m_Data.SetSize(target.Width, target.Height);
			glfwWindowHint(GLFW_REFRESH_RATE, target.RefreshRate > 0 ? target.RefreshRate : GLFW_DONT_CARE);

			// No windowed position exists yet; center the restore rect on this monitor.
			int32_t monitorX = 0, monitorY = 0;
			glfwGetMonitorPos(monitor, &monitorX, &monitorY);
			m_WindowedPosX = monitorX + (m_DesktopMode.Width - m_WindowedWidth) / 2;
			m_WindowedPosY = monitorY + (m_DesktopMode.Height - m_WindowedHeight) / 2;
		}

		m_WindowHandle = glfwCreateWindow(m_Data.Width, m_Data.Height, m_Params.Title.c_str(), monitor, nullptr);
		glfwWindowHint(GLFW_REFRESH_RATE, GLFW_DONT_CARE);
		DE_CORE_VERIFY(m_WindowHandle, "Failed to create GLFW window.");
		if (!m_WindowHandle)
		{
			glfwTerminate();
		}

		if (monitor)
		{
			int32_t width = 0, height = 0;
			glfwGetWindowSize(m_WindowHandle, &width, &height);
			m_Data.SetSize(width, height);
		}

		glfwSetWindowUserPointer(m_WindowHandle, &m_Data);

		Input::AttachWindow(m_WindowHandle);

		double cursorX = 0.0, cursorY = 0.0;
		glfwGetCursorPos(m_WindowHandle, &cursorX, &cursorY);
		Input::SeedMousePosition(static_cast<float>(cursorX), static_cast<float>(cursorY));

		SetupGLFWCallbacks();
	}

	void Window::Shutdown()
	{
		Input::AttachWindow(nullptr);

		glfwSetJoystickCallback(nullptr);
		s_JoystickEventCallback = nullptr;

		glfwDestroyWindow(m_WindowHandle);
		glfwTerminate();
	}

	void Window::Update()
	{
		glfwPollEvents();
	}

	void Window::WaitEvents(double timeoutSeconds)
	{
		// GLFW asserts on a negative timeout.
		if (timeoutSeconds > 0.0)
			glfwWaitEventsTimeout(timeoutSeconds);
		else
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

	bool Window::IsFocused() const
	{
		return glfwGetWindowAttrib(m_WindowHandle, GLFW_FOCUSED) != 0;
	}

	void Window::SetVSync(bool vsync)
	{
		if (m_Params.VSync == vsync)
			return;

		m_Params.VSync = vsync;
		if (!m_VSyncForcedOff)
			Internal::RendererBackend::QueueVSync(vsync);
	}

	GLFWmonitor* Window::GetCurrentMonitor() const
	{
		if (GLFWmonitor* monitor = glfwGetWindowMonitor(m_WindowHandle))
			return monitor;
		return FindWindowMonitor(m_WindowHandle);
	}

	void Window::SetWindowMode(WindowMode mode, const DisplayMode& fullscreenMode)
	{
		if (mode == WindowMode::Windowed)
		{
			m_Mode = WindowMode::Windowed;
			m_FullscreenMode = {};
			if (IsFullscreen())
			{
				// Some window managers deliver the restored size only in a later poll.
				m_Data.WindowedWidth = m_WindowedWidth;
				m_Data.WindowedHeight = m_WindowedHeight;
				glfwSetWindowMonitor(m_WindowHandle, nullptr, m_WindowedPosX, m_WindowedPosY, m_WindowedWidth, m_WindowedHeight, 0);
			}
			return;
		}

		GLFWmonitor* monitor = GetCurrentMonitor();
		if (!monitor)
		{
			DE_CORE_WARN("Window::SetWindowMode: no monitor to go fullscreen on.");
			return;
		}

		if (!IsFullscreen())
		{
			glfwGetWindowPos(m_WindowHandle, &m_WindowedPosX, &m_WindowedPosY);
			// The size SetSize last asked for, which a window manager may not have applied yet.
			m_WindowedWidth = m_Data.WindowedWidth;
			m_WindowedHeight = m_Data.WindowedHeight;
		}
		if (m_Mode != WindowMode::ExclusiveFullscreen || !IsFullscreen())
		{
			const GLFWvidmode* videoMode = glfwGetVideoMode(monitor);
			if (!videoMode)
			{
				DE_CORE_WARN("Window::SetWindowMode: the monitor reports no video mode.");
				return;
			}
			m_DesktopMode = ToDisplayMode(*videoMode);
		}

		DisplayMode target = m_DesktopMode;
		if (mode == WindowMode::ExclusiveFullscreen)
			target = ChooseDisplayMode(ListDisplayModes(monitor), fullscreenMode.IsValid() ? fullscreenMode : m_DesktopMode);

		m_Mode = mode;
		m_FullscreenMode = mode == WindowMode::ExclusiveFullscreen ? target : DisplayMode();
		// The size callback fires from glfwSetWindowMonitor, so the resize event
		// (and thus the swap-chain recreation) follows the normal path.
		glfwSetWindowMonitor(m_WindowHandle, monitor, 0, 0, target.Width, target.Height, target.RefreshRate > 0 ? target.RefreshRate : GLFW_DONT_CARE);
	}

	void Window::SetFullscreen(bool fullscreen)
	{
		if (fullscreen == IsFullscreen())
			return;
		SetWindowMode(fullscreen ? WindowMode::BorderlessFullscreen : WindowMode::Windowed);
	}

	std::vector<DisplayMode> Window::GetDisplayModes() const
	{
		GLFWmonitor* monitor = GetCurrentMonitor();
		return monitor ? ListDisplayModes(monitor) : std::vector<DisplayMode>();
	}

	DisplayMode Window::GetDesktopDisplayMode() const
	{
		if (m_Mode == WindowMode::ExclusiveFullscreen && IsFullscreen())
			return m_DesktopMode;
		GLFWmonitor* monitor = GetCurrentMonitor();
		const GLFWvidmode* videoMode = monitor ? glfwGetVideoMode(monitor) : nullptr;
		return videoMode ? ToDisplayMode(*videoMode) : DisplayMode();
	}

	void Window::SetSize(int32_t width, int32_t height)
	{
		if (width <= 0 || height <= 0)
		{
			DE_CORE_WARN("Window::SetSize({}, {}) ignored: both sides must be above 0.", width, height);
			return;
		}

		m_WindowedWidth = width;
		m_WindowedHeight = height;
		if (!IsFullscreen())
		{
			m_Data.WindowedWidth = width;
			m_Data.WindowedHeight = height;
			glfwSetWindowSize(m_WindowHandle, width, height);
		}
	}

	int32_t Window::GetWindowedWidth() const
	{
		return IsFullscreen() ? m_WindowedWidth : m_Data.WindowedWidth;
	}

	int32_t Window::GetWindowedHeight() const
	{
		return IsFullscreen() ? m_WindowedHeight : m_Data.WindowedHeight;
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
			windowData.SetSize(width, height);
			if (width > 0 && height > 0 && !glfwGetWindowMonitor(window))
			{
				windowData.WindowedWidth = width;
				windowData.WindowedHeight = height;
			}

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
					KeyPressedEvent keyPressedEvent(keyCode, false);
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
					KeyPressedEvent keyPressedRepeatEvent(keyCode, true);
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

		glfwSetWindowFocusCallback(m_WindowHandle, [](GLFWwindow* window, int focused)
		{
			const bool isFocused = focused == GLFW_TRUE;
			Input::OnWindowFocusChanged(isFocused);

			const WindowData& windowData = *((WindowData*)glfwGetWindowUserPointer(window));

			WindowFocusEvent focusEvent(isFocused);
			windowData.EventCallback(focusEvent);
		});

		s_JoystickEventCallback = &m_Data.EventCallback;
		glfwSetJoystickCallback([](int jid, int event)
		{
			if (event == GLFW_CONNECTED)
				Input::OnJoystickConnected();
			else if (event == GLFW_DISCONNECTED)
				Input::OnJoystickDisconnected(static_cast<uint32_t>(jid));

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
