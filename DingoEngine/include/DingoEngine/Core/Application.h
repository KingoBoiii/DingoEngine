#pragma once
#include "DingoEngine/Core/LayerStack.h"
#include "DingoEngine/Windowing/Window.h"

#include "DingoEngine/ImGui/ImGuiParams.h"

namespace DingoEngine
{

	struct ApplicationParams
	{
		WindowParams Window; // Parameters for the application window

		bool EnableImGui = true; // Whether to enable ImGui support
		ImGuiParams ImGui; // Parameters for ImGui configuration, only used if EnableImGui is true
	};

	class ImGuiLayer;

	class Application
	{
	public:
		virtual ~Application();

	public:
		void Initialize();
		void Destroy();
		void Run();

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);

		Window& GetWindow() { return *m_Window; }
		static Application& Get() { return *s_Instance; }

	protected:
		Application(const ApplicationParams& params = {});

	protected:
		virtual void OnInitialize() {}
		virtual void OnDestroy() {}

	private:
		ApplicationParams m_Params;
		LayerStack m_LayerStack;
		Window* m_Window = nullptr;
		ImGuiLayer* m_ImGuiLayer = nullptr;

	private:
		inline static Application* s_Instance = nullptr;
	};

	Application* CreateApplication(int argc, char** argv);

}
