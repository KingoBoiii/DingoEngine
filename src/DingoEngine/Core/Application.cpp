#include "depch.h"
#include "DingoEngine/Core/Application.h"
#include "DingoEngine/Core/Timer.h"
#include "DingoEngine/Core/Layer.h"
#include "DingoEngine/Core/CacheManager.h"
#include "DingoEngine/Core/Layers/EmptyLayer.h"
#include "DingoEngine/Core/Input.h"
#include "DingoEngine/Core/KeyCodes.h"
#include "DingoEngine/UI/DebugPanels.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Audio/AudioEngine.h"

#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/ImGui/ImGuiLayer.h"
#include <DingoEngine/Graphics/NVRHI/NvrhiGraphicsContext.h>

namespace Dingo
{
	Application::Application(const ApplicationParams& params)
		: m_Params(params)
	{
		s_Instance = this;
	}

	Application::~Application()
	{
		s_Instance = nullptr;

		Destroy();
	}

	void Application::Initialize()
	{
		DE_CORE_ASSERT(s_Instance, "Application already initialized. Cannot initialize again.");

		CacheManager::Initialize();

		m_Window = new Window(m_Params.Window);
		m_Window->Initialize();
		m_Window->SetEventCallback(DE_BIND_EVENT_FN(Application::OnEvent));

		GraphicsParams graphicsParams = m_Params.Graphics;
		graphicsParams.NativeWindowHandle = m_Window->GetNativeWindowHandle();
		m_GraphicsContext = GraphicsContext::Create(graphicsParams);
		m_GraphicsContext->Initialize();

		m_SwapChain = SwapChain::Create(SwapChainParams()
			.SetNativeWindowHandle(m_Window->GetNativeWindowHandle())
			.SetWidth(m_Window->GetWidth())
			.SetHeight(m_Window->GetHeight())
			.SetVSync(m_Params.Window.VSync));
		m_SwapChain->Initialize();

		Renderer::Initialize(m_SwapChain);

		m_Renderer2D = Renderer2D::Create(m_Params.Renderer2D);
		m_Renderer3D = Renderer3D::Create(m_Params.Renderer3D);
		m_SceneRenderer = new SceneRenderer(*m_Renderer2D, *m_Renderer3D);

		// Audio is independent of the graphics/window stack (miniaudio owns its own
		// device thread); bring it up here so it's live before OnInitialize().
		m_AudioEngine = AudioEngine::Create();
		m_AudioEngine->Initialize();

		OnInitialize();

		if (m_LayerStack.Empty())
		{
			// If no layers are pushed, we can push a default layer
			DE_CORE_ERROR("No layers pushed to the application. Pushing an empty layer.");
			PushLayer(new EmptyLayer());

			m_Params.EnableUI = false; // Disable the UI layer if no layers are pushed
			return;
		}

		// Bring up the ImGui backend if the game wants UI, or if the engine's built-in
		// debug overlays are enabled -- so the renderer-stats window works even in
		// projects that use no UI of their own, in every build config (Distribution too).
		bool enableImGui = m_Params.EnableUI || m_Params.EnableDebugOverlays;
		if (enableImGui)
		{
			m_ImGuiLayer = new ImGuiLayer(m_Params.UI);
			PushOverlay(m_ImGuiLayer);
		}

		if (m_ImGuiLayer && m_Params.EnableDebugOverlays)
			DE_CORE_INFO("Debug overlays enabled - press F3 for engine stats, F4 for renderer stats.");
	}

	void Application::Destroy()
	{
		OnDestroy();

		// Stop the render thread first so the GPU is idle before any resources are freed.
		Renderer::Shutdown();

		m_LayerStack.Clear();

		// Tear down audio after the layers (so their destructors can still stop sounds)
		// but independently of the graphics stack.
		if (m_AudioEngine)
		{
			m_AudioEngine->Shutdown();
			delete m_AudioEngine;
			m_AudioEngine = nullptr;
		}

		// Delete the SceneRenderer before the renderers it references.
		if (m_SceneRenderer)
		{
			delete m_SceneRenderer;
			m_SceneRenderer = nullptr;
		}

		if (m_Renderer3D)
		{
			m_Renderer3D->Shutdown();
			delete m_Renderer3D;
			m_Renderer3D = nullptr;
		}

		if (m_Renderer2D)
		{
			m_Renderer2D->Shutdown();
			delete m_Renderer2D;
			m_Renderer2D = nullptr;
		}

		if (m_Window)
		{
			m_Window->Shutdown();
			delete m_Window;
			m_Window = nullptr;
		}

		if (m_SwapChain)
		{
			m_SwapChain->Destroy();
			delete m_SwapChain;
			m_SwapChain = nullptr;
		}

		if (m_GraphicsContext)
		{
			m_GraphicsContext->Shutdown();
			delete m_GraphicsContext;
			m_GraphicsContext = nullptr;
		}

		CacheManager::Shutdown();
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);

		dispatcher.Dispatch<WindowCloseEvent>(DE_BIND_EVENT_FN(Application::OnWindowCloseEvent));
		dispatcher.Dispatch<WindowResizeEvent>(DE_BIND_EVENT_FN(Application::OnWindowResizeEvent));

		for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
		{
			if (e.Handled)
				break;
			(*it)->OnEvent(e);
		}
	}

	void Application::Run()
	{
		Timer timer;

		while (m_IsRunning)
		{
			float time = timer.Elapsed();
			m_DeltaTime = time - m_LastFrameTime;
			m_LastFrameTime = time;

			Input::Update();
			m_Window->Update();

			if (m_AudioEngine)
				m_AudioEngine->Update(); // reap finished one-shots

			Renderer::BeginFrame();

			for (Layer* layer : m_LayerStack)
			{
				layer->OnUpdate(m_DeltaTime);
			}

			if (m_ImGuiLayer)
			{
				m_ImGuiLayer->Begin();

				if (m_Params.EnableUI)
				{
					for (Layer* layer : m_LayerStack)
					{
						layer->OnUIRender();
					}
				}

				if (m_Params.EnableDebugOverlays)
					RenderDebugOverlays();

				m_ImGuiLayer->End();
			}

			Renderer::EndFrame();

			// Execute any post-execution callbacks
			for (const auto& callback : m_PostExecutionCallbacks)
			{
				callback();
			}
			m_PostExecutionCallbacks.clear();
		}
	}

	void Application::RenderDebugOverlays()
	{
		// F3 toggles the built-in engine-stats window, F4 the renderer-stats window.
		// Input::IsKeyDown is edge-triggered ("just pressed"), so each flips once per
		// press, not every frame it's held.
		if (Input::IsKeyDown(Key::F3))
			m_ShowEngineStats = !m_ShowEngineStats;
		if (Input::IsKeyDown(Key::F4))
			m_ShowRendererStats = !m_ShowRendererStats;

		if (m_ShowEngineStats)
			UI::EngineStatsWindow(&m_ShowEngineStats);
		if (m_ShowRendererStats)
			UI::RendererStatsWindow(&m_ShowRendererStats);
	}

	void Application::PushLayer(Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* overlay)
	{
		m_LayerStack.PushOverlay(overlay);
		overlay->OnAttach();
	}

	void Application::Close()
	{
		m_IsRunning = false;
	}

	void Application::RequestRestart(GraphicsAPI api)
	{
		s_PendingRestart = true;
		s_PendingRestartAPI = api;
		SubmitPostExecution([]() { Application::Get().Close(); });
	}

	GraphicsAPI Application::ConsumePendingRestart()
	{
		s_PendingRestart = false;
		return s_PendingRestartAPI;
	}

	bool Application::OnWindowCloseEvent(WindowCloseEvent& e)
	{
		m_IsRunning = false; // Stop the application loop
		return true;
	}

	bool Application::OnWindowResizeEvent(WindowResizeEvent& e)
	{
		m_SwapChain->Resize(e.GetWidth(), e.GetHeight());
		return true;
	}

}
