#pragma once
#include "DingoEngine/Core/LayerStack.h"
#include "DingoEngine/Windowing/Window.h"
#include "DingoEngine/Graphics/GraphicsParams.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/SwapChain.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/Renderer2D.h"
#include "DingoEngine/Events/Event.h"
#include "DingoEngine/Events/WindowEvents.h"

#include "DingoEngine/ImGui/ImGuiParams.h"

namespace Dingo
{

	struct ApplicationParams
	{
		WindowParams Window;		// Parameters for the application window
		GraphicsParams Graphics;	// Parameters for the graphics context

		bool EnableImGui = true;	// Whether to enable ImGui support
		ImGuiParams ImGui;			// Parameters for ImGui configuration, only used if EnableImGui is true
	};

	class ImGuiLayer;
	class AppRenderer;

	class Application
	{
	public:
		Application() = delete;
		virtual ~Application();

	public:
		void Initialize();
		void Destroy();

		void OnEvent(Event& e);
		void Run();

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);

		void Close();

		void SubmitPostExecution(const std::function<void()>& callback)
		{
			m_PostExecutionCallbacks.push_back(callback);
		}

		static Application& Get() { return *s_Instance; }
		const Window& GetWindow() const { return *m_Window; }
		const GraphicsContext& GetGraphicsContext() const { return *m_GraphicsContext; }
		Renderer& GetRenderer() const;
		Renderer2D& GetRenderer2D() const { return *m_Renderer2D; }
		SwapChain* GetSwapChain() const { return m_SwapChain; }

	protected:
		Application(const ApplicationParams& params = {});

	protected:
		virtual void OnInitialize() {}
		virtual void OnDestroy() {}

	private:
		bool OnWindowCloseEvent(WindowCloseEvent& e);
		bool OnWindowResizeEvent(WindowResizeEvent& e);

	private:
		ApplicationParams m_Params;
		Window* m_Window = nullptr;
		GraphicsContext* m_GraphicsContext = nullptr;
		SwapChain* m_SwapChain = nullptr;
		AppRenderer* m_Renderer = nullptr;
		Renderer2D* m_Renderer2D = nullptr;
		LayerStack m_LayerStack;
		ImGuiLayer* m_ImGuiLayer = nullptr;
		bool m_IsRunning = true;
		float m_LastFrameTime = 0.0f;
		float m_DeltaTime = 0.0f;

		std::vector<std::function<void()>> m_PostExecutionCallbacks;

	private:
		inline static Application* s_Instance = nullptr;
	};

	Application* CreateApplication(int argc, char** argv);

}
