#pragma once
#include "DingoEngine/Windowing/Window.h"

namespace DingoEngine
{

	struct ApplicationParams
	{
	};

	class Application
	{
	public:
		virtual ~Application();

	public:
		void Initialize();
		void Destroy();
		void Run();

		Window& GetWindow() { return *m_Window; }
		static Application& Get() { return *s_Instance; }

	protected:
		Application(const ApplicationParams& params = {});

	protected:
		virtual void OnInitialize() {}
		virtual void OnDestroy() {}

	private:
		Window* m_Window = nullptr;

	private:
		inline static Application* s_Instance = nullptr;
	};

	Application* CreateApplication();

}
