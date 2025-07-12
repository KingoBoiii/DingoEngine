#pragma once
#include "DingoEngine/Core/LayerStack.h"
#include "DingoEngine/Windowing/Window.h"

#include "DingoEngine/ImGui/ImGuiParams.h"

namespace DingoEngine
{

	struct ApplicationParams
	{
		bool EnableImGui = false;
		ImGuiParams ImGuiParams; // Parameters for ImGui configuration, only used if EnableImGui is true
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

	Application* CreateApplication();

}
