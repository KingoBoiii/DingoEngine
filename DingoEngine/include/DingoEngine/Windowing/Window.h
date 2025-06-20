#pragma once

namespace DingoEngine
{

	struct WindowOptions
	{
	};

	class Window
	{
	public:
		Window(const WindowOptions& options = {});
		~Window() = default;
	public:
		void Initialize();
		void Shutdown();
	private:
		WindowOptions m_Options;
	};

}
