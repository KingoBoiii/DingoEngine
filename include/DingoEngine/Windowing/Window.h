#pragma once
#include "DingoEngine/Common.h"
#include "DingoEngine/Events/Event.h"
#include "DingoEngine/Windowing/DisplayMode.h"

#include <functional>

struct GLFWwindow;
struct GLFWmonitor;

namespace Dingo
{

	struct WindowParams
	{
		std::string Title = "DingoEngine Application";
		int32_t Width = 1600;
		int32_t Height = 900;
		bool VSync = true;
		bool Resizable = true;
		// The mode to start in. Width/Height stay the windowed size to restore to.
		WindowMode Mode = WindowMode::Windowed;
		// ExclusiveFullscreen's display mode (the nearest listed one); invalid = the desktop's.
		DisplayMode FullscreenMode;

		WindowParams& SetTitle(const std::string& title)
		{
			Title = title;
			return *this;
		}

		WindowParams& SetWidth(int32_t width)
		{
			Width = width;
			return *this;
		}

		WindowParams& SetHeight(int32_t height)
		{
			Height = height;
			return *this;
		}

		WindowParams& SetVSync(bool vsync)
		{
			VSync = vsync;
			return *this;
		}

		WindowParams& SetResizable(bool resizable)
		{
			Resizable = resizable;
			return *this;
		}

		// Borderless fullscreen or windowed.
		WindowParams& SetFullscreen(bool fullscreen)
		{
			Mode = fullscreen ? WindowMode::BorderlessFullscreen : WindowMode::Windowed;
			return *this;
		}

		WindowParams& SetMode(WindowMode mode)
		{
			Mode = mode;
			return *this;
		}

		WindowParams& SetFullscreenMode(const DisplayMode& mode)
		{
			FullscreenMode = mode;
			return *this;
		}
	};

	class GraphicsContext;

	class Window
	{
	public:
		using EventCallbackFn = std::function<void(Event&)>;

	public:
		~Window() = default;
		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		bool IsRunning() const;

		// On the monitor the window is on. Both fullscreen modes are a fullscreen window that is
		// minimized when it loses focus; exclusive also switches the display to fullscreenMode's
		// nearest listed mode (invalid = the desktop's). The swap chain follows through the
		// ordinary resize event; safe from event handlers.
		void SetWindowMode(WindowMode mode, const DisplayMode& fullscreenMode = {});
		WindowMode GetWindowMode() const { return m_Mode; }
		// The display mode exclusive fullscreen runs at; invalid in the other modes.
		DisplayMode GetFullscreenDisplayMode() const { return m_Mode == WindowMode::ExclusiveFullscreen ? m_FullscreenMode : DisplayMode(); }

		// Borderless fullscreen, or windowed. Already fullscreen in either mode, true changes nothing.
		void SetFullscreen(bool fullscreen);
		void ToggleFullscreen() { SetFullscreen(!IsFullscreen()); }
		bool IsFullscreen() const;

		// The display modes of the monitor the window is on, for an options screen: no duplicates,
		// largest first.
		std::vector<DisplayMode> GetDisplayModes() const;
		// That monitor's mode outside exclusive fullscreen.
		DisplayMode GetDesktopDisplayMode() const;

		// The windowed client size. While fullscreen it is kept as the size to restore to.
		void SetSize(int32_t width, int32_t height);
		// The windowed size: the current size while windowed, the one to restore to while fullscreen.
		int32_t GetWindowedWidth() const;
		int32_t GetWindowedHeight() const;

		// Safe from event handlers; the swap chain takes it after the next present (Vulkan
		// recreates it, as a resize does). A restart goes back to WindowParams::VSync.
		void SetVSync(bool vsync);
		bool IsVSync() const { return m_Params.VSync; }

		bool IsFocused() const;

		int32_t GetWidth() const { return m_Data.Width; }
		int32_t GetHeight() const { return m_Data.Height; }
		// The last size with both sides above 0, so a minimized window (0 x 0) keeps the aspect it had.
		float GetAspectRatio() const { return m_Data.AspectRatio; }

	private:
		// The Application owns the one window (GetWindow) and drives it.
		Window(const WindowParams& params = {});

		void Initialize();
		void Shutdown();

		void Update();
		void WaitEvents(double timeoutSeconds);

		void SetEventCallback(const EventCallbackFn& callback) { m_Data.EventCallback = callback; }
		GLFWwindow* GetNativeWindowHandle() const { return m_WindowHandle; }

		friend class Application;

		void SetupGLFWCallbacks() const;
		GLFWmonitor* GetCurrentMonitor() const;

	private:
		WindowParams m_Params;
		GLFWwindow* m_WindowHandle = nullptr;

		// Windowed geometry to restore when leaving fullscreen.
		int32_t m_WindowedPosX = 0;
		int32_t m_WindowedPosY = 0;
		int32_t m_WindowedWidth = 0;
		int32_t m_WindowedHeight = 0;

		WindowMode m_Mode = WindowMode::Windowed;
		DisplayMode m_FullscreenMode;
		// The monitor's mode before exclusive fullscreen changed it.
		DisplayMode m_DesktopMode;

		struct WindowData
		{
			int32_t Width;
			int32_t Height;
			float AspectRatio = 1.0f;
			// The last size while windowed and not minimized.
			int32_t WindowedWidth = 0;
			int32_t WindowedHeight = 0;
			EventCallbackFn EventCallback;

			void SetSize(int32_t width, int32_t height)
			{
				Width = width;
				Height = height;
				if (width > 0 && height > 0)
					AspectRatio = static_cast<float>(width) / static_cast<float>(height);
			}
		} m_Data;

		friend class ImGuiLayer;
	};

}
