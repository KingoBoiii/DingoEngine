#pragma once
#include "DingoEngine/Common.h"
#include "DingoEngine/Version.h"
#include "DingoEngine/BuildInfo.h"
#include "DingoEngine/Core/LayerStack.h"
#include "DingoEngine/Windowing/Window.h"
#include "DingoEngine/Graphics/GraphicsParams.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/Renderer2D.h"
#include "DingoEngine/Graphics/Renderer3D.h"
#include "DingoEngine/Scene/SceneRenderer.h"
#include "DingoEngine/Asset/AssetManager.h"
#include "DingoEngine/Events/Event.h"
#include "DingoEngine/Events/WindowEvents.h"

#include "DingoEngine/UI/ImGuiParams.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace Dingo
{

	/// The program's command line, as the engine's main() received it.
	struct ApplicationCommandLineArgs
	{
		int Count = 0;			///< Number of entries, argc.
		char** Args = nullptr;	///< The entries, argv; Args[0] is the program name.

		/// The raw argument at index, as in argv. Asserts (Debug builds) that index < Count.
		const char* operator[](int index) const
		{
			DE_CORE_ASSERT(index < Count, "Command line argument index out of bounds");
			return Args[index];
		}

		/// Finds --name (flag) or --name=value. Returns nullopt if not present,
		/// empty string_view for bare flags, or the value string for key=value pairs.
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

	/// Everything the Application is created with; build one in CreateApplication and pass it to
	/// your Application subclass. An aggregate whose members are append-only, so designated
	/// initializers (.Name = ..., .Window = {...}) are safe across minor releases.
	struct ApplicationParams
	{
		ApplicationCommandLineArgs CommandLineArgs;	///< Parsed command line arguments

		/// The folder name of the game's per-user data (Platform::GetUserDataDir): logs and crash
		/// dumps go to its logs/ folder. Empty = the executable's name.
		std::string Name;
		/// Where Dingo.log (the last 5 runs: Dingo.1.log is the run before) and crash dumps go.
		/// Empty = GetUserDataDir(Name)/logs, since a game installed under Program Files can't
		/// write beside itself. --log-here (the working directory) and `--log-dir=<path>` override
		/// it.
		std::filesystem::path LogDirectory;
		/// Where SaveSlots keeps its files. Empty = GetUserDataDir(Name)/saves; `--save-dir=<path>`
		/// overrides it.
		std::filesystem::path SaveDirectory;
		/// A Settings slot to start from (Settings::k_DefaultSlot is "settings"): the window opens
		/// in its display settings, and its master volume and gamepad deadzone are set. Bus volumes
		/// wait for the game's Settings::Apply, after it creates its buses. Empty = none.
		std::string SettingsSlot;

		WindowParams Window;		///< Parameters for the application window
		GraphicsParams Graphics;	///< Parameters for the graphics context
		/// Parameters for the 2D renderer (e.g. per-batch quad capacity)
		Renderer2DParams Renderer2D;
		/// Parameters for the 3D renderer (lighting, mesh batch capacity)
		Renderer3DParams Renderer3D;
		AssetManagerParams Assets;		///< Parameters for the asset manager (asset root directory)

		bool EnableImGui = true;	///< Whether to enable the immediate-mode UI layer
		ImGuiParams ImGui;			///< only used while EnableImGui or EnableDebugOverlays is true

		/// The built-in tabbed Debug window (F3 = engine tab, F4 = renderer tab,
		/// F5 = input tab, F6 = assets tab, F7 = animation tab, F8 = profiler tab, F9 = scene tab;
		/// the active tab's key closes it).
		/// Independent of EnableImGui: the engine brings up the UI backend for it even if the game
		/// uses no UI of its own. Honoured in every build config, Distribution included -- set
		/// false to strip the overlay (and, when EnableImGui is also false, the ImGui backend)
		/// from a shipping build.
		bool EnableDebugOverlays = true;

		/// Keep calling OnUpdate while the window is minimized or unfocused, for a game that must
		/// not stop there (networking, a simulation). Otherwise the app pauses: no OnUpdate and no
		/// rendering, the paused time is left out of the deltas, and audio keeps playing. A
		/// minimized window renders nothing either way. Changeable at runtime, from the main
		/// thread, with Application::SetUpdateInBackground.
		bool UpdateInBackground = false;
	};

	class ImGuiLayer;
	class AudioEngine;

	namespace Internal
	{
		// The engine's main(): EntryPoint.h's main and WinMain call it.
		int Main(int argc, char** argv);
		enum class DebugTab : uint8_t;
		class SwapChain;
	}

	/// The main thread's last frame, split up, for the F8 Profiler tab. Zero in a paused frame.
	struct FrameTimings
	{
		float FrameMs = 0.0f;    ///< The frame's delta, in milliseconds.
		/// Milliseconds Renderer::BeginFrame spent waiting for the render thread to finish the
		/// frame before.
		float WaitMs = 0.0f;
		/// Milliseconds in AssetManager::Update and every layer's OnUpdate, which record the draws.
		float UpdateMs = 0.0f;
		/// Milliseconds in ImGui: every layer's OnImGuiRender, the debug window and recording
		/// ImGui's draws.
		float UIMs = 0.0f;
	};

	/// The program's one application: owns the window, renderers, audio, assets and the layer
	/// stack, and runs the frame loop. Subclass it, override OnInitialize to push your layers, and
	/// return a heap-allocated instance from CreateApplication; the engine's main() initializes,
	/// runs and deletes it. Reach it anywhere with Application::Get(). Use it from the main thread.
	class Application
	{
	public:
		/// There is no default constructor; a subclass passes ApplicationParams to the protected
		/// one.
		Application() = delete;
		/// Tears everything down in order: layers (OnDetach, with the renderer still live), assets,
		/// audio, renderers, swap chain, graphics context, window. The engine deletes the
		/// application; never delete it yourself.
		virtual ~Application();

	public:
		/// Brings up the window, renderers, audio and assets, then calls OnInitialize. The engine
		/// calls it after CreateApplication returns; calling it from CreateApplication first is
		/// harmless. Stops with an error when the graphics back-end can't be created. If
		/// OnInitialize pushed no layer it logs an error and pushes an empty one.
		void Initialize();

		/// Handles a window or input event: close, resize and focus first (the close event ends
		/// there; layers see the others), then each layer's OnEvent from the top (overlays first)
		/// until one sets Handled. The window calls it.
		void OnEvent(Event& e);
		/// The frame loop: runs until Close() or the window closes, then calls OnDestroy().
		/// The engine calls it from main(); it returns when the application ends.
		void Run();

		/// Takes ownership of layer and calls its OnAttach() now. Layers update in push order,
		/// below every overlay. Push from OnInitialize, once the renderers and assets are live.
		void PushLayer(Layer* layer);
		/// Like PushLayer, but the overlay sits above all layers: it updates last and sees events
		/// first.
		void PushOverlay(Layer* overlay);

		/// Ends the frame loop after the current frame.
		void Close();
		/// Closes the app; CreateApplication then runs again and its Application starts on `api`.
		/// Takes effect at the end of the current frame.
		void RequestRestart(GraphicsAPI api);

		/// Queues callback to run once on the main thread at the end of the current frame, after
		/// rendering. A callback that submits another runs it one frame later.
		void SubmitPostExecution(const std::function<void()>& callback)
		{
			m_PostExecutionCallbacks.push_back(callback);
		}

		/// The running application. Undefined (a null dereference) when none exists; see
		/// HasInstance().
		static Application& Get() { return *s_Instance; }
		/// False before construction and after teardown completes - check it from code
		/// that can run outside the application's lifetime (e.g. static-lifetime scenes).
		static bool HasInstance() { return s_Instance != nullptr; }

		/// The application window. Valid from Initialize() until teardown.
		///@{
		Window& GetWindow() { return *m_Window; }
		const Window& GetWindow() const { return *m_Window; }
		///@}

		/// From a zero-sized WindowResizeEvent until the next real size. Nothing renders meanwhile
		/// (Renderer::SkipFrame) and Window::GetWidth/GetHeight read 0.
		bool IsMinimized() const { return m_Minimized; }

		/// Changes ApplicationParams::UpdateInBackground at runtime; main thread only.
		void SetUpdateInBackground(bool updateInBackground) { m_Params.UpdateInBackground = updateInBackground; }
		/// True when layers keep updating while the window is minimized or unfocused.
		bool IsUpdatingInBackground() const { return m_Params.UpdateInBackground; }
		/// Deprecated name for IsUpdatingInBackground; removed in 2.0.
		[[deprecated("use IsUpdatingInBackground; removed in 2.0")]] bool GetUpdateInBackground() const { return IsUpdatingInBackground(); }
		/// The graphics device context created by Initialize(); owned by the application.
		const GraphicsContext& GetGraphicsContext() const { return *m_GraphicsContext; }
		/// The command line the application was created with.
		const ApplicationCommandLineArgs& GetCommandLineArgs() const { return m_Params.CommandLineArgs; }
		/// The engine's subsystems. The application owns them: they are valid from Initialize()
		/// until teardown, and are never deleted by game code.
		///@{
		Renderer2D& GetRenderer2D() const { return *m_Renderer2D; }
		Renderer3D& GetRenderer3D() const { return *m_Renderer3D; }
		SceneRenderer& GetSceneRenderer() const { return *m_SceneRenderer; }
		AudioEngine& GetAudioEngine() const { return *m_AudioEngine; }
		AssetManager& GetAssetManager() const { return *m_AssetManager; }
		///@}
		/// The main thread's last frame, split into wait, update and UI time.
		const FrameTimings& GetFrameTimings() const { return m_FrameTimings; }

		/// The engine's version packed with DE_MAKE_VERSION; decode it with DE_VERSION_MAJOR,
		/// DE_VERSION_MINOR and DE_VERSION_PATCH.
		const uint32_t GetEngineVersion() const { return DE_MAKE_VERSION(DE_ENGINE_VERSION_MAJOR, DE_ENGINE_VERSION_MINOR, DE_ENGINE_VERSION_PATCH); }
		/// The build number the engine was built with (CI's commit count; 0 in a local build).
		const uint32_t GetEngineBuildNumber() const { return DE_ENGINE_VERSION_BUILD; }

		/// ApplicationParams::Name, or the executable's name when that was empty.
		const std::string& GetName() const { return m_Params.Name; }
		/// ApplicationParams::SaveDirectory resolved; created by the first save.
		const std::filesystem::path& GetSaveDirectory() const { return m_Params.SaveDirectory; }

	protected:
		/// For a subclass to call with the params CreateApplication built. Opens the log file,
		/// installs the crash handler and applies ApplicationParams::SettingsSlot to the window
		/// params; the window and renderers only come up in Initialize().
		Application(const ApplicationParams& params = {});

	protected:
		/// Called at the end of Initialize(), with the window, renderers, audio and assets live.
		/// Push your layers here.
		virtual void OnInitialize() {}
		/// Runs once when Run() returns, before teardown: layers, renderer, audio and assets
		/// are all still live.
		virtual void OnDestroy() {}

	private:
		void Destroy();

		bool OnWindowCloseEvent(WindowCloseEvent& e);
		bool OnWindowResizeEvent(WindowResizeEvent& e);
		bool OnWindowFocusEvent(WindowFocusEvent& e);

		// Renders the engine's built-in tabbed Debug window (F3-F9 select its
		// tabs) inside the ImGui frame. Gated at runtime by
		// ApplicationParams::EnableDebugOverlays.
		void RenderDebugOverlays();

		void RunPostExecutionCallbacks();

	private:
		ApplicationParams m_Params;
		Window* m_Window = nullptr;
		GraphicsContext* m_GraphicsContext = nullptr;
		Internal::SwapChain* m_SwapChain = nullptr;
		Renderer2D* m_Renderer2D = nullptr;
		Renderer3D* m_Renderer3D = nullptr;
		SceneRenderer* m_SceneRenderer = nullptr;
		AudioEngine* m_AudioEngine = nullptr;
		AssetManager* m_AssetManager = nullptr;
		Internal::LayerStack m_LayerStack;
		ImGuiLayer* m_ImGuiLayer = nullptr;
		bool m_ShowDebugWindow = false; // built-in tabbed debug window (F3-F9 select a tab)
		Internal::DebugTab m_ActiveDebugTab{};
		Internal::DebugTab m_PendingDebugTab{}; // tab selection requested but not yet applied
		bool m_IsRunning = true;
		bool m_Minimized = false;
		bool m_Focused = true;
		float m_LastFrameTime = 0.0f;
		std::optional<float> m_StartupMasterVolume;
		float m_DeltaTime = 0.0f;
		FrameTimings m_FrameTimings;

		std::vector<std::function<void()>> m_PostExecutionCallbacks;
		// The batch currently being drained, kept as a member so the swap reuses its
		// capacity instead of allocating a vector per frame.
		std::vector<std::function<void()>> m_DrainingPostExecution;

	private:
		inline static Application* s_Instance = nullptr;
		inline static bool s_PendingRestart = false;
		inline static GraphicsAPI s_PendingRestartAPI = GraphicsAPI::Vulkan;

		friend int Internal::Main(int argc, char** argv);
	};

	/// The game's factory, which the game defines (see EntryPoint.h): return a heap-allocated
	/// Application subclass built from args. The engine owns and deletes it; after a
	/// RequestRestart it is called again.
	Application* CreateApplication(ApplicationCommandLineArgs args);

}
