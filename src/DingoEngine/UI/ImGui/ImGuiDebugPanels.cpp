#include "depch.h"

#include "DingoEngine/UI/DebugPanels.h"

#include "DingoEngine/Core/Application.h"
#include "DingoEngine/Core/Input.h"
#include "DingoEngine/Graphics/Renderer2D.h"
#include "DingoEngine/Graphics/Renderer3D.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Windowing/Window.h"
#include "DingoEngine/Audio/AudioEngine.h"
#include "DingoEngine/Asset/AssetManager.h"
#include "DingoEngine/Version.h"
#include "DingoEngine/BuildInfo.h"

#include <imgui.h>

#include <algorithm>
#include <format>
#include <vector>

// ImGui-backed implementation of the engine's renderer debug panels
// (DingoEngine/UI/DebugPanels.h). Like ImGuiUI.cpp, this is one of the only
// translation units that includes <imgui.h>; clients call only Dingo::UI.

namespace Dingo::UI
{

	namespace
	{
		// "label  [=====      ] used / capacity" — a labelled usage bar for a
		// per-scene budget (the 3D vertex/index caps). ImGui tints the fill.
		void BudgetBar(const char* label, uint32_t used, uint32_t capacity)
		{
			const float fraction = capacity > 0 ? static_cast<float>(used) / static_cast<float>(capacity) : 0.0f;
			const std::string overlay = std::format("{} / {}", used, capacity);

			ImGui::TextUnformatted(label);
			ImGui::SameLine(140.0f);
			ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f), overlay.c_str());
		}

		// "X.X FPS   X.XX ms/frame" from ImGui's own frame timing, shared by
		// FrameTimingSection and RendererStatsWindow's performance block.
		void FpsLine()
		{
			const ImGuiIO& io = ImGui::GetIO();
			const float frameMs = 1000.0f / (io.Framerate > 0.0f ? io.Framerate : 1.0f);
			ImGui::Text("%.1f FPS   %.2f ms/frame", io.Framerate, frameMs);
		}

		ImVec4 AssetStateColor(AssetState state)
		{
			switch (state)
			{
				case AssetState::Ready:   return ImVec4(0.35f, 0.85f, 0.40f, 1.0f);
				case AssetState::Queued:
				case AssetState::Loading: return ImVec4(0.95f, 0.80f, 0.30f, 1.0f);
				case AssetState::Failed:  return ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
				case AssetState::Unloaded:
				default:                  return ImVec4(0.60f, 0.60f, 0.60f, 1.0f);
			}
		}

		// Registry snapshot ordered by path: the registry is an unordered_map, so
		// iterating it directly would reshuffle rows between frames.
		std::vector<AssetMetadata> SortedRegistry(const AssetManager& assets)
		{
			std::vector<AssetMetadata> entries;
			entries.reserve(assets.GetRegistry().size());
			for (const auto& [handle, metadata] : assets.GetRegistry())
				entries.push_back(metadata);

			std::sort(entries.begin(), entries.end(), [](const AssetMetadata& a, const AssetMetadata& b)
			{
				return a.FilePath.generic_string() < b.FilePath.generic_string();
			});
			return entries;
		}
	}

	void RendererStatsSection()
	{
		// ---- Performance (from ImGui's own frame timing) --------------------
		const ImGuiIO& io = ImGui::GetIO();
		const float frameMs = 1000.0f / (io.Framerate > 0.0f ? io.Framerate : 1.0f);

		// Rolling frame-time history for the graph (a simple ring buffer).
		static float s_FrameTimes[120] = {};
		static int s_Cursor = 0;
		s_FrameTimes[s_Cursor] = frameMs;
		s_Cursor = (s_Cursor + 1) % IM_ARRAYSIZE(s_FrameTimes);

		float average = 0.0f;
		for (const float value : s_FrameTimes)
			average += value;
		average /= static_cast<float>(IM_ARRAYSIZE(s_FrameTimes));

		ImGui::TextUnformatted("Performance");
		ImGui::Separator();
		FpsLine();

		const std::string overlay = std::format("avg {:.2f} ms", average);
		// Fixed 0..33.34 ms axis: the top of the graph is the 30 FPS line, so it reads
		// as a stable reference rather than an axis that auto-rescales every frame.
		ImGui::PlotLines("##frametime", s_FrameTimes, IM_ARRAYSIZE(s_FrameTimes), s_Cursor,
			overlay.c_str(), 0.0f, 33.34f, ImVec2(0.0f, 60.0f));

		// ---- Renderer2D -----------------------------------------------------
		const Renderer2D::Statistics& stats2D = Application::Get().GetRenderer2D().GetStatistics();

		ImGui::Spacing();
		ImGui::TextUnformatted("Renderer2D  (most recent scene)");
		ImGui::Separator();
		ImGui::Text("Draw calls : %u", stats2D.DrawCalls);
		ImGui::Text("Quads      : %u", stats2D.QuadCount);
		ImGui::Text("Circles    : %u", stats2D.CircleCount);
		ImGui::Text("Text quads : %u", stats2D.TextQuadCount);
		ImGui::Text("Vertices   : %u    Indices : %u", stats2D.GetVertexCount(), stats2D.GetIndexCount());

		// ---- Renderer3D -----------------------------------------------------
		const Renderer3D& renderer3D = Application::Get().GetRenderer3D();
		const Renderer3D::Statistics& stats3D = renderer3D.GetStatistics();
		const Renderer3DCapabilities& caps3D = renderer3D.GetCapabilities();

		ImGui::Spacing();
		ImGui::TextUnformatted("Renderer3D  (most recent scene)");
		ImGui::Separator();
		ImGui::Text("Draw calls : %u   (one per material)", stats3D.DrawCalls);
		ImGui::Text("Meshes     : %u submitted", stats3D.SubmittedMeshes);
		if (stats3D.DroppedMeshes > 0)
			ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
				"Dropped    : %u  (raise Renderer3D MaxVertices/MaxIndices)", stats3D.DroppedMeshes);
		else
			ImGui::Text("Dropped    : 0");

		ImGui::Spacing();
		BudgetBar("Vertices", stats3D.VertexCount, caps3D.MaxVertices);
		BudgetBar("Indices", stats3D.IndexCount, caps3D.MaxIndices);
	}

	void RendererStatsWindow(bool* open)
	{
		// Begin() returns false when collapsed/clipped; skip the body but still End().
		if (!ImGui::Begin("Renderer Stats", open))
		{
			ImGui::End();
			return;
		}

		RendererStatsSection();

		ImGui::End();
	}

	void EngineInfoSection()
	{
		const uint32_t version = Application::Get().GetEngineVersion();

		ImGui::TextUnformatted("Engine");
		ImGui::Separator();
		ImGui::Text("Version : %u.%u.%u  (build %u)",
			DE_VERSION_MAJOR(version), DE_VERSION_MINOR(version), DE_VERSION_PATCH(version),
			Application::Get().GetEngineBuildNumber());
	}

	void GraphicsInfoSection()
	{
		const GraphicsContext& context = Application::Get().GetGraphicsContext();
		const AdapterInfo& adapter = context.GetAdapterInfo();

		ImGui::TextUnformatted("Graphics");
		ImGui::Separator();

		const char* apiName = "Unknown";
		switch (context.GetGraphicsAPI())
		{
			case GraphicsAPI::Headless:   apiName = "Headless";  break;
			case GraphicsAPI::Vulkan:     apiName = "Vulkan";    break;
			case GraphicsAPI::DirectX11:  apiName = "DirectX11"; break;
			case GraphicsAPI::DirectX12:  apiName = "DirectX12"; break;
		}
		ImGui::Text("API     : %s", apiName);

		if (!adapter.Name.empty())
		{
			ImGui::Text("Adapter : %s", adapter.Name.c_str());
			ImGui::Text("Vendor  : %s", GraphicsContext::VendorName(adapter.VendorID).c_str());
			if (adapter.DedicatedVideoMemory > 0)
				ImGui::Text("VRAM    : %.1f MB", static_cast<double>(adapter.DedicatedVideoMemory) / (1024.0 * 1024.0));
		}
	}

	void WindowInfoSection()
	{
		const Window& window = Application::Get().GetWindow();

		ImGui::TextUnformatted("Window");
		ImGui::Separator();
		ImGui::Text("Size : %d x %d", window.GetWidth(), window.GetHeight());
	}

	void FrameTimingSection()
	{
		ImGui::TextUnformatted("Frame Timing");
		ImGui::Separator();
		FpsLine();
	}

	void AudioStatsSection()
	{
		const AudioEngine& audio = Application::Get().GetAudioEngine();

		ImGui::TextUnformatted("Audio");
		ImGui::Separator();
		ImGui::Text("Status        : %s", audio.IsValid() ? "Valid" : "Invalid");
		ImGui::Text("Master volume : %.2f", audio.GetMasterVolume());
		ImGui::Text("Active sounds : %u", audio.GetActiveSoundCount());
	}

	void EngineStatsWindow(bool* open)
	{
		if (!ImGui::Begin("Engine Stats", open))
		{
			ImGui::End();
			return;
		}

		EngineInfoSection();

		ImGui::Spacing();
		GraphicsInfoSection();

		ImGui::Spacing();
		WindowInfoSection();

		ImGui::Spacing();
		FrameTimingSection();

		ImGui::Spacing();
		AudioStatsSection();

		ImGui::End();
	}

	void MouseInputSection()
	{
		const glm::vec2 position = Input::GetMousePosition();
		const glm::vec2 delta = Input::GetMouseDelta();
		const glm::vec2 scroll = Input::GetMouseScrollDelta();

		ImGui::TextUnformatted("Mouse");
		ImGui::Separator();
		ImGui::Text("Position : %.0f, %.0f", position.x, position.y);
		ImGui::Text("Delta    : %+.1f, %+.1f", delta.x, delta.y);
		ImGui::Text("Scroll   : %+.1f, %+.1f", scroll.x, scroll.y);

		std::string held;
		static constexpr const char* s_ButtonNames[] = { "Left", "Right", "Middle", "Button3", "Button4", "Button5" };
		for (int i = 0; i < IM_ARRAYSIZE(s_ButtonNames); i++)
		{
			if (!Input::IsMouseButtonDown(static_cast<MouseButton>(i)))
				continue;
			if (!held.empty())
				held += ", ";
			held += s_ButtonNames[i];
		}
		ImGui::Text("Buttons  : %s", held.empty() ? "-" : held.c_str());
	}

	void KeyboardInputSection()
	{
		ImGui::TextUnformatted("Keyboard");
		ImGui::Separator();

		std::string held;
		for (uint16_t code = static_cast<uint16_t>(KeyCode::Space); code <= static_cast<uint16_t>(KeyCode::Menu); code++)
		{
			if (!Input::IsKeyDown(static_cast<KeyCode>(code)))
				continue;
			if (!held.empty())
				held += ", ";
			held += ToString(static_cast<KeyCode>(code));
		}
		ImGui::Text("Held : %s", held.empty() ? "-" : held.c_str());
	}

	void GamepadInputSection()
	{
		ImGui::TextUnformatted("Gamepads");
		ImGui::Separator();

		float deadzone = Input::GetGamepadDeadzone();
		if (ImGui::SliderFloat("Deadzone", &deadzone, 0.0f, 0.95f, "%.2f"))
			Input::SetGamepadDeadzone(deadzone);

		bool any = false;
		for (uint32_t pad = 0; pad < MaxGamepads; pad++)
		{
			if (!Input::IsGamepadConnected(pad))
				continue;
			any = true;

			ImGui::Spacing();
			ImGui::Text("Slot %u : %s ('%s')", pad, ToString(Input::GetGamepadType(pad)), Input::GetGamepadName(pad).c_str());

			std::string held;
			for (uint8_t button = 0; button < GamepadButtonCount; button++)
			{
				if (!Input::IsGamepadButtonDown(static_cast<GamepadButton>(button), pad))
					continue;
				if (!held.empty())
					held += ", ";
				held += ToString(static_cast<GamepadButton>(button));
			}
			ImGui::Text("Buttons : %s", held.empty() ? "-" : held.c_str());

			const glm::vec2 left = Input::GetGamepadLeftStick(pad);
			const glm::vec2 right = Input::GetGamepadRightStick(pad);
			ImGui::Text("Left stick  : %+.2f, %+.2f   (raw %+.2f, %+.2f)", left.x, left.y,
				Input::GetGamepadAxisRaw(GamepadAxis::LeftX, pad), Input::GetGamepadAxisRaw(GamepadAxis::LeftY, pad));
			ImGui::Text("Right stick : %+.2f, %+.2f   (raw %+.2f, %+.2f)", right.x, right.y,
				Input::GetGamepadAxisRaw(GamepadAxis::RightX, pad), Input::GetGamepadAxisRaw(GamepadAxis::RightY, pad));

			// Triggers as 0..1 usage bars (the remapped, deadzone-filtered values).
			ImGui::ProgressBar(Input::GetGamepadAxis(GamepadAxis::LeftTrigger, pad), ImVec2(120.0f, 0.0f), "LT");
			ImGui::SameLine();
			ImGui::ProgressBar(Input::GetGamepadAxis(GamepadAxis::RightTrigger, pad), ImVec2(120.0f, 0.0f), "RT");
		}

		if (!any)
			ImGui::TextUnformatted("No gamepads connected.");
	}

	void AssetSummarySection()
	{
		const AssetManager& assets = Application::Get().GetAssetManager();

		const uint32_t registered = assets.GetRegisteredCount();
		const uint32_t loaded = assets.GetLoadedCount();
		const uint32_t pending = assets.GetPendingCount();

		ImGui::TextUnformatted("Assets");
		ImGui::Separator();
		// Wrapped: absolute asset roots are long enough to clip in a narrow window.
		ImGui::TextWrapped("Root : %s", assets.GetRootDirectory().string().c_str());
		ImGui::Text("Registered : %u    Loaded : %u    In flight : %u", registered, loaded, pending);

		const float fraction = registered > 0 ? static_cast<float>(loaded) / static_cast<float>(registered) : 0.0f;
		const std::string overlay = pending > 0
			? std::format("loading... {} / {}", loaded, registered)
			: std::format("{} / {}", loaded, registered);
		ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f), overlay.c_str());

		uint32_t perType[6] = {};
		uint32_t perState[5] = {};
		for (const auto& [handle, metadata] : assets.GetRegistry())
		{
			perType[static_cast<size_t>(metadata.Type)]++;
			perState[static_cast<size_t>(metadata.State)]++;
		}

		ImGui::Spacing();
		ImGui::TextUnformatted("By type");
		ImGui::Separator();
		for (size_t type = 1; type < IM_ARRAYSIZE(perType); type++)
		{
			if (perType[type] == 0)
				continue;
			ImGui::Text("%-10s : %u", AssetTypeToString(static_cast<AssetType>(type)), perType[type]);
		}
		if (registered == 0)
			ImGui::TextUnformatted("No assets registered.");

		ImGui::Spacing();
		ImGui::TextUnformatted("By state");
		ImGui::Separator();
		for (size_t state = 0; state < IM_ARRAYSIZE(perState); state++)
		{
			if (perState[state] == 0)
				continue;
			const AssetState value = static_cast<AssetState>(state);
			ImGui::TextColored(AssetStateColor(value), "%-10s : %u", AssetStateToString(value), perState[state]);
		}
	}

	void AssetRegistrySection()
	{
		AssetManager& assets = Application::Get().GetAssetManager();

		ImGui::TextUnformatted("Registry");
		ImGui::Separator();

		bool hotReload = assets.IsHotReloadEnabled();
		if (ImGui::Checkbox("Hot-reload textures & shaders", &hotReload))
			assets.SetHotReloadEnabled(hotReload);

		static char s_Filter[128] = "";
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##assetfilter", "filter by path...", s_Filter, IM_ARRAYSIZE(s_Filter));

		// Buttons act on the manager, which mutates the registry - collect the request
		// and apply it after the table is closed.
		enum class Action { None, Load, Reload };
		Action action = Action::None;
		AssetHandle target = k_InvalidAsset;

		const std::vector<AssetMetadata> entries = SortedRegistry(assets);

		if (ImGui::BeginTable("##assettable", 4,
			ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
			ImVec2(0.0f, 240.0f)))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 62.0f);
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 74.0f);
			ImGui::TableSetupColumn("Path");
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 118.0f);
			ImGui::TableHeadersRow();

			for (const AssetMetadata& metadata : entries)
			{
				const std::string path = metadata.FilePath.generic_string();
				if (s_Filter[0] != '\0' && path.find(s_Filter) == std::string::npos)
					continue;

				ImGui::PushID(static_cast<int>(static_cast<uint64_t>(metadata.Handle) & 0x7FFFFFFF));
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::TextColored(AssetStateColor(metadata.State), "%s", AssetStateToString(metadata.State));

				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(AssetTypeToString(metadata.Type));

				ImGui::TableSetColumnIndex(2);
				ImGui::TextUnformatted(path.c_str());
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("handle %llu\n%s", static_cast<unsigned long long>(static_cast<uint64_t>(metadata.Handle)),
						assets.ResolvePath(metadata.FilePath).string().c_str());

				ImGui::TableSetColumnIndex(3);
				const bool isLoaded = metadata.State == AssetState::Ready;
				const bool inFlight = metadata.State == AssetState::Queued || metadata.State == AssetState::Loading;
				// Reload destroys and recreates any type SupportsInPlaceReload rejects, which
				// would invalidate pointers games hold - do not offer it for those.
				const bool reloadBlocked = isLoaded && !AssetManager::SupportsInPlaceReload(metadata.Type);

				// No Unload button on purpose: unloading frees the object, and games
				// legitimately cache the pointers they were handed.
				ImGui::BeginDisabled(inFlight || reloadBlocked);
				if (ImGui::SmallButton(isLoaded ? "Reload" : "Load"))
				{
					action = isLoaded ? Action::Reload : Action::Load;
					target = metadata.Handle;
				}
				ImGui::EndDisabled();
				if (reloadBlocked && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
					ImGui::SetTooltip("%s is destroyed and recreated on reload, which would invalidate pointers the game holds - not offered here.", AssetTypeToString(metadata.Type));

				ImGui::PopID();
			}

			ImGui::EndTable();
		}

		if (ImGui::Button("Reload all loaded"))
		{
			for (const AssetMetadata& metadata : entries)
			{
				if (metadata.State == AssetState::Ready && AssetManager::SupportsInPlaceReload(metadata.Type))
					assets.Reload(metadata.Handle);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Retry failed"))
		{
			for (const AssetMetadata& metadata : entries)
			{
				if (metadata.State == AssetState::Unloaded || metadata.State == AssetState::Failed)
					assets.LoadAsync(metadata.FilePath);
			}
		}

		switch (action)
		{
			case Action::Reload: assets.Reload(target); break;
			case Action::Load:
			{
				if (const AssetMetadata* metadata = assets.GetMetadata(target))
					assets.LoadAsync(metadata->FilePath);
				break;
			}
			case Action::None: break;
		}
	}

	void AssetStatsWindow(bool* open)
	{
		if (!ImGui::Begin("Asset Stats", open))
		{
			ImGui::End();
			return;
		}

		AssetSummarySection();

		ImGui::Spacing();
		AssetRegistrySection();

		ImGui::End();
	}

	void InputStatsWindow(bool* open)
	{
		if (!ImGui::Begin("Input Stats", open))
		{
			ImGui::End();
			return;
		}

		MouseInputSection();

		ImGui::Spacing();
		KeyboardInputSection();

		ImGui::Spacing();
		GamepadInputSection();

		ImGui::End();
	}

	DebugTab DebugWindow(bool* open, DebugTab select)
	{
		DebugTab active = DebugTab::None;

		ImGui::SetNextWindowSize(ImVec2(560.0f, 560.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Debug", open))
		{
			ImGui::End();
			return active;
		}

		if (ImGui::BeginTabBar("##DebugTabs"))
		{
			auto tab = [&](const char* label, DebugTab id, auto&& content)
			{
				const ImGuiTabItemFlags flags = (select == id) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
				if (!ImGui::BeginTabItem(label, nullptr, flags))
					return;

				active = id;
				ImGui::Spacing();
				content();
				ImGui::EndTabItem();
			};

			tab("Engine", DebugTab::Engine, []
			{
				EngineInfoSection();
				ImGui::Spacing();
				GraphicsInfoSection();
				ImGui::Spacing();
				WindowInfoSection();
				ImGui::Spacing();
				FrameTimingSection();
				ImGui::Spacing();
				AudioStatsSection();
			});

			tab("Renderer", DebugTab::Renderer, []
			{
				RendererStatsSection();
			});

			tab("Input", DebugTab::Input, []
			{
				MouseInputSection();
				ImGui::Spacing();
				KeyboardInputSection();
				ImGui::Spacing();
				GamepadInputSection();
			});

			tab("Assets", DebugTab::Assets, []
			{
				AssetSummarySection();
				ImGui::Spacing();
				AssetRegistrySection();
			});

			ImGui::EndTabBar();
		}

		ImGui::End();
		return active;
	}

}
