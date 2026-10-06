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
	namespace
	{
		constexpr float k_MinimizedUpdateInterval = 1.0f / 60.0f;
		constexpr float k_PausedWaitTimeout = 0.1f;
	}

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
		// s_Instance is set unconditionally by the constructor, so it can't detect a
		// second Initialize() call; m_Window is null until this function creates it.
		DE_CORE_ASSERT(!m_Window, "Application already initialized. Cannot initialize again.");

		CacheManager::Initialize();

		m_Window = new Window(m_Params.Window);
		m_Window->Initialize();
		m_Window->SetEventCallback(DE_BIND_EVENT_FN(Application::OnEvent));
		m_Focused = m_Window->IsFocused();

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
			DE_CORE_INFO("Debug window enabled - press F3 (engine), F4 (renderer), F5 (input), F6 (assets) or F7 (animation) to open its tabs.");
	}

	void Application::Destroy()
	{
		// Park the render thread first so the GPU is idle before any resources are freed.
		Renderer::Shutdown();

		// Detach layers while the renderer can still answer queries: OnDetach is where a
		// layer frees its GPU resources, and doing that legitimately involves asking for
		// the white texture, a sampler or the current framebuffer.
		m_LayerStack.Clear();

		// Only now drop the renderer's own resources and state.
		Renderer::Destroy();

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
		dispatcher.Dispatch<WindowFocusEvent>(DE_BIND_EVENT_FN(Application::OnWindowFocusEvent));

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
		float lastWaitEnd = 0.0f;
		bool snapshotInput = true;
		bool renderedOnce = false;
		uint32_t minimizedUpdates = 0;
		float minimizedTime = 0.0f;

		// The first frame renders even without focus, so a window that opens unfocused isn't blank.
		const auto shouldUpdate = [&]()
		{
			return (!renderedOnce && !m_Minimized) || m_Params.UpdateInBackground || (!m_Minimized && m_Focused);
		};

		while (m_IsRunning)
		{
			float time = timer.Elapsed();
			m_DeltaTime = time - m_LastFrameTime;
			m_LastFrameTime = time;

			const bool resuming = !snapshotInput;

			// Snapshotted once after each round of OnUpdate, before the next poll, so every edge
			// reaches exactly one OnUpdate: the first one after a pause sees what changed during it.
			if (snapshotInput)
				Input::Update();

			if (!shouldUpdate())
			{
				m_Window->WaitEvents(k_PausedWaitTimeout);
			}
			else if (m_Minimized)
			{
				// No swap-chain image to render into: pace the updates instead of spinning, often
				// enough that a game's networking keeps answering.
				m_Window->WaitEvents(k_MinimizedUpdateInterval - (timer.Elapsed() - lastWaitEnd));
				lastWaitEnd = timer.Elapsed();
			}
			else
			{
				m_Window->Update();
			}

			if (m_AudioEngine)
				m_AudioEngine->Update(); // reap finished one-shots

			snapshotInput = shouldUpdate();
			if (!snapshotInput)
			{
				m_LastFrameTime = timer.Elapsed(); // a paused stretch is not a frame delta
				RunPostExecutionCallbacks();
				continue;
			}

			if (resuming)
			{
				m_LastFrameTime = timer.Elapsed(); // nor is the wait for the event that ended it
				Input::Resume();
			}

			const bool render = !m_Minimized;
			if (render)
				Renderer::BeginFrame();
			else
				Renderer::SkipFrame();

			if (render && minimizedUpdates > 0)
			{
				DE_CORE_INFO("Window restored: layers updated {} times over {:.1f} s while it was minimized.", minimizedUpdates, minimizedTime);
				minimizedUpdates = 0;
				minimizedTime = 0.0f;
			}

			// After BeginFrame or SkipFrame: the render thread is parked until the next EndFrame,
			// so the GPU work in here (texture uploads, shader recompiles) can't race its
			// garbage-collection/present pass on the NVRHI device.
			if (m_AssetManager)
				m_AssetManager->Update(m_DeltaTime); // finalize async loads, poll hot-reload

			for (Layer* layer : m_LayerStack)
			{
				layer->OnUpdate(m_DeltaTime);
			}

			if (render)
			{
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
				renderedOnce = true;
			}
			else
			{
				++minimizedUpdates;
				minimizedTime += m_DeltaTime;
			}

			RunPostExecutionCallbacks();
		}

		// Here rather than in Destroy(): that runs from ~Application, where the derived
		// class is already gone and the call would reach only the empty base hook.
		OnDestroy();
	}

	void Application::RunPostExecutionCallbacks()
	{
		// Drain into a local: a callback is free to SubmitPostExecution (RequestRestart
		// already is one), which would push into the vector being iterated - dangling the
		// iterator on a reallocation - and the clear() would then drop the new entry
		// anyway. Whatever a callback submits runs on the next frame instead.
		m_DrainingPostExecution.swap(m_PostExecutionCallbacks);
		for (const auto& callback : m_DrainingPostExecution)
		{
			callback();
		}
		m_DrainingPostExecution.clear();
	}

	void Application::RenderDebugOverlays()
	{
		// One tabbed debug window: F3 = Engine, F4 = Renderer, F5 = Input, F6 = Assets,
		// F7 = Animation.
		// A key opens the window on its tab (or switches to it); the active tab's key
		// closes it.
		UI::DebugTab request = UI::DebugTab::None;
		if (Input::IsKeyPressed(Key::F3))
			request = UI::DebugTab::Engine;
		if (Input::IsKeyPressed(Key::F4))
			request = UI::DebugTab::Renderer;
		if (Input::IsKeyPressed(Key::F5))
			request = UI::DebugTab::Input;
		if (Input::IsKeyPressed(Key::F6))
			request = UI::DebugTab::Assets;
		if (Input::IsKeyPressed(Key::F7))
			request = UI::DebugTab::Animation;

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
		m_Minimized = e.GetWidth() == 0 || e.GetHeight() == 0;
		Renderer::QueueResize(e.GetWidth(), e.GetHeight());
		return false; // let layers react to the new size too
	}

	bool Application::OnWindowFocusEvent(WindowFocusEvent& e)
	{
		m_Focused = e.IsFocused();
		return false;
	}

}
