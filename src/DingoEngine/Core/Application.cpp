#include "depch.h"
#include "DingoEngine/Graphics/RendererBackend.h"
#include "DingoEngine/Graphics/SwapChain.h"
#include "DingoEngine/Core/Application.h"
#include "DingoEngine/Core/Timer.h"
#include "DingoEngine/Core/Layer.h"
#include "DingoEngine/Core/CacheManager.h"
#include "DingoEngine/Core/LogLifecycle.h"
#include "DingoEngine/Core/CrashHandler.h"
#include "DingoEngine/Core/Platform.h"
#include "DingoEngine/Core/PathText.h"
#include "DingoEngine/Core/LaunchFlags.h"
#include "DingoEngine/Core/Layers/EmptyLayer.h"
#include "DingoEngine/Core/Input.h"
#include "DingoEngine/Core/KeyCodes.h"
#include "DingoEngine/Core/Profiler.h"
#include "DingoEngine/Core/Settings.h"
#include "DingoEngine/UI/DebugPanels.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Audio/AudioEngine.h"

#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/ImGui/ImGuiLayer.h"
#include <DingoEngine/Graphics/NVRHI/NvrhiGraphicsContext.h>

#include <chrono>
#include <cstdlib>
#include <thread>

namespace Dingo
{
	namespace
	{
		constexpr float k_PacedUpdateInterval = 1.0f / 60.0f;
		constexpr float k_PausedWaitTimeout = 0.1f;
	}

	Application::Application(const ApplicationParams& params)
		: m_Params(params)
	{
		s_Instance = this;

		if (auto value = m_Params.CommandLineArgs.Get("graphics"))
		{
			const std::optional<GraphicsAPI> api = Internal::ParseGraphicsFlag(*value);
			if (!api)
				DE_CORE_WARN("Ignoring --graphics={} (expected vulkan, dx11, dx12 or headless).", *value);
#ifndef DE_PLATFORM_WINDOWS
			else if (*api == GraphicsAPI::DirectX11 || *api == GraphicsAPI::DirectX12)
				DE_CORE_WARN("Ignoring --graphics={}: DirectX runs only on Windows.", *value);
#endif
			else
				m_Params.Graphics.GraphicsAPI = *api;
		}

		if (s_PendingRestart)
		{
			m_Params.Graphics.GraphicsAPI = s_PendingRestartAPI;
			s_PendingRestart = false;
		}

		if (m_Params.Name.empty())
			m_Params.Name = Internal::PathToUtf8(Platform::GetExecutablePath().stem());

		std::filesystem::path logDirectory = m_Params.LogDirectory;
		if (auto directory = m_Params.CommandLineArgs.Get("log-dir"); directory && !directory->empty())
			logDirectory = std::filesystem::path(*directory);
		else if (m_Params.CommandLineArgs.Get("log-here"))
			logDirectory = std::filesystem::current_path();
		if (logDirectory.empty())
			logDirectory = Platform::GetUserDataDir(m_Params.Name) / "logs";

		if (auto directory = m_Params.CommandLineArgs.Get("save-dir"); directory && !directory->empty())
			m_Params.SaveDirectory = std::filesystem::path(*directory);
		if (m_Params.SaveDirectory.empty())
			m_Params.SaveDirectory = Platform::GetUserDataDir(m_Params.Name) / "saves";

		Internal::OpenLogFile(logDirectory);
		Internal::CrashHandler::Install(Log::GetFilePath(), logDirectory, m_Params.Name);

		if (!m_Params.SettingsSlot.empty() && SaveSlots::Exists(m_Params.SettingsSlot))
		{
			if (const std::optional<SaveData> data = SaveSlots::Read(m_Params.SettingsSlot))
			{
				Settings defaults;
				defaults.Display.Mode = m_Params.Window.Mode;
				defaults.Display.Width = m_Params.Window.Width;
				defaults.Display.Height = m_Params.Window.Height;
				defaults.Display.FullscreenMode = m_Params.Window.FullscreenMode;
				defaults.Display.VSync = m_Params.Window.VSync;
				defaults.Input.GamepadDeadzone = Input::GetGamepadDeadzone();
				const Settings settings = Settings::FromSaveData(*data, defaults);
				settings.ApplyTo(m_Params.Window);
				Input::SetGamepadDeadzone(settings.Input.GamepadDeadzone);
				m_StartupMasterVolume = settings.Audio.MasterVolume;
			}
		}

		if (auto value = m_Params.CommandLineArgs.Get("window"))
		{
			if (const std::optional<glm::ivec2> size = Internal::ParseWindowFlag(*value))
			{
				m_LaunchDisplayRequest = m_Params.Window;
				m_Params.Window.Width = size->x;
				m_Params.Window.Height = size->y;
				m_Params.Window.Mode = WindowMode::Windowed;
			}
			else
			{
				DE_CORE_WARN("Ignoring --window={} (expected WxH, from {}x{} to {}x{}).", *value, Internal::k_MinLaunchWindowSize.x, Internal::k_MinLaunchWindowSize.y,
					Internal::k_MaxLaunchWindowSize.x, Internal::k_MaxLaunchWindowSize.y);
			}
		}
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
		// A headless app has no window, so the asset manager marks a finished Initialize().
		if (m_AssetManager)
			return;

		Internal::CacheManager::Initialize();

		if (!IsHeadless())
		{
			m_Window = new Window(m_Params.Window);
			m_Window->m_VSyncForcedOff = Internal::IsLaunchFlagOn(m_Params.CommandLineArgs.Get("no-vsync"));
			m_Window->m_DisplayRequest = m_LaunchDisplayRequest;
			m_Window->Initialize();
			m_Window->SetEventCallback(DE_BIND_EVENT_FN(Application::OnEvent));
			m_Focused = m_Window->IsFocused();

			GraphicsParams graphicsParams = m_Params.Graphics;
			if (const auto validation = m_Params.CommandLineArgs.Get("vulkan-validation"))
				graphicsParams.VulkanValidation = !(*validation == "off" || *validation == "false" || *validation == "0" || *validation == "no");
			m_GraphicsContext = GraphicsContext::Create(graphicsParams, m_Window->GetNativeWindowHandle());
			if (!m_GraphicsContext)
				FatalError("No graphics device could be created. See the log for why.");
			m_GraphicsContext->Initialize();

			m_SwapChain = Internal::SwapChain::Create(Internal::SwapChainParams()
				.SetNativeWindowHandle(m_Window->GetNativeWindowHandle())
				.SetWidth(m_Window->GetWidth())
				.SetHeight(m_Window->GetHeight())
				.SetVSync(m_Params.Window.VSync && !m_Window->m_VSyncForcedOff));
			m_SwapChain->Initialize();

			Internal::RendererBackend::Initialize(m_SwapChain);

			m_Renderer2D = Renderer2D::Create(m_Params.Renderer2D);
			m_Renderer3D = Renderer3D::Create(m_Params.Renderer3D);
			m_SceneRenderer = new SceneRenderer(*m_Renderer2D, *m_Renderer3D);
		}

		// Audio is independent of the graphics/window stack (miniaudio owns its own
		// device thread); bring it up here so it's live before OnInitialize().
		m_AudioEngine = AudioEngine::Create();
		m_AudioEngine->Initialize();
		if (m_StartupMasterVolume)
			m_AudioEngine->SetMasterVolume(*m_StartupMasterVolume);

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

			m_Params.EnableImGui = false; // Disable the UI layer if no layers are pushed
			return;
		}

		// Bring up the ImGui backend if the game wants UI, or if the engine's built-in
		// debug overlays are enabled -- so the renderer-stats window works even in
		// projects that use no UI of their own, in every build config (Distribution too).
		bool enableImGui = !IsHeadless() && (m_Params.EnableImGui || m_Params.EnableDebugOverlays);
		if (enableImGui)
		{
			m_ImGuiLayer = new ImGuiLayer(m_Params.ImGui);
			PushOverlay(m_ImGuiLayer);
		}

		if (m_ImGuiLayer && m_Params.EnableDebugOverlays)
			DE_CORE_INFO("Debug window enabled - press F3 (engine), F4 (renderer), F5 (input), F6 (assets), F7 (animation), F8 (profiler) or F9 (scene) to open its tabs.");
	}

	void Application::Destroy()
	{
		// Park the render thread first so the GPU is idle before any resources are freed.
		Internal::RendererBackend::Shutdown();

		// Detach layers while the renderer can still answer queries: OnDetach is where a
		// layer frees its GPU resources, and doing that legitimately involves asking for
		// the white texture, a sampler or the current framebuffer.
		m_LayerStack.Clear();

		// Only now drop the renderer's own resources and state.
		Internal::RendererBackend::Destroy();

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

		// Last: glfwTerminate closes the X connection, which Mesa's X11 swap chain still
		// talks to while it is destroyed (X errors, crashes and hangs on close otherwise).
		if (m_Window)
		{
			m_Window->Shutdown();
			delete m_Window;
			m_Window = nullptr;
		}

		Internal::CacheManager::Shutdown();

		DE_CORE_INFO("Shut down.");
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
		if (IsHeadless())
			RunHeadless();
		else
			RunWindowed();

		DE_CORE_INFO("Shutting down.");

		// Here rather than in Destroy(): that runs from ~Application, where the derived
		// class is already gone and the call would reach only the empty base hook.
		OnDestroy();
	}

	void Application::RunHeadless()
	{
		Timer timer;
		auto next = std::chrono::steady_clock::now();

		DE_PROFILE_THREAD("Main");

		while (m_IsRunning)
		{
			DE_PROFILE_FRAME();
			DE_PROFILE_SCOPE("Application::Run");

			const float frameStart = timer.Elapsed();
			m_DeltaTime = frameStart - m_LastFrameTime;
			m_LastFrameTime = frameStart;

			if (m_AudioEngine)
				m_AudioEngine->Update();

			if (m_AssetManager)
			{
				DE_PROFILE_SCOPE("AssetManager::Update");
				m_AssetManager->Update(m_DeltaTime);
			}

			for (Layer* layer : m_LayerStack)
			{
				DE_PROFILE_SCOPE_TEXT("Layer::OnUpdate", layer->GetName());
				layer->OnUpdate(m_DeltaTime);
			}

			m_FrameTimings = {};
			m_FrameTimings.FrameMs = m_DeltaTime * 1000.0f;
			m_FrameTimings.UpdateMs = (timer.Elapsed() - frameStart) * 1000.0f;

			RunPostExecutionCallbacks();

			// Nothing presents, so nothing paces the loop: sleep to a running deadline instead of
			// spinning, so the coarse Windows timer evens out to the target rate on average.
			next += std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<float>(k_PacedUpdateInterval));
			const auto now = std::chrono::steady_clock::now();
			if (next < now)
				next = now;
			else if (m_IsRunning)
				std::this_thread::sleep_until(next);
		}
	}

	void Application::RunWindowed()
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

		DE_PROFILE_THREAD("Main");

		while (m_IsRunning)
		{
			DE_PROFILE_FRAME();
			DE_PROFILE_SCOPE("Application::Run");

			float time = timer.Elapsed();
			m_DeltaTime = time - m_LastFrameTime;
			m_LastFrameTime = time;

			const bool resuming = !snapshotInput;

			// Snapshotted once after each round of OnUpdate, before the next poll, so every edge
			// reaches exactly one OnUpdate: the first one after a pause sees what changed during it.
			if (snapshotInput)
				Input::Update();
			Input::UpdateRumble();

			if (!shouldUpdate())
			{
				m_Window->WaitEvents(k_PausedWaitTimeout);
			}
			else if (m_Minimized)
			{
				// No swap-chain image to render into: pace the updates instead of spinning, often
				// enough that a game's networking keeps answering.
				m_Window->WaitEvents(k_PacedUpdateInterval - (timer.Elapsed() - lastWaitEnd));
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
				m_FrameTimings = {};
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
			const float waitStart = timer.Elapsed();
			{
				DE_PROFILE_SCOPE("Renderer::BeginFrame");
				if (render)
					Internal::RendererBackend::BeginFrame();
				else
					Internal::RendererBackend::SkipFrame();
			}
			const float updateStart = timer.Elapsed();

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
			{
				DE_PROFILE_SCOPE("AssetManager::Update");
				m_AssetManager->Update(m_DeltaTime); // finalize async loads, poll hot-reload
			}

			for (Layer* layer : m_LayerStack)
			{
				DE_PROFILE_SCOPE_TEXT("Layer::OnUpdate", layer->GetName());
				layer->OnUpdate(m_DeltaTime);
			}
			const float uiStart = timer.Elapsed();

			if (render)
			{
				if (m_ImGuiLayer)
				{
					DE_PROFILE_SCOPE("ImGui");
					Renderer::BeginGpuTimer("ImGui");
					m_ImGuiLayer->Begin();

					if (m_Params.EnableImGui)
					{
						for (Layer* layer : m_LayerStack)
						{
							DE_PROFILE_SCOPE_TEXT("Layer::OnImGuiRender", layer->GetName());
							layer->OnImGuiRender();
						}
					}

					if (m_Params.EnableDebugOverlays)
						RenderDebugOverlays();

					m_ImGuiLayer->End();
					Renderer::EndGpuTimer();
				}

				DE_PROFILE_SCOPE("Renderer::EndFrame");
				Internal::RendererBackend::EndFrame();
				renderedOnce = true;
			}
			else
			{
				++minimizedUpdates;
				minimizedTime += m_DeltaTime;
			}

			const float frameEnd = timer.Elapsed();
			m_FrameTimings.FrameMs = m_DeltaTime * 1000.0f;
			m_FrameTimings.WaitMs = (updateStart - waitStart) * 1000.0f;
			m_FrameTimings.UpdateMs = (uiStart - updateStart) * 1000.0f;
			m_FrameTimings.UIMs = (frameEnd - uiStart) * 1000.0f;

			RunPostExecutionCallbacks();
		}
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
		// F7 = Animation, F8 = Profiler, F9 = Scene.
		// A key opens the window on its tab (or switches to it); the active tab's key
		// closes it.
		Internal::DebugTab request = Internal::DebugTab::None;
		if (Input::IsKeyPressed(Key::F3))
			request = Internal::DebugTab::Engine;
		if (Input::IsKeyPressed(Key::F4))
			request = Internal::DebugTab::Renderer;
		if (Input::IsKeyPressed(Key::F5))
			request = Internal::DebugTab::Input;
		if (Input::IsKeyPressed(Key::F6))
			request = Internal::DebugTab::Assets;
		if (Input::IsKeyPressed(Key::F7))
			request = Internal::DebugTab::Animation;
		if (Input::IsKeyPressed(Key::F8))
			request = Internal::DebugTab::Profiler;
		if (Input::IsKeyPressed(Key::F9))
			request = Internal::DebugTab::Scene;

		if (request != Internal::DebugTab::None)
		{
			if (m_ShowDebugWindow && m_ActiveDebugTab == request)
			{
				m_ShowDebugWindow = false;
				m_PendingDebugTab = Internal::DebugTab::None;
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
			m_ActiveDebugTab = Internal::DebugWindow(&m_ShowDebugWindow, m_PendingDebugTab);
			if (m_ActiveDebugTab == m_PendingDebugTab)
				m_PendingDebugTab = Internal::DebugTab::None;
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

	bool Application::OnWindowCloseEvent(WindowCloseEvent& e)
	{
		DE_CORE_INFO("Window close requested.");
		m_IsRunning = false; // Stop the application loop
		return true;
	}

	bool Application::OnWindowResizeEvent(WindowResizeEvent& e)
	{
		// This runs on the main thread while the render thread may be presenting; the
		// actual swap-chain recreation happens on the render thread at a safe point.
		m_Minimized = e.GetWidth() == 0 || e.GetHeight() == 0;
		Internal::RendererBackend::QueueResize(e.GetWidth(), e.GetHeight());
		return false; // let layers react to the new size too
	}

	bool Application::OnWindowFocusEvent(WindowFocusEvent& e)
	{
		m_Focused = e.IsFocused();
		return false;
	}

}
