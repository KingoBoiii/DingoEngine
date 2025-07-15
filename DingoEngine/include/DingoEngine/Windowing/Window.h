#pragma once
#include "DingoEngine/Common.h"

struct GLFWwindow;

namespace Dingo
{

	struct WindowParams
	{
		std::string Title = "DingoEngine Application";
		int32_t Width = 1600;
		int32_t Height = 900;
		GraphicsAPI GraphicsAPI = GraphicsAPI::Vulkan;
		bool VSync = true;
		bool Resizable = true;

		WindowParams SetTitle(const std::string& title)
		{
			Title = title;
			return *this;
		}

		WindowParams SetWidth(int32_t width)
		{
			Width = width;
			return *this;
		}

		WindowParams SetHeight(int32_t height)
		{
			Height = height;
			return *this;
		}

		WindowParams SetGraphicsAPI(Dingo::GraphicsAPI graphicsAPI)
		{
			GraphicsAPI = graphicsAPI;
			return *this;
		}

		WindowParams SetVSync(bool vsync)
		{
			VSync = vsync;
			return *this;
		}

		WindowParams SetResizable(bool resizable)
		{
			Resizable = resizable;
			return *this;
		}
	};

	class GraphicsContext;

	class Window
	{
	public:
		Window(const WindowParams& params = {});
		~Window() = default;
	public:
		void Initialize();
		void Shutdown();

		void Update();

		bool IsRunning() const;

		int32_t GetWidth() const { return m_Data.Width; }
		int32_t GetHeight() const { return m_Data.Height; }
		float GetAspectRatio() const
		{
			return static_cast<float>(m_Data.Width) / static_cast<float>(m_Data.Height); 
		}
		GLFWwindow* GetNativeWindowHandle() const { return m_WindowHandle; }

	private:
		void SetupGLFWCallbacks() const;

	private:
		WindowParams m_Params;
		GLFWwindow* m_WindowHandle = nullptr;

		struct WindowData
		{
			int32_t Width;
			int32_t Height;
		} m_Data;

		friend class ImGuiLayer;
	};

}
