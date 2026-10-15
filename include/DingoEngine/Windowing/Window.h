#pragma once
#include "DingoEngine/Common.h"
#include "DingoEngine/Events/Event.h"
#include "DingoEngine/Windowing/DisplayMode.h"

#include <functional>
#include <optional>

struct GLFWwindow;
struct GLFWmonitor;

namespace Dingo
{

	/// How the application window is created; the members of ApplicationParams::Window.
	struct WindowParams
	{
		std::string Title = "DingoEngine Application";	///< Title bar text.
		int32_t Width = 1600;		///< Client width in pixels (the windowed size).
		int32_t Height = 900;		///< Client height in pixels (the windowed size).
		bool VSync = true;			///< Start value; switch at runtime with Window::SetVSync.
		bool Resizable = true;		///< Whether the user can resize the window.
		/// The mode to start in. Width/Height stay the windowed size to restore to.
		WindowMode Mode = WindowMode::Windowed;
		/// ExclusiveFullscreen's display mode (the nearest listed one); invalid = the desktop's.
		DisplayMode FullscreenMode;

		/// Each setter sets the field of the same name and returns *this, for chaining.
		///@{
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
		///@}

		/// Borderless fullscreen or windowed.
		WindowParams& SetFullscreen(bool fullscreen)
		{
			Mode = fullscreen ? WindowMode::BorderlessFullscreen : WindowMode::Windowed;
			return *this;
		}

		/// Each setter sets the field of the same name and returns *this, for chaining.
		///@{
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
		///@}
	};

	class GraphicsContext;

	/// The application's window. The Application creates, drives and destroys the one window; get
	/// it from Application::GetWindow(). Use it from the main thread.
	class Window
	{
	public:
		/// Receives every window and input event; the Application installs Application::OnEvent.
		using EventCallbackFn = std::function<void(Event&)>;

	public:
		/// The Application deletes the window at teardown; never delete it yourself.
		~Window() = default;
		/// Not copyable.
		Window(const Window&) = delete;
		/// Not copyable.
		Window& operator=(const Window&) = delete;

		/// False once the window has been asked to close (the close button, Alt+F4, the OS).
		bool IsRunning() const;

		/// Switches between windowed, borderless and exclusive fullscreen.
		/// On the monitor the window is on. Both fullscreen modes are a fullscreen window that is
		/// minimized when it loses focus; exclusive also switches the display to fullscreenMode's
		/// nearest listed mode (invalid = the desktop's). The swap chain follows through the
		/// ordinary resize event; safe from event handlers. fullscreenMode matters only for
		/// ExclusiveFullscreen.
		void SetWindowMode(WindowMode mode, const DisplayMode& fullscreenMode = {});
		/// The current mode; it changes as soon as SetWindowMode is called.
		WindowMode GetWindowMode() const { return m_Mode; }
		/// The display mode exclusive fullscreen runs at; invalid in the other modes.
		DisplayMode GetFullscreenDisplayMode() const { return m_Mode == WindowMode::ExclusiveFullscreen ? m_FullscreenMode : DisplayMode(); }

		/// Borderless fullscreen, or windowed. Already fullscreen in either mode, true changes
		/// nothing.
		void SetFullscreen(bool fullscreen);
		/// Windowed becomes borderless fullscreen; either fullscreen mode becomes windowed.
		void ToggleFullscreen() { SetFullscreen(!IsFullscreen()); }
		/// True in either fullscreen mode.
		bool IsFullscreen() const;

		/// The display modes of the monitor the window is on, for an options screen: no duplicates,
		/// largest first.
		std::vector<DisplayMode> GetDisplayModes() const;
		/// That monitor's mode outside exclusive fullscreen.
		DisplayMode GetDesktopDisplayMode() const;

		/// The windowed client size, in pixels. While fullscreen it is kept as the size to restore
		/// to. Ignored, with a warning, unless both sides are above 0. Works under `--window`
		/// too; only Settings::Apply leaves the window alone then.
		void SetSize(int32_t width, int32_t height);
		/// The windowed width in pixels: the current width while windowed, the one to restore to
		/// while fullscreen.
		int32_t GetWindowedWidth() const;
		/// The windowed height in pixels: the current height while windowed, the one to restore to
		/// while fullscreen.
		int32_t GetWindowedHeight() const;

		/// Safe from event handlers; the swap chain takes it after the next present (Vulkan
		/// recreates it, as a resize does). A restart goes back to WindowParams::VSync. Started
		/// with `--no-vsync`, the swap chain stays off for the whole run; the request is kept.
		void SetVSync(bool vsync);
		/// The requested value; it changes at once, before the swap chain takes it. `--no-vsync`
		/// doesn't change it, so a settings screen saves the player's choice.
		bool IsVSync() const { return m_Params.VSync; }

		/// True while the window has input focus.
		bool IsFocused() const;

		/// Current client width in pixels; 0 while the window is minimized.
		int32_t GetWidth() const { return m_Data.Width; }
		/// Current client height in pixels; 0 while the window is minimized.
		int32_t GetHeight() const { return m_Data.Height; }
		/// The last size with both sides above 0, so a minimized window (0 x 0) keeps the aspect it
		/// had.
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
		friend struct Settings;

		void SetupGLFWCallbacks() const;
		GLFWmonitor* GetCurrentMonitor() const;

	private:
		WindowParams m_Params;
		GLFWwindow* m_WindowHandle = nullptr;
		bool m_VSyncForcedOff = false;
		// While --window holds the window: the mode and size the game asked for, which Settings
		// reads and writes instead of the window's.
		std::optional<WindowParams> m_DisplayRequest;

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
