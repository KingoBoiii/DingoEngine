#pragma once
#include "DingoEngine/Common.h"
#include "DingoEngine/Version.h"
#include "DingoEngine/BuildInfo.h"
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

#include <optional>
#include <string_view>

namespace Dingo
{

	struct ApplicationCommandLineArgs
	{
		int Count = 0;
		char** Args = nullptr;

		const char* operator[](int index) const
		{
			DE_CORE_ASSERT(index < Count, "Command line argument index out of bounds");
			return Args[index];
		}

		// Finds --name (flag) or --name=value. Returns nullopt if not present,
		// empty string_view for bare flags, or the value string for key=value pairs.
		std::optional<std::string_view> Get(std::string_view name) const
		{
			for (int i = 1; i < Count; ++i)
			{
				std::string_view arg = Args[i];
				if (!arg.starts_with("--"))
					continue;

				std::string_view key = arg.substr(2);
				auto eqPos = key.find('=');
				if (eqPos != std::string_view::npos)
				{
					if (key.substr(0, eqPos) == name)
						return key.substr(eqPos + 1);
				}
				else if (key == name)
				{
					return std::string_view{};
				}
			}
			return std::nullopt;
		}
	};

	struct ApplicationParams
	{
		ApplicationCommandLineArgs CommandLineArgs;	// Parsed command line arguments

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
		void RequestRestart(GraphicsAPI api);

		void SubmitPostExecution(const std::function<void()>& callback)
		{
			m_PostExecutionCallbacks.push_back(callback);
		}

		static Application& Get() { return *s_Instance; }
		static bool HasPendingRestart() { return s_PendingRestart; }
		static GraphicsAPI ConsumePendingRestart();

		const Window& GetWindow() const { return *m_Window; }
		const GraphicsContext& GetGraphicsContext() const { return *m_GraphicsContext; }
		const ApplicationCommandLineArgs& GetCommandLineArgs() const { return m_Params.CommandLineArgs; }
		Renderer& GetRenderer() const;
		Renderer2D& GetRenderer2D() const { return *m_Renderer2D; }
		SwapChain* GetSwapChain() const { return m_SwapChain; }

		const uint32_t GetEngineVersion() const { return DE_MAKE_VERSION(DE_ENGINE_VERSION_MAJOR, DE_ENGINE_VERSION_MINOR, DE_ENGINE_VERSION_PATCH); }
		const uint32_t GetEngineBuildNumber() const { return DE_ENGINE_VERSION_BUILD; }

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
		inline static bool s_PendingRestart = false;
		inline static GraphicsAPI s_PendingRestartAPI = GraphicsAPI::Vulkan;
	};

	Application* CreateApplication(ApplicationCommandLineArgs args);

}
