#include "depch.h"

#include "DingoEngine/UI/DebugPanels.h"

#include "DingoEngine/Core/Application.h"
#include "DingoEngine/Core/Input.h"
#include "DingoEngine/Core/Profiler.h"
#include "DingoEngine/Graphics/Renderer2D.h"
#include "DingoEngine/Graphics/Renderer3D.h"
#include "DingoEngine/Graphics/PostProcess.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Windowing/Window.h"
#include "DingoEngine/Audio/AudioEngine.h"
#include "DingoEngine/Asset/AssetManager.h"
#include "DingoEngine/Graphics/Animator.h"
#include "DingoEngine/Scene/AnimationDebug.h"
#include "DingoEngine/Version.h"
#include "DingoEngine/BuildInfo.h"

#include <imgui.h>

#include <algorithm>
#include <array>
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
		// budget. ImGui tints the fill.
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
				case AssetState::Ready:     return ImVec4(0.35f, 0.85f, 0.40f, 1.0f);
				case AssetState::Queued:
				case AssetState::Loading:
				case AssetState::Reloading: return ImVec4(0.95f, 0.80f, 0.30f, 1.0f);
				case AssetState::Failed:    return ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
				case AssetState::Unloaded:
				default:                    return ImVec4(0.60f, 0.60f, 0.60f, 1.0f);
			}
		}

		// Registry snapshot ordered by path: the registry is an unordered_map, so
		// iterating it directly would reshuffle rows between frames.
		// Caches handles, not AssetMetadata pointers: Remove() erases from the registry
		// map, which invalidates that element's address, so resolving fresh via
		// GetMetadata() at point of use is what makes an erased handle merely fail to
		// resolve instead of reading freed memory.
		//
		// Rebuilds when stale, where stale = registered count changed OR some
		// previously-cached handle no longer resolves. The second half is load-bearing:
		// a same-frame-gap Remove(h)+Import(p) nets no count change, so the count check
		// alone would miss it, leaving both the dead handle AND the new import invisible
		// until some later change happened to move the count. Checking every cached
		// handle resolves costs one GetMetadata hash lookup per entry every frame (no
		// allocation) but not the O(n log n) collect-and-sort, which only runs when
		// something actually changed - that's the trade this cache exists to make.
		// What this does NOT catch: an asset's path changing without its handle
		// changing, which the current Import/Remove API has no way to do anyway.
		const std::vector<AssetHandle>& SortedRegistry(const AssetManager& assets)
		{
			static std::vector<AssetHandle> s_Sorted;
			static uint32_t s_LastCount = 0;

			const uint32_t count = assets.GetRegisteredCount();
			const bool stale = count != s_LastCount || std::any_of(s_Sorted.begin(), s_Sorted.end(),
				[&assets](AssetHandle handle) { return assets.GetMetadata(handle) == nullptr; });

			if (stale)
			{
				s_Sorted.clear();
				s_Sorted.reserve(count);
				for (const auto& [handle, metadata] : assets.GetRegistry())
					s_Sorted.push_back(handle);

				std::sort(s_Sorted.begin(), s_Sorted.end(), [&assets](AssetHandle a, AssetHandle b)
				{
					// Both handles were just read from this registry, so they always resolve.
					return assets.GetMetadata(a)->FilePath.native() < assets.GetMetadata(b)->FilePath.native();
				});

				s_LastCount = count;
			}

			return s_Sorted;
		}

		constexpr size_t k_MaxAnimatorsShown = 16;

		const char* ClipName(const AnimationClip& clip)
		{
			return clip.GetName().empty() ? "(unnamed)" : clip.GetName().c_str();
		}

		std::string StateLabel(const Animator& animator, const AnimatorStateInfo& state, uint32_t layer)
		{
			std::string label;
			if (state.Frozen)
			{
				label = "(frozen)";
			}
			else if (state.Blend)
			{
				label = std::format("Blend1D {} = {:.2f}", state.Parameter, animator.GetFloat(state.Parameter));
				if (state.Clip)
					label += std::format("  [{}]", ClipName(*state.Clip));
			}
			else if (state.Clip)
			{
				label = ClipName(*state.Clip);
			}
			else
			{
				label = layer == 0 ? "(rest pose)" : "(below)";
			}

			if (!state.Looping)
				label += "  (once)";
			return label;
		}

		const char* EventTypeName(AnimationEventType type)
		{
			switch (type)
			{
				case AnimationEventType::RangeBegin: return "begin";
				case AnimationEventType::RangeEnd:   return "end";
				case AnimationEventType::Instant:
				default:                             return "instant";
			}
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
		ImGui::Text("Draw calls : %u   (one or more per material)", stats3D.DrawCalls);
		ImGui::Text("Meshes     : %u submitted", stats3D.SubmittedMeshes);
		if (stats3D.DroppedMeshes > 0)
			ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
				"Dropped    : %u  (a mesh exceeds Renderer3D MaxVertices/MaxIndices on its own)", stats3D.DroppedMeshes);
		else
			ImGui::Text("Dropped    : 0");

		ImGui::Spacing();
		BudgetBar("Vertices", stats3D.VertexCount, caps3D.MaxVertices);
		BudgetBar("Indices", stats3D.IndexCount, caps3D.MaxIndices);

		ImGui::Spacing();
		ImGui::TextUnformatted("Renderer3D lights  (most recent scene)");
		ImGui::Separator();
		ImGui::Text("Directional: %u / %u", stats3D.DirectionalLights, Renderer3D::k_MaxDirectionalLights);
		BudgetBar("Point/spot", stats3D.LocalLights, renderer3D.GetLocalLightBudget());
		ImGui::Text("Out of view: %u  (range can't reach the screen)", stats3D.CulledLights);
		if (stats3D.DroppedLights > 0)
			ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
				"Dropped    : %u  (past a light limit; the log says which)", stats3D.DroppedLights);
		else
			ImGui::Text("Dropped    : 0");
		if (caps3D.LightBudgetFade > 0.0f)
			ImGui::Text("Faded      : %u  (budget fade band %.2f)", stats3D.FadedLights, caps3D.LightBudgetFade);

		ImGui::Spacing();
		ImGui::TextUnformatted("Renderer3D skinning  (most recent scene; budget per frame)");
		ImGui::Separator();
		ImGui::Text("Skinned draws: %u  (also counted in draw calls)", stats3D.SkinnedDraws);
		BudgetBar("Instances", stats3D.SkinnedInstances, renderer3D.GetSkinnedInstanceBudget());
		ImGui::Text("Joints     : %u uploaded", stats3D.SkinnedJoints);
		if (stats3D.DroppedSkinnedDraws > 0)
			ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
				"Dropped    : %u  (skinned draws of instances past MaxSkinnedInstances; warned once)", stats3D.DroppedSkinnedDraws);
		else
			ImGui::Text("Dropped    : 0");

		ImGui::Spacing();
		ImGui::TextUnformatted("Renderer3D shadows  (most recent scene)");
		ImGui::Separator();
		if (stats3D.ShadowViews == 0 && stats3D.UnshadowedLights == 0)
			ImGui::TextDisabled("None cast: no light with CastShadows, or nothing that casts.");
		else
		{
			ImGui::Text("Tiles      : %u rendered into the atlas", stats3D.ShadowViews);
			ImGui::Text("Cascades   : %u   ends at %.1f / %.1f / %.1f / %.1f along the view", stats3D.ShadowCascades,
				stats3D.ShadowCascadeEnds[0], stats3D.ShadowCascadeEnds[1], stats3D.ShadowCascadeEnds[2], stats3D.ShadowCascadeEnds[3]);
			BudgetBar("Shadowed", stats3D.ShadowedLights, std::min(caps3D.MaxShadowedLocalLights, Renderer3D::k_MaxShadowedLocalLights));
			if (stats3D.UnshadowedLights > 0)
				ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
					"Unshadowed : %u  (casting lights past MaxShadowedLocalLights or the atlas; warned once)", stats3D.UnshadowedLights);
			else
				ImGui::Text("Unshadowed : 0");
			ImGui::Text("Casters    : %u meshes in %u instanced draws (not in draw calls)", stats3D.ShadowCasters, stats3D.ShadowDrawCalls);
		}

		Renderer3D& mutableRenderer3D = Application::Get().GetRenderer3D();
		Renderer3DShadowSettings shadowSettings = mutableRenderer3D.GetShadowSettings();
		if (ImGui::Checkbox("Tint by cascade", &shadowSettings.DebugCascades))
			mutableRenderer3D.SetShadowSettings(shadowSettings);

		if (Framebuffer* atlas = mutableRenderer3D.GetShadowAtlas())
		{
			ImGui::Text("Atlas      : %u x %u D32, %.0f MB", atlas->GetWidth(), atlas->GetHeight(),
				static_cast<double>(atlas->GetWidth()) * atlas->GetHeight() * 4.0 / (1024.0 * 1024.0));
			if (Texture* depth = atlas->GetDepthAttachment())
			{
				static bool s_ShowAtlas = false;
				ImGui::Checkbox("Show the atlas (depth in red, near is dark)", &s_ShowAtlas);
				if (s_ShowAtlas)
					ImGui::Image(reinterpret_cast<ImTextureID>(depth->GetTextureHandle()), ImVec2(256.0f, 256.0f));
			}
		}

		const PostProcessStack::Statistics& post = Renderer::GetPostProcessStack().GetStatistics();
		ImGui::Spacing();
		ImGui::TextUnformatted("Post chain  (last frame that ran it)");
		ImGui::Separator();
		if (post.SceneTargets == 0)
			ImGui::TextDisabled("Not used: no PostProcessComponent or PostProcessStack::Begin has enabled it.");
		else
		{
			ImGui::Text("Scenes     : %u (%u bloomed)   Scene target %u x %u, RGBA16F + D32", post.Scenes, post.BloomScenes, post.Width, post.Height);
			ImGui::Text("Targets    : %u cached, %.1f MB", post.SceneTargets, static_cast<double>(post.TargetBytes) / (1024.0 * 1024.0));
		}
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

	void CursorInputSection()
	{
		const char* modeName = "Normal";
		switch (Input::GetCursorMode())
		{
			case CursorMode::Normal: modeName = "Normal"; break;
			case CursorMode::Hidden: modeName = "Hidden"; break;
			case CursorMode::Locked: modeName = "Locked"; break;
		}

		const glm::vec2 delta = Input::GetMouseDelta();

		ImGui::TextUnformatted("Cursor");
		ImGui::Separator();
		ImGui::Text("Mode        : %s", modeName);
		ImGui::Text("Focused     : %s", Application::Get().GetWindow().IsFocused() ? "yes" : "no");
		ImGui::Text("Raw motion  : %s  (supported: %s)",
			Input::IsRawMouseMotionEnabled() ? "enabled" : "disabled",
			Input::IsRawMouseMotionSupported() ? "yes" : "no");
		ImGui::Text("Delta       : %+.1f, %+.1f", delta.x, delta.y);
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

			ImGui::PushID(static_cast<int>(pad));
			if (Input::IsGamepadRumbleSupported(pad))
			{
				if (ImGui::Button("Rumble: low"))
					Input::SetGamepadRumble(0.8f, 0.0f, 0.5f, pad);
				ImGui::SameLine();
				if (ImGui::Button("Rumble: high"))
					Input::SetGamepadRumble(0.0f, 0.8f, 0.5f, pad);
			}
			else
			{
				ImGui::TextDisabled("No rumble (XInput pads only)");
			}
			ImGui::PopID();
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

		std::array<uint32_t, static_cast<size_t>(AssetType::Count)> perType{};
		std::array<uint32_t, static_cast<size_t>(AssetState::Count)> perState{};
		for (const auto& [handle, metadata] : assets.GetRegistry())
		{
			perType[static_cast<size_t>(metadata.Type)]++;
			perState[static_cast<size_t>(metadata.State)]++;
		}

		ImGui::Spacing();
		ImGui::TextUnformatted("By type");
		ImGui::Separator();
		for (size_t type = 1; type < perType.size(); type++)
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
		for (size_t state = 0; state < perState.size(); state++)
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
		if (ImGui::Checkbox("Hot-reload changed files", &hotReload))
			assets.SetHotReloadEnabled(hotReload);

		static char s_Filter[128] = "";
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##assetfilter", "filter by path...", s_Filter, IM_ARRAYSIZE(s_Filter));

		// Buttons act on the manager, which mutates the registry - collect the request
		// and apply it after the table is closed.
		enum class Action { None, Load, Reload };
		Action action = Action::None;
		AssetHandle target = k_InvalidAsset;

		const std::vector<AssetHandle>& entries = SortedRegistry(assets);

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

			for (const AssetHandle handle : entries)
			{
				const AssetMetadata* metadata = assets.GetMetadata(handle);
				if (!metadata)
					continue; // removed since the snapshot was built - skip rather than show stale data

				const std::string path = metadata->FilePath.generic_string();
				if (s_Filter[0] != '\0' && path.find(s_Filter) == std::string::npos)
					continue;

				ImGui::PushID(static_cast<int>(static_cast<uint64_t>(handle) & 0x7FFFFFFF));
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::TextColored(AssetStateColor(metadata->State), "%s", AssetStateToString(metadata->State));

				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(AssetTypeToString(metadata->Type));

				ImGui::TableSetColumnIndex(2);
				ImGui::TextUnformatted(path.c_str());
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("handle %llu\n%s", static_cast<unsigned long long>(static_cast<uint64_t>(handle)),
						assets.ResolvePath(metadata->FilePath).string().c_str());

				ImGui::TableSetColumnIndex(3);
				// Reloading keeps the object alive and usable (that's the point of an
				// in-place hot-reload) with a new version decoding in the background, so it
				// counts as loaded for the button label and as in-flight for disabling it.
				const bool isLoaded = metadata->State == AssetState::Ready || metadata->State == AssetState::Reloading;
				const bool inFlight = metadata->State == AssetState::Queued || metadata->State == AssetState::Loading
					|| metadata->State == AssetState::Reloading;
				// Reload destroys and recreates any type SupportsInPlaceReload rejects, which
				// would invalidate pointers games hold - do not offer it for those.
				const bool reloadBlocked = isLoaded && !AssetManager::SupportsInPlaceReload(metadata->Type);

				// No Unload button on purpose: unloading frees the object, and games
				// legitimately cache the pointers they were handed.
				ImGui::BeginDisabled(inFlight || reloadBlocked);
				if (ImGui::SmallButton(isLoaded ? "Reload" : "Load"))
				{
					action = isLoaded ? Action::Reload : Action::Load;
					target = handle;
				}
				ImGui::EndDisabled();
				if (reloadBlocked && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
					ImGui::SetTooltip("%s is destroyed and recreated on reload, which would invalidate pointers the game holds - not offered here.", AssetTypeToString(metadata->Type));

				ImGui::PopID();
			}

			ImGui::EndTable();
		}

		if (ImGui::Button("Reload all loaded"))
		{
			for (const AssetHandle handle : entries)
			{
				const AssetMetadata* metadata = assets.GetMetadata(handle);
				// Ready only, deliberately excluding Reloading: it already has a reload in
				// flight, so re-triggering one here would just queue a redundant decode.
				if (metadata && metadata->State == AssetState::Ready && AssetManager::SupportsInPlaceReload(metadata->Type))
					assets.Reload(handle);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Retry failed"))
		{
			for (const AssetHandle handle : entries)
			{
				const AssetMetadata* metadata = assets.GetMetadata(handle);
				if (metadata && (metadata->State == AssetState::Unloaded || metadata->State == AssetState::Failed))
					assets.LoadAsync(metadata->FilePath);
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

	void AnimationSection()
	{
		static std::vector<Internal::AnimationDebug::AnimatorRow> s_Animators;
		static std::vector<Internal::AnimationDebug::EventRow> s_Events;
		Internal::AnimationDebug::CollectAnimators(s_Animators);
		Internal::AnimationDebug::CollectRecentEvents(s_Events, Internal::AnimationDebug::k_RecentEvents);

		ImGui::TextUnformatted(std::format("Animators ({})", s_Animators.size()).c_str());
		ImGui::Separator();
		if (s_Animators.empty())
			ImGui::TextDisabled("No scene has an AnimatorComponent with a skinned model.");

		// A crowd scrolls in its own region, so the events below stay in view.
		const bool scroll = s_Animators.size() > k_MaxAnimatorsShown;
		if (scroll)
			ImGui::BeginChild("##animators", ImVec2(0.0f, 320.0f), ImGuiChildFlags_Borders);
		for (size_t index = 0; index < s_Animators.size(); ++index)
		{
			const Internal::AnimationDebug::AnimatorRow& row = s_Animators[index];
			std::string label = std::format("{}  ({}{}{})", row.Entity, row.Scene, row.Model.empty() ? "" : ", ", row.Model);
			if (!row.Instance)
			{
				ImGui::TextDisabled("%s  not bound: no skinned model, or not updated since it changed", label.c_str());
				continue;
			}

			const Animator& animator = *row.Instance;
			if (!row.Enabled)
				label += "  disabled";
			if (row.Speed != 1.0f)
				label += std::format("  speed {:.2f}", row.Speed);
			label += "###animator";

			ImGui::PushID(static_cast<int>(index));
			if (ImGui::TreeNodeEx(label.c_str(), s_Animators.size() == 1 ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None))
			{
				for (uint32_t layer = 0; layer < animator.GetLayerCount(); ++layer)
				{
					const AnimationLayer& settings = animator.GetLayer(layer);
					const std::string& mask = settings.GetMaskRoot();

					std::string line = std::format("Layer {}  weight {:.2f}  ", layer, settings.GetWeight());
					line += mask.empty() ? "whole body" : "mask " + mask;
					if (animator.IsOneShotPlaying(layer))
						line += "  one-shot";
					ImGui::TextUnformatted(line.c_str());

					const std::vector<AnimatorStateInfo> states = animator.GetStates(layer);
					if (states.empty())
					{
						ImGui::TextDisabled(layer == 0 ? "  nothing playing: the rest pose" : "  nothing playing: the layers below show");
						continue;
					}

					ImGui::PushID(static_cast<int>(layer));
					if (ImGui::BeginTable("##states", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
					{
						ImGui::TableSetupColumn("State");
						ImGui::TableSetupColumn("Fade", ImGuiTableColumnFlags_WidthFixed, 90.0f);
						ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 130.0f);
						ImGui::TableHeadersRow();

						for (const AnimatorStateInfo& state : states)
						{
							ImGui::TableNextRow();

							ImGui::TableSetColumnIndex(0);
							ImGui::TextUnformatted(StateLabel(animator, state, layer).c_str());

							ImGui::TableSetColumnIndex(1);
							ImGui::ProgressBar(state.Weight, ImVec2(-1.0f, 0.0f), std::format("{:.2f}", state.Weight).c_str());

							ImGui::TableSetColumnIndex(2);
							if (state.Blend)
								ImGui::ProgressBar(state.NormalizedTime, ImVec2(-1.0f, 0.0f), std::format("phase {:.2f}", state.NormalizedTime).c_str());
							else if (state.Clip && !state.Frozen)
								ImGui::ProgressBar(state.NormalizedTime, ImVec2(-1.0f, 0.0f), std::format("{:.2f} / {:.2f}s", state.Time, state.Clip->GetDuration()).c_str());
							else
								ImGui::TextDisabled("-");
						}

						ImGui::EndTable();
					}
					ImGui::PopID();
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		if (scroll)
			ImGui::EndChild();

		ImGui::Spacing();
		ImGui::TextUnformatted("Recent events");
		ImGui::Separator();
		if (s_Events.empty())
		{
			ImGui::TextDisabled("No animation events yet.");
			return;
		}

		if (ImGui::BeginTable("##animevents", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("Entity");
			ImGui::TableSetupColumn("Clip");
			ImGui::TableSetupColumn("Event");
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 60.0f);
			ImGui::TableSetupColumn("Layer", ImGuiTableColumnFlags_WidthFixed, 40.0f);
			ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 64.0f);
			ImGui::TableHeadersRow();

			for (auto it = s_Events.rbegin(); it != s_Events.rend(); ++it)
			{
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(it->Entity.c_str());
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(it->Clip.c_str());
				ImGui::TableSetColumnIndex(2);
				ImGui::TextUnformatted(it->Name.data(), it->Name.data() + it->Name.size());
				ImGui::TableSetColumnIndex(3);
				ImGui::TextUnformatted(EventTypeName(it->Type));
				ImGui::TableSetColumnIndex(4);
				ImGui::Text("%u", it->Layer);
				ImGui::TableSetColumnIndex(5);
				ImGui::Text("@%.3f", it->Time);
			}

			ImGui::EndTable();
		}
	}

	void AnimationStatsWindow(bool* open)
	{
		if (!ImGui::Begin("Animation Stats", open))
		{
			ImGui::End();
			return;
		}

		AnimationSection();

		ImGui::End();
	}

	void ProfilerSection()
	{
		ImGui::TextUnformatted("Tracy");
		ImGui::Separator();
		if (!Profiler::IsCompiledIn())
			ImGui::TextDisabled("Not compiled in: regenerate with premake --profile to record zones.");
		else if (Profiler::IsConnected())
			ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.40f, 1.0f), "Compiled in, viewer connected: recording.");
		else
			ImGui::Text("Compiled in, waiting for the Tracy viewer to connect.");

		// One sample a frame while this tab is drawn.
		constexpr int k_History = 120;
		constexpr int k_Rows = 5;
		static float s_History[k_Rows][k_History] = {};
		static int s_Cursor = 0;
		static int s_Count = 0;

		const FrameTimings& frame = Application::Get().GetFrameTimings();
		const float samples[k_Rows] = { frame.FrameMs, frame.WaitMs, frame.UpdateMs, frame.UIMs, Renderer::GetRenderThreadMilliseconds() };
		for (int row = 0; row < k_Rows; ++row)
			s_History[row][s_Cursor] = samples[row];
		s_Cursor = (s_Cursor + 1) % k_History;
		s_Count = std::min(s_Count + 1, k_History);

		auto timingTable = [](const char* id, const char* firstColumn, auto&& rows)
		{
			if (!ImGui::BeginTable(id, 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp))
				return;
			ImGui::TableSetupColumn(firstColumn);
			ImGui::TableSetupColumn("Last ms", ImGuiTableColumnFlags_WidthFixed, 70.0f);
			ImGui::TableSetupColumn("Mean ms", ImGuiTableColumnFlags_WidthFixed, 70.0f);
			ImGui::TableSetupColumn("Max ms", ImGuiTableColumnFlags_WidthFixed, 70.0f);
			ImGui::TableHeadersRow();
			rows();
			ImGui::EndTable();
		};
		auto timingRow = [](const char* name, int indent, float last, float mean, float max)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%*s%s", indent * 2, "", name);
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%.3f", last);
			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%.3f", mean);
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%.3f", max);
		};

		ImGui::Spacing();
		ImGui::TextUnformatted("CPU  (last 120 frames while this tab is open)");
		ImGui::Separator();
		static constexpr const char* s_RowNames[k_Rows] = { "Frame", "Wait for render thread", "Update", "UI", "Render thread (execute + present)" };
		static constexpr int s_RowIndent[k_Rows] = { 0, 1, 1, 1, 0 };
		timingTable("##cputimes", "Main thread", [&]
		{
			for (int row = 0; row < k_Rows; ++row)
			{
				float sum = 0.0f, max = 0.0f;
				for (int i = 0; i < s_Count; ++i)
				{
					sum += s_History[row][i];
					max = std::max(max, s_History[row][i]);
				}
				timingRow(s_RowNames[row], s_RowIndent[row], samples[row], s_Count > 0 ? sum / static_cast<float>(s_Count) : 0.0f, max);
			}
		});

		ImGui::Spacing();
		ImGui::TextUnformatted("GPU passes  (timer queries, read 4 frames late)");
		ImGui::Separator();
		const std::vector<GpuTimerStats>& timers = Renderer::GetGpuTimers();
		if (timers.empty())
		{
			ImGui::TextDisabled("No GPU timer has reported yet.");
			return;
		}

		timingTable("##gputimes", "Pass", [&]
		{
			for (const GpuTimerStats& timer : timers)
			{
				if (timer.Samples > 0)
					timingRow(timer.Name, static_cast<int>(timer.Depth), timer.LastMs, timer.MeanMs, timer.MaxMs);
			}
		});
	}

	void ProfilerStatsWindow(bool* open)
	{
		if (!ImGui::Begin("Profiler", open))
		{
			ImGui::End();
			return;
		}

		ProfilerSection();

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
		CursorInputSection();

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
				CursorInputSection();
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

			tab("Animation", DebugTab::Animation, [] { AnimationSection(); });

			tab("Profiler", DebugTab::Profiler, [] { ProfilerSection(); });

			ImGui::EndTabBar();
		}

		ImGui::End();
		return active;
	}

}
