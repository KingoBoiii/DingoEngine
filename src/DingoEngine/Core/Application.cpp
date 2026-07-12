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
		// Destroy() tears down the layers, whose scenes reach back through
		// Application::Get() (e.g. to stop their sounds) — the instance must stay
		// valid until teardown has finished.
		Destroy();

		s_Instance = nullptr;
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

		// After audio (clips load through it), before OnInitialize() so layers can load
		// assets from OnAttach.
		m_AssetManager = new AssetManager(m_Params.Assets, m_AudioEngine);
		m_AssetManager->Initialize();

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
			DE_CORE_INFO("Debug window enabled - press F3 (engine), F4 (renderer) or F5 (input) to open its tabs.");
	}

	void Application::Destroy()
	{
		OnDestroy();

		// Stop the render thread first so the GPU is idle before any resources are freed.
		Renderer::Shutdown();

		m_LayerStack.Clear();

		// Free managed assets after the layers that borrow them, but before the audio
		// engine: AudioClips must not outlive the engine that decoded them.
		if (m_AssetManager)
		{
			m_AssetManager->Shutdown();
			delete m_AssetManager;
			m_AssetManager = nullptr;
		}

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

			// After BeginFrame: the render thread is parked until EndFrame, so the GPU
			// work in here (texture uploads, shader recompiles) can't race its
			// garbage-collection/present pass on the NVRHI device.
			if (m_AssetManager)
				m_AssetManager->Update(m_DeltaTime); // finalize async loads, poll hot-reload

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
		// One tabbed debug window: F3 = Engine, F4 = Renderer, F5 = Input. A key opens
		// the window on its tab (or switches to it); the active tab's key closes it.
		UI::DebugTab request = UI::DebugTab::None;
		if (Input::IsKeyPressed(Key::F3))
			request = UI::DebugTab::Engine;
		if (Input::IsKeyPressed(Key::F4))
			request = UI::DebugTab::Renderer;
		if (Input::IsKeyPressed(Key::F5))
			request = UI::DebugTab::Input;

		if (request != UI::DebugTab::None)
		{
			if (m_ShowDebugWindow && m_ActiveDebugTab == request)
			{
				m_ShowDebugWindow = false;
				m_PendingDebugTab = UI::DebugTab::None;
			}
			else
			{
				m_ShowDebugWindow = true;
				m_PendingDebugTab = request;
			}
		}

		if (m_ShowDebugWindow)
		{
			// Keep requesting the pending tab until the window reports it active --
			// ImGui applies a programmatic tab selection one frame late.
			m_ActiveDebugTab = UI::DebugWindow(&m_ShowDebugWindow, m_PendingDebugTab);
			if (m_ActiveDebugTab == m_PendingDebugTab)
				m_PendingDebugTab = UI::DebugTab::None;
		}
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
		// This runs on the main thread while the render thread may be presenting; the
		// actual swap-chain recreation happens on the render thread at a safe point.
		Renderer::QueueResize(e.GetWidth(), e.GetHeight());
		return false; // let layers react to the new size too
	}

}
