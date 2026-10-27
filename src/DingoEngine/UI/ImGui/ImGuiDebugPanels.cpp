#include "depch.h"

#include "DingoEngine/UI/DebugPanels.h"
#include "DingoEngine/Core/PathText.h"
#include "DingoEngine/Core/LaunchFlags.h"

#include "DingoEngine/Core/Application.h"
#include "DingoEngine/Core/Input.h"
#include "DingoEngine/Core/Profiler.h"
#include "DingoEngine/Graphics/Renderer2D.h"
#include "DingoEngine/Graphics/Renderer3D.h"
#include "DingoEngine/Graphics/PostProcess.h"
#include "DingoEngine/Graphics/ParticleRenderer.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Windowing/Window.h"
#include "DingoEngine/Audio/AudioEngine.h"
#include "DingoEngine/Asset/AssetManager.h"
#include "DingoEngine/Graphics/Animator.h"
#include "DingoEngine/Scene/AnimationDebug.h"
#include "DingoEngine/Scene/SceneDebug.h"
#include "DingoEngine/UI/DingoUI/DingoUIDebug.h"
#include "DingoEngine/Version.h"
#include "DingoEngine/BuildInfo.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <format>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

// ImGui-backed implementation of the engine's renderer debug panels
// (DingoEngine/UI/DebugPanels.h). Like ImGuiUI.cpp, this is one of the only
// translation units that includes <imgui.h>; clients call only Dingo::UI.

namespace Dingo::Internal
{

	namespace
	{
		// D7: effects are authored in code, so the editor changes the live effect and prints the code
		// that builds what is on screen.
		void ParticleEffectEditor()
		{
			const std::vector<ParticleEffect*>& effects = Internal::GetLiveParticleEffects();
			if (!ImGui::CollapsingHeader("Particle effect editor"))
				return;
			if (effects.empty())
			{
				ImGui::TextDisabled("No ParticleEffect exists.");
				return;
			}

			static const ParticleEffect* s_Selected = nullptr;
			if (std::find(effects.begin(), effects.end(), s_Selected) == effects.end())
				s_Selected = effects.front();

			auto nameOf = [&effects](const ParticleEffect* effect)
			{
				const std::string& name = effect->GetParams().DebugName;
				return name.empty() ? std::format("Effect {}", std::find(effects.begin(), effects.end(), effect) - effects.begin()) : name;
			};
			if (ImGui::BeginCombo("Effect", nameOf(s_Selected).c_str()))
			{
				for (const ParticleEffect* effect : effects)
				{
					ImGui::PushID(effect);
					if (ImGui::Selectable(nameOf(effect).c_str(), effect == s_Selected))
						s_Selected = effect;
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}

			ParticleEffect* effect = *std::find(effects.begin(), effects.end(), s_Selected);
			ParticleEffectParams params = effect->GetParams();
			bool changed = false;

			int shape = static_cast<int>(params.Shape);
			changed |= ImGui::Combo("Shape", &shape, "Point\0Sphere\0Cone\0Box\0");
			params.Shape = static_cast<ParticleShape>(shape);
			changed |= ImGui::DragFloat3("Shape size", &params.ShapeSize.x, 0.01f, 0.0f, 100.0f);
			changed |= ImGui::DragFloat("Rate", &params.Rate, 1.0f, 0.0f, 100000.0f);
			int burst = static_cast<int>(params.BurstOnPlay);
			changed |= ImGui::DragInt("Burst on play", &burst, 1.0f, 0, 100000);
			params.BurstOnPlay = static_cast<uint32_t>(std::max(burst, 0));
			changed |= ImGui::DragFloat2("Lifetime", &params.Lifetime.x, 0.01f, 0.01f, 60.0f);
			changed |= ImGui::DragFloat2("Speed", &params.Speed.x, 0.01f, 0.0f, 100.0f);
			changed |= ImGui::DragFloat3("Gravity", &params.Gravity.x, 0.05f, -100.0f, 100.0f);
			changed |= ImGui::DragFloat("Drag", &params.Drag, 0.01f, 0.0f, 20.0f);
			changed |= ImGui::DragFloat("Inherit velocity", &params.InheritVelocity, 0.01f, 0.0f, 1.0f);
			changed |= ImGui::DragFloat("Noise strength", &params.NoiseStrength, 0.01f, 0.0f, 50.0f);
			changed |= ImGui::DragFloat("Noise scale", &params.NoiseScale, 0.01f, 0.01f, 20.0f);
			changed |= ImGui::DragFloat2("Start size", &params.StartSize.x, 0.005f, 0.0f, 20.0f);
			changed |= ImGui::DragFloat("End size", &params.EndSize, 0.01f, 0.0f, 20.0f);
			changed |= ImGui::DragFloat2("Start rotation (deg)", &params.StartRotation.x, 1.0f, -360.0f, 360.0f);
			changed |= ImGui::DragFloat2("Spin (deg/s)", &params.Spin.x, 1.0f, -3600.0f, 3600.0f);
			int keys = static_cast<int>(params.ColorKeyCount);
			changed |= ImGui::SliderInt("Colour keys", &keys, 1, static_cast<int>(ParticleEffectParams::k_MaxColorKeys));
			params.ColorKeyCount = static_cast<uint32_t>(keys);
			for (int i = 0; i < keys; ++i)
			{
				ImGui::PushID(i);
				changed |= ImGui::DragFloat("Time", &params.ColorKeys[i].Time, 0.01f, 0.0f, 1.0f);
				changed |= ImGui::ColorEdit4("Colour", &params.ColorKeys[i].Color.x, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
				ImGui::PopID();
			}
			int blend = static_cast<int>(params.Blend);
			changed |= ImGui::Combo("Blend", &blend, "Additive\0Alpha\0");
			params.Blend = static_cast<ParticleBlend>(blend);
			changed |= ImGui::DragFloat("Soft distance", &params.SoftDistance, 0.01f, 0.0f, 10.0f);
			ImGui::Text("Capacity %u per emitter (fixed when an emitter is made)", effect->GetEmitterCapacity());

			if (changed)
				effect->SetParams(params);

			if (ImGui::Button("Copy as code"))
			{
				const std::string code = Internal::ParticleEffectToCode(effect->GetParams());
				ImGui::SetClipboardText(code.c_str());
				DE_CORE_INFO("Particle effect '{}':\n{}", effect->GetParams().DebugName, code);
			}
			ImGui::SameLine();
			ImGui::TextDisabled("(and logs it)");
		}

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

		// Resolving a source walks the mounts and stats loose files, so the column refreshes once a
		// second rather than per row per frame.
		const std::string& CachedAssetSource(const AssetManager& assets, AssetHandle handle)
		{
			static std::unordered_map<AssetHandle, std::string> s_Sources;
			static double s_LastRefresh = -1.0;

			const double now = ImGui::GetTime();
			if (now - s_LastRefresh > 1.0)
			{
				s_Sources.clear();
				s_LastRefresh = now;
			}

			auto it = s_Sources.find(handle);
			if (it == s_Sources.end())
				it = s_Sources.emplace(handle, assets.GetAssetSource(handle)).first;
			return it->second;
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
				s_Sorted = assets.GetHandles();

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
		Renderer2D& renderer2D = Application::Get().GetRenderer2D();
		const Renderer2D::Statistics& stats2D = renderer2D.GetStatistics();

		ImGui::Spacing();
		ImGui::TextUnformatted("Renderer2D  (most recent scene)");
		ImGui::Separator();
		ImGui::Text("Draw calls : %u", stats2D.DrawCalls);
		ImGui::Text("Quads      : %u", stats2D.QuadCount);
		ImGui::Text("Circles    : %u", stats2D.CircleCount);
		ImGui::Text("Text quads : %u", stats2D.TextQuadCount);
		ImGui::Text("Vertices   : %u    Indices : %u", stats2D.GetVertexCount(), stats2D.GetIndexCount());
		ImGui::Text("Culled     : %u  (sprites and circles out of view, by Scene::RenderEntities)", stats2D.CulledQuads);
		bool culling2D = renderer2D.IsCullingEnabled();
		if (ImGui::Checkbox("Cull 2D entities", &culling2D))
			renderer2D.SetCullingEnabled(culling2D);

		// ---- Renderer3D -----------------------------------------------------
		Renderer3D& renderer3D = Application::Get().GetRenderer3D();
		const Renderer3D::Statistics& stats3D = renderer3D.GetStatistics();
		const Renderer3DCapabilities& caps3D = renderer3D.GetCapabilities();

		ImGui::Spacing();
		ImGui::TextUnformatted("Renderer3D  (most recent scene)");
		ImGui::Separator();
		ImGui::Text("Draw calls : %u   (one or more per material)", stats3D.DrawCalls);
		ImGui::Text("Translucent: %u meshes in %u draws  (sorted far to near, also counted above)", stats3D.TranslucentMeshes, stats3D.TranslucentDraws);
		ImGui::Text("Surfaces   : %u meshes with a SurfaceOverride", stats3D.SurfaceOverrides);
		ImGui::Text("Meshes     : %u submitted", stats3D.SubmittedMeshes);
		if (stats3D.DroppedMeshes > 0)
			ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
				"Dropped    : %u  (a mesh exceeds Renderer3D MaxVertices/MaxIndices on its own)", stats3D.DroppedMeshes);
		else
			ImGui::Text("Dropped    : 0");
		ImGui::Text("Culled     : %u static, %u skinned out of view or past their cull distance", stats3D.CulledMeshes, stats3D.CulledSkinned);
		ImGui::Text("             %u casters reach no shadow view", stats3D.CulledCasters);
		bool culling3D = renderer3D.IsCullingEnabled();
		if (ImGui::Checkbox("Cull meshes", &culling3D))
			renderer3D.SetCullingEnabled(culling3D);
		ImGui::SameLine();
		bool frozen = renderer3D.IsCullingFrozen();
		if (ImGui::Checkbox("Freeze culling frustum", &frozen))
			renderer3D.SetCullingFrozen(frozen);
		ImGui::Text("Uploaded   : %u vertices, %u indices  (one draw takes at most %u / %u)", stats3D.VertexCount, stats3D.IndexCount, caps3D.MaxVertices, caps3D.MaxIndices);
		ImGui::Text("Instanced  : %u meshes in %u draws, %u shadow draws  (also counted above)", stats3D.InstancedMeshes, stats3D.InstancedDraws, stats3D.InstancedShadowDraws);
		if (stats3D.DroppedInstancedMeshes > 0)
			ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
				"             %u meshes of an Instancing material skipped  (past the frame's instanced draws; warned once)", stats3D.DroppedInstancedMeshes);
		bool instancing = renderer3D.IsInstancingEnabled();
		if (ImGui::Checkbox("Instance repeated meshes", &instancing))
			renderer3D.SetInstancingEnabled(instancing);
		ImGui::Text("Static     : %u baked meshes in %u draws, %u cells baked  (also counted above)", stats3D.StaticMeshes, stats3D.StaticDraws, stats3D.StaticCellsBaked);

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
		ImGui::Text("Fog        : %s", stats3D.Fogged ? "on" : "off");

		ImGui::Spacing();
		ImGui::TextUnformatted("Renderer3D skinning  (most recent scene; the bar is the whole frame)");
		ImGui::Separator();
		ImGui::Text("Skinned draws: %u  (also counted in draw calls)", stats3D.SkinnedDraws);
		BudgetBar("Instances", renderer3D.GetSkinnedInstancesThisFrame(), renderer3D.GetSkinnedInstanceBudget());
		ImGui::Text("Scene      : %u instances, %u joints, %.1f KiB arena", stats3D.SkinnedInstances, stats3D.SkinnedJoints, stats3D.SkinPaletteBytes / 1024.0f);
		if (stats3D.DroppedSkinnedDraws > 0)
			ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
				"Dropped    : %u  (skinned draws of instances past MaxSkinnedInstances, or past %u with a SkinData shader; warned once)", stats3D.DroppedSkinnedDraws, Internal::k_MaxSkinDataInstances);
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

		ImGui::Spacing();
		ImGui::TextUnformatted("Renderer3D particles  (most recent scene)");
		ImGui::Separator();
		BudgetBar("Pool", renderer3D.GetParticlePoolUsed(), renderer3D.GetParticlePoolCapacity());
		if (stats3D.ParticleEmitters == 0)
			ImGui::TextDisabled("No emitter drawn this scene.");
		else
		{
			ImGui::Text("Emitters   : %u in %u draws, %u slots simulated", stats3D.ParticleEmitters, stats3D.ParticleDrawCalls, stats3D.ParticleSlots);
			ImGui::Text("Spawned    : %u", stats3D.ParticlesSpawned);
		}
		if (stats3D.DroppedParticleSpawns > 0)
			ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
				"Dropped    : %u spawns  (an emitter's ring was full; warned once)", stats3D.DroppedParticleSpawns);

		ParticleEffectEditor();

		const PostProcessStack::Statistics& post = Renderer::GetPostProcessStack().GetStatistics();
		ImGui::Spacing();
		ImGui::TextUnformatted("Post chain  (last frame that ran it)");
		ImGui::Separator();
		if (post.SceneTargets == 0)
			ImGui::TextDisabled("Not used: no PostProcessComponent or PostProcessStack::Begin has enabled it.");
		else
		{
			ImGui::Text("Scenes     : %u (%u bloomed, %u with AO)   Scene target %u x %u, RGBA16F + D32", post.Scenes, post.BloomScenes, post.AmbientOcclusionScenes, post.Width, post.Height);
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
			case GraphicsAPI::Vulkan:     apiName = "Vulkan";    break;
			case GraphicsAPI::DirectX11:  apiName = "DirectX11"; break;
			case GraphicsAPI::DirectX12:  apiName = "DirectX12"; break;
			case GraphicsAPI::Headless:   apiName = "Headless";  break;
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
		Window& window = Application::Get().GetWindow();

		ImGui::TextUnformatted("Window");
		ImGui::Separator();
		ImGui::Text("Size : %d x %d", window.GetWidth(), window.GetHeight());

		bool vsync = window.IsVSync();
		if (ImGui::Checkbox("VSync", &vsync))
			window.SetVSync(vsync);
		if (Internal::IsLaunchFlagOn(Application::Get().GetCommandLineArgs().Get("no-vsync")))
		{
			ImGui::SameLine();
			ImGui::TextDisabled("(off: --no-vsync)");
		}
	}

	void FrameTimingSection()
	{
		ImGui::TextUnformatted("Frame Timing");
		ImGui::Separator();
		FpsLine();
	}

	// Enumerating asks the OS, so the list is read when the node opens or on Refresh.
	void CaptureSection()
	{
		AudioEngine& audio = Application::Get().GetAudioEngine();
		const AudioCaptureState state = audio.GetCaptureState();
		const char* stateName = state == AudioCaptureState::Running ? "Running" : state == AudioCaptureState::Lost ? "Lost" : "Stopped";
		ImGui::Text("Capture       : %s", stateName);
		if (state == AudioCaptureState::Stopped)
			return;

		ImGui::Text("Queued frames : %u (overruns %llu)", audio.GetCapturedFrames(), static_cast<unsigned long long>(audio.GetCaptureOverruns()));
		ImGui::ProgressBar(audio.GetCaptureLevel(), ImVec2(-FLT_MIN, 0.0f), audio.IsCaptureGateOpen() ? "gate open" : "gate closed");

		static std::vector<AudioCaptureDevice> s_Devices;
		static bool s_Listed = false;
		const bool open = ImGui::TreeNode("Capture devices");
		if (!open)
		{
			s_Devices.clear();
			s_Listed = false;
			return;
		}
		if (!s_Listed || ImGui::SmallButton("Refresh"))
		{
			s_Devices = audio.EnumerateCaptureDevices();
			s_Listed = true;
		}
		const std::string current = audio.GetCaptureDeviceId();
		for (const AudioCaptureDevice& device : s_Devices)
		{
			ImGui::PushID(device.Id.c_str());
			const std::string label = device.IsDefault ? device.Name + " (default)" : device.Name;
			if (ImGui::Selectable(label.c_str(), device.Id == current) && device.Id != current)
				audio.SetCaptureDevice(device.Id);
			ImGui::PopID();
		}
		ImGui::TreePop();
	}

	void AudioStatsSection()
	{
		const AudioEngine& audio = Application::Get().GetAudioEngine();

		ImGui::TextUnformatted("Audio");
		ImGui::Separator();
		ImGui::Text("Status        : %s", audio.IsValid() ? "Valid" : "Invalid");
		ImGui::Text("Master volume : %.2f", audio.GetMasterVolume());
		ImGui::Text("Active sounds : %u", audio.GetActiveSoundCount());
		ImGui::Text("Buses         : %u", audio.GetBusCount());
		CaptureSection();
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
		for (uint32_t pad = 0; pad < k_MaxGamepads; pad++)
		{
			if (!Input::IsGamepadConnected(pad))
				continue;
			any = true;

			ImGui::Spacing();
			ImGui::Text("Slot %u : %s ('%s')", pad, ToString(Input::GetGamepadType(pad)), Input::GetGamepadName(pad).c_str());

			std::string held;
			for (uint8_t button = 0; button < k_GamepadButtonCount; button++)
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
		ImGui::TextWrapped("Root : %s", Internal::PathToUtf8(assets.GetRootDirectory()).c_str());
		ImGui::Text("Registered : %u    Loaded : %u    In flight : %u", registered, loaded, pending);

		const float fraction = registered > 0 ? static_cast<float>(loaded) / static_cast<float>(registered) : 0.0f;
		const std::string overlay = pending > 0
			? std::format("loading... {} / {}", loaded, registered)
			: std::format("{} / {}", loaded, registered);
		ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f), overlay.c_str());

		std::array<uint32_t, static_cast<size_t>(AssetType::Count)> perType{};
		std::array<uint32_t, static_cast<size_t>(AssetState::Count)> perState{};
		for (const AssetHandle handle : assets.GetHandles())
		{
			const AssetMetadata* metadata = assets.GetMetadata(handle);
			perType[static_cast<size_t>(metadata->Type)]++;
			perState[static_cast<size_t>(metadata->State)]++;
		}

		ImGui::Spacing();
		ImGui::TextUnformatted("By type");
		ImGui::Separator();
		for (size_t type = 1; type < perType.size(); type++)
		{
			if (perType[type] == 0)
				continue;
			ImGui::Text("%-10s : %u", ToString(static_cast<AssetType>(type)), perType[type]);
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
			ImGui::TextColored(AssetStateColor(value), "%-10s : %u", ToString(value), perState[state]);
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

		if (ImGui::BeginTable("##assettable", 5,
			ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
			ImVec2(0.0f, 240.0f)))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 62.0f);
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 74.0f);
			ImGui::TableSetupColumn("Path");
			ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 96.0f);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 118.0f);
			ImGui::TableHeadersRow();

			for (const AssetHandle handle : entries)
			{
				const AssetMetadata* metadata = assets.GetMetadata(handle);
				if (!metadata)
					continue; // removed since the snapshot was built - skip rather than show stale data

				const std::string path = Internal::PathToGenericUtf8(metadata->FilePath);
				if (s_Filter[0] != '\0' && path.find(s_Filter) == std::string::npos)
					continue;

				ImGui::PushID(static_cast<int>(static_cast<uint64_t>(handle) & 0x7FFFFFFF));
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::TextColored(AssetStateColor(metadata->State), "%s", ToString(metadata->State));

				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(ToString(metadata->Type));

				ImGui::TableSetColumnIndex(2);
				ImGui::TextUnformatted(path.c_str());
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("handle %llu\n%s", static_cast<unsigned long long>(static_cast<uint64_t>(handle)),
						Internal::PathToGenericUtf8(assets.ResolvePath(metadata->FilePath)).c_str());

				ImGui::TableSetColumnIndex(3);
				const std::string& source = CachedAssetSource(assets, handle);
				ImGui::TextDisabled("%s", source.c_str());

				ImGui::TableSetColumnIndex(4);
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
					ImGui::SetTooltip("%s is destroyed and recreated on reload, which would invalidate pointers the game holds - not offered here.", ToString(metadata->Type));

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
		const std::unique_lock scenesLock = Internal::SceneDebug::LockScenes();
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
		// A closed row off screen takes its height without its names being read or formatted.
		static float s_ClosedRowHeight = 0.0f;
		const bool defaultOpen = s_Animators.size() == 1;
		for (const Internal::AnimationDebug::AnimatorRow& row : s_Animators)
		{
			// Keyed on the entity, so an open row stays open while others join or leave the crowd.
			ImGui::PushID(row.Owner);
			ImGui::PushID(static_cast<int>(row.Entity));
			const bool open = row.Instance && ImGui::GetStateStorage()->GetInt(ImGui::GetID("###animator"), defaultOpen ? 1 : 0) != 0;
			if (!open && s_ClosedRowHeight > 0.0f && !ImGui::IsRectVisible(ImVec2(ImGui::GetContentRegionAvail().x, s_ClosedRowHeight)))
			{
				ImGui::Dummy(ImVec2(0.0f, s_ClosedRowHeight - ImGui::GetStyle().ItemSpacing.y));
				ImGui::PopID();
				ImGui::PopID();
				continue;
			}

			const float top = ImGui::GetCursorPosY();
			const Internal::AnimationDebug::AnimatorNames names = Internal::AnimationDebug::DescribeAnimator(row);
			std::string label = std::format("{}  ({}{}{})", names.Entity, names.Scene, names.Model.empty() ? "" : ", ", names.Model);
			if (!row.Instance)
			{
				ImGui::TextDisabled("%s  not bound: no skinned model, or not updated since it changed", label.c_str());
				s_ClosedRowHeight = ImGui::GetCursorPosY() - top;
				ImGui::PopID();
				ImGui::PopID();
				continue;
			}

			const Animator& animator = *row.Instance;
			if (!row.Enabled)
				label += "  disabled";
			if (row.Speed != 1.0f)
				label += std::format("  speed {:.2f}", row.Speed);
			if (row.Culled)
				label += row.CullingMode == AnimatorCullingMode::PauseWhenCulled ? "  culled, paused" : "  culled, every 4th update";
			label += "###animator";

			const bool expanded = ImGui::TreeNodeEx(label.c_str(), defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None);
			if (!expanded)
				s_ClosedRowHeight = ImGui::GetCursorPosY() - top;
			if (expanded)
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
		constexpr int k_Rows = 6;
		static float s_History[k_Rows][k_History] = {};
		static int s_Cursor = 0;
		static int s_Count = 0;

		const FrameTimings& frame = Application::Get().GetFrameTimings();
		const float samples[k_Rows] = { frame.FrameMs, frame.WaitMs, frame.UpdateMs, frame.DingoUIMs, frame.UIMs, Renderer::GetRenderThreadMilliseconds() };
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
		static constexpr const char* s_RowNames[k_Rows] = { "Frame", "Wait for render thread", "Update", "Game UI", "ImGui", "Render thread (execute + present)" };
		static constexpr int s_RowIndent[k_Rows] = { 0, 1, 1, 1, 1, 0 };
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
		if (Application::Get().GetGraphicsContext().GetGraphicsAPI() == GraphicsAPI::DirectX11)
			ImGui::TextDisabled("D3D11: approximate, a timestamp doesn't wait for the work before it");
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

	void SceneSection()
	{
		namespace SD = Internal::SceneDebug;

		// Keyed on the scene's serial and the entity's UUID, never on an address.
		static uint64_t s_SelectedScene = 0;
		static UUID s_SelectedEntity;
		static std::array<char, 64> s_NameFilter = {};
		static int s_ComponentFilter = -1;
		static std::vector<SD::SceneRow> s_Scenes;
		static std::vector<SD::EntityRow> s_Entities;
		static SD::EntityDetail s_Detail;

		SD::CollectScenes(s_Scenes);
		ImGui::TextUnformatted(std::format("Scenes ({})", s_Scenes.size()).c_str());
		ImGui::Separator();
		if (s_Scenes.empty())
		{
			ImGui::TextDisabled("No scene exists.");
			return;
		}

		const auto selected = std::find_if(s_Scenes.begin(), s_Scenes.end(), [](const SD::SceneRow& row) { return row.Serial == s_SelectedScene; });
		if (selected == s_Scenes.end())
		{
			s_SelectedScene = s_Scenes.front().Serial;
			s_SelectedEntity = UUID();
		}

		const uint32_t localBudget = Application::Get().GetRenderer3D().GetLocalLightBudget();
		if (ImGui::BeginTable("##scenes", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("Scene");
			ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 110.0f);
			ImGui::TableSetupColumn("Entities", ImGuiTableColumnFlags_WidthFixed, 60.0f);
			ImGui::TableSetupColumn("Local lights", ImGuiTableColumnFlags_WidthFixed, 80.0f);
			ImGui::TableSetupColumn("Bodies", ImGuiTableColumnFlags_WidthFixed, 60.0f);
			ImGui::TableHeadersRow();

			for (const SD::SceneRow& row : s_Scenes)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::PushID(static_cast<int>(row.Serial));
				if (ImGui::Selectable(row.Name.c_str(), row.Serial == s_SelectedScene, ImGuiSelectableFlags_SpanAllColumns))
				{
					if (s_SelectedScene != row.Serial)
						s_SelectedEntity = UUID();
					s_SelectedScene = row.Serial;
				}
				ImGui::PopID();

				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(!row.Running ? "stopped" : row.PhysicsRunning ? "running, physics" : "running");
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%u", row.Entities);
				ImGui::TableSetColumnIndex(3);
				ImGui::Text("%u / %u", row.PointLights + row.SpotLights, localBudget);
				ImGui::TableSetColumnIndex(4);
				ImGui::Text("%u", row.Bodies2D + row.Bodies3D + row.Controllers);
			}

			ImGui::EndTable();
		}

		const SD::SceneRow& scene = *std::find_if(s_Scenes.begin(), s_Scenes.end(), [](const SD::SceneRow& row) { return row.Serial == s_SelectedScene; });

		ImGui::Spacing();
		ImGui::TextUnformatted(std::format("{}  ({} entities, {} roots, {} scripts)", scene.Name, scene.Entities, scene.Roots, scene.Scripts).c_str());
		ImGui::Separator();
		if (scene.Camera.empty())
			ImGui::TextDisabled("Camera: none");
		else
			ImGui::Text("Camera: %s (%s)", scene.Camera.c_str(), scene.CameraProjection.c_str());

		const uint32_t localLights = scene.PointLights + scene.SpotLights;
		std::string lights = std::format("Lights: {} directional, {} ambient, {} point, {} spot", scene.DirectionalLights, scene.AmbientLights, scene.PointLights, scene.SpotLights);
		if (scene.DisabledLocalLights > 0)
			lights += std::format(" ({} disabled)", scene.DisabledLocalLights);
		if (scene.InactiveLocalLights > 0)
			lights += std::format(" ({} inactive)", scene.InactiveLocalLights);
		ImGui::TextUnformatted(lights.c_str());
		if (scene.UnplacedLocalLights > 0)
			ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.25f, 1.0f), "%u point or spot lights have no Transform3DComponent and are ignored", scene.UnplacedLocalLights);
		ImGui::TextUnformatted(std::format("Local lights: {} enabled against a budget of {} in view, {} casting shadows", localLights, localBudget, scene.ShadowedLocalLights).c_str());
		if (localLights > localBudget)
			ImGui::TextDisabled("More than the budget: past it, the lowest-priority lights in view are dropped (F4 shows the frame's count).");
		if (!scene.HasLight)
			ImGui::TextDisabled("No light component the renderer uses: lit by the default directional light.");
		ImGui::Text("Fog components: %u   Clear colour: (%.2f, %.2f, %.2f, %.2f)", scene.FogComponents,
			scene.ClearColor.r, scene.ClearColor.g, scene.ClearColor.b, scene.ClearColor.a);

		if (!scene.Has2DWorld && !scene.Has3DWorld)
			ImGui::TextDisabled(scene.Running ? "Physics: no world (the scene has no bodies or controllers)" : "Physics: not started");
		if (scene.Has2DWorld)
			ImGui::Text("Physics 2D: %u bodies, gravity (%.2f, %.2f)", scene.Bodies2D, scene.Gravity2D.x, scene.Gravity2D.y);
		if (scene.Has3DWorld)
			ImGui::Text("Physics 3D: %u bodies (%u sensors), %u controllers, gravity (%.2f, %.2f, %.2f)", scene.Bodies3D, scene.Sensors3D,
				scene.Controllers, scene.Gravity3D.x, scene.Gravity3D.y, scene.Gravity3D.z);
		if (scene.FixedTick)
			ImGui::Text("Fixed tick: on, step %.4f s, tick %llu, alpha %.2f, %u this update, %llu dropped, interpolation %s", scene.FixedStep,
				static_cast<unsigned long long>(scene.Tick), scene.FixedAlpha, scene.FixedStepsLastUpdate,
				static_cast<unsigned long long>(scene.DroppedFixedSteps), scene.InterpolatePhysics ? "on" : "off");
		else
			ImGui::Text("Fixed tick: off (physics steps per update), fixed step %.4f s, %u this update, %llu dropped", scene.FixedStep,
				scene.FixedStepsLastUpdate, static_cast<unsigned long long>(scene.DroppedFixedSteps));

		SD::CollectEntities(scene.Serial, s_Entities);

		ImGui::Spacing();
		ImGui::TextUnformatted("Entities");
		ImGui::Separator();
		ImGui::SetNextItemWidth(180.0f);
		ImGui::InputTextWithHint("##namefilter", "filter by name", s_NameFilter.data(), s_NameFilter.size());
		ImGui::SameLine();
		ImGui::SetNextItemWidth(180.0f);
		const char* componentLabel = s_ComponentFilter < 0 ? "any component" : SD::ComponentName(static_cast<SD::Component>(s_ComponentFilter));
		if (ImGui::BeginCombo("##componentfilter", componentLabel))
		{
			if (ImGui::Selectable("any component", s_ComponentFilter < 0))
				s_ComponentFilter = -1;
			for (int component = 0; component < static_cast<int>(SD::Component::Count); ++component)
			{
				if (ImGui::Selectable(SD::ComponentName(static_cast<SD::Component>(component)), component == s_ComponentFilter))
					s_ComponentFilter = component;
			}
			ImGui::EndCombo();
		}

		const std::string nameFilter = s_NameFilter.data();
		const bool filtering = !nameFilter.empty() || s_ComponentFilter >= 0;
		auto matches = [&](const SD::EntityRow& row)
		{
			if (s_ComponentFilter >= 0 && (row.Components & SD::ComponentBit(static_cast<SD::Component>(s_ComponentFilter))) == 0)
				return false;
			if (nameFilter.empty())
				return true;
			auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
			return std::search(row.Name.begin(), row.Name.end(), nameFilter.begin(), nameFilter.end(),
				[&](char a, char b) { return lower(a) == lower(b); }) != row.Name.end();
		};
		auto label = [](const SD::EntityRow& row)
		{
			std::string text = row.Name;
			if (!row.Socket.empty())
				text += std::format("  @{}", row.Socket);
			return text + std::format("###{}:{}", static_cast<uint64_t>(row.Id), row.Index);
		};

		ImGui::BeginChild("##entities", ImVec2(0.0f, 220.0f), ImGuiChildFlags_Borders);
		if (s_Entities.empty())
			ImGui::TextDisabled("The scene has no entities.");
		else if (filtering)
		{
			static std::vector<size_t> s_Matches;
			s_Matches.clear();
			for (size_t index = 0; index < s_Entities.size(); ++index)
			{
				if (matches(s_Entities[index]))
					s_Matches.push_back(index);
			}

			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(s_Matches.size()));
			while (clipper.Step())
			{
				for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line)
				{
					const SD::EntityRow& row = s_Entities[s_Matches[static_cast<size_t>(line)]];
					if (ImGui::Selectable(label(row).c_str(), row.Id == s_SelectedEntity))
						s_SelectedEntity = row.Id;
				}
			}
			if (s_Matches.empty())
				ImGui::TextDisabled("No entity matches.");
		}
		else
		{
			// A collapsed node skips its subtree, which follows it in the depth-first list.
			uint32_t pushed = 0;
			uint32_t skipBelow = (std::numeric_limits<uint32_t>::max)();
			for (const SD::EntityRow& row : s_Entities)
			{
				if (row.Depth > skipBelow)
					continue;
				skipBelow = (std::numeric_limits<uint32_t>::max)();
				for (; pushed > row.Depth; --pushed)
					ImGui::TreePop();

				ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
				if (row.ChildCount == 0)
					flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
				if (row.Id == s_SelectedEntity)
					flags |= ImGuiTreeNodeFlags_Selected;

				const bool open = ImGui::TreeNodeEx(label(row).c_str(), flags);
				if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
					s_SelectedEntity = row.Id;
				if (row.ChildCount > 0)
				{
					if (open)
						++pushed;
					else
						skipBelow = row.Depth;
				}
			}
			for (; pushed > 0; --pushed)
				ImGui::TreePop();
		}
		ImGui::EndChild();

		ImGui::Spacing();
		if (!SD::CollectEntity(scene.Serial, s_SelectedEntity, s_Detail))
		{
			ImGui::TextDisabled("Select an entity to inspect its components.");
			return;
		}

		ImGui::TextUnformatted(std::format("{}  (UUID {})", s_Detail.Name, static_cast<uint64_t>(s_Detail.Id)).c_str());
		ImGui::Separator();
		if (s_Detail.HasParent)
			ImGui::Text("Parent: %s%s%s", s_Detail.ParentName.c_str(), s_Detail.Socket.empty() ? "" : ", socket ", s_Detail.Socket.c_str());
		else
			ImGui::TextDisabled("Parent: none (a root)");
		ImGui::Text("Children: %u", s_Detail.ChildCount);
		if (!s_Detail.Active)
			ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.25f, 1.0f), "Inactive (SetActive(false)): not drawn, simulated or updated");
		else if (!s_Detail.ActiveInHierarchy)
			ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.25f, 1.0f), "Inactive through an ancestor: not drawn, simulated or updated");

		for (size_t index = 0; index < s_Detail.Sections.size(); ++index)
		{
			const SD::Section& section = s_Detail.Sections[index];
			ImGui::PushID(section.Title.c_str());
			const std::string title = section.Runtime ? section.Title + "  (runtime, read-only)###section" : section.Title + "###section";
			if (section.Runtime)
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.80f, 0.95f, 1.0f));
			const bool open = ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
			if (section.Runtime)
				ImGui::PopStyleColor();
			if (open && ImGui::BeginTable("##fields", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 190.0f);
				ImGui::TableSetupColumn("Value");
				for (const SD::Field& field : section.Fields)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(field.Name.c_str());
					ImGui::TableSetColumnIndex(1);
					ImGui::TextUnformatted(field.Value.c_str());
				}
				ImGui::EndTable();
			}
			ImGui::PopID();
		}

		if (s_Detail.Motor.Present)
		{
			SD::MotorView motor = s_Detail.Motor;
			bool changed = false;
			ImGui::PushID("##motor");
			if (ImGui::CollapsingHeader("Constraint motor (editable)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				const char* modes[] = { "Off", "Velocity", "Position" };
				int mode = static_cast<int>(motor.Mode);
				ImGui::SetNextItemWidth(160.0f);
				if (ImGui::Combo("Mode", &mode, modes, 3))
				{
					motor.Mode = static_cast<MotorMode3D>(mode);
					changed = true;
				}

				auto params = [&](const char* label, MotorParams3D& value)
				{
					ImGui::PushID(label);
					ImGui::TextUnformatted(label);
					changed |= ImGui::SliderFloat("Stiffness (N*m/rad)", &value.Stiffness, 0.0f, 100000.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
					changed |= ImGui::SliderFloat("Damping (N*m per rad/s)", &value.Damping, 0.0f, 10000.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
					changed |= ImGui::SliderFloat("MaxTorque (N*m)", &value.MaxTorque, 0.0f, 20000.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
					ImGui::PopID();
				};
				params(motor.SwingTwist ? "Swing motor" : "Hinge motor", motor.Motor);
				if (motor.SwingTwist)
					params("Twist motor", motor.TwistMotor);

				changed |= ImGui::SliderAngle("Target angle", &motor.TargetAngle, -180.0f, 180.0f);
				changed |= ImGui::SliderFloat("Target velocity (rad/s)", &motor.TargetVelocity, -20.0f, 20.0f);

				if (!motor.Live)
					ImGui::TextDisabled("No built joint: physics is not running or the bodies are missing.");
				else
				{
					if (!motor.SwingTwist)
						ImGui::Text("Current angle: %.1f deg", glm::degrees(motor.Angle));
					auto bar = [](const char* label, float torque, float maxTorque)
					{
						const float fraction = maxTorque > 0.0f ? (std::min)(torque / maxTorque, 1.0f) : 0.0f;
						const std::string text = std::format("{}: {:.1f} / {:.1f} N*m", label, torque, maxTorque);
						ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f), text.c_str());
					};
					bar(motor.SwingTwist ? "Swing torque" : "Torque", motor.Torque, motor.Motor.MaxTorque);
					if (motor.SwingTwist)
						bar("Twist torque", motor.TwistTorque, motor.TwistMotor.MaxTorque);
				}
			}
			ImGui::PopID();
			if (changed)
				SD::WriteMotor(scene.Serial, s_SelectedEntity, motor);
		}
	}

	void DingoUISection()
	{
		namespace DD = Internal::DingoUIDebug;

		static DD::Snapshot s_Snapshot;
		static bool s_ShowRects = false;
		static bool s_ShowBlocking = false;
		static bool s_ShowFocus = false;
		static bool s_ShowIds = false;
		static bool s_ShowTree = true;

		const bool detail = s_ShowRects || s_ShowBlocking || s_ShowFocus || s_ShowTree;
		if (!DD::Collect(s_Snapshot, detail))
		{
			ImGui::TextDisabled("DingoUI is off (headless, or DingoUIParams::Enabled is false).");
			return;
		}

		const DD::Snapshot& snapshot = s_Snapshot;
		const DD::Stats& stats = snapshot.Frame;

		ImGui::Text("Frame %llu, target %.0f x %.0f", static_cast<unsigned long long>(stats.Frame), snapshot.TargetSize.x, snapshot.TargetSize.y);
		ImGui::Text("Roots %u, elements %u, widgets %u", stats.Roots, stats.Elements, stats.Widgets);
		ImGui::Text("Draw calls %u, quads %u (culled %u), vertices %u", stats.DrawCalls, stats.Quads, stats.CulledQuads, stats.Vertices);
		ImGui::Text("Vertex cache: %s, uploads %u, upload bytes %llu", stats.CacheHits ? "hit" : (stats.Uploads ? "rebuilt" : "none"), stats.Uploads, static_cast<unsigned long long>(stats.UploadBytes));
		ImGui::Text("State map %u, tweens %u, transition roots %u, exiting roots %u, id collisions %u", stats.StateMapSize, stats.TweenCount, stats.TransitionRoots, stats.ExitingRoots, stats.IdCollisions);
		ImGui::Text("CPU ms: begin %.3f, layout %.3f, emit %.3f, hash %.3f, build %.3f, upload %.3f, draw %.3f", stats.BeginMs, stats.LayoutMs, stats.EmitMs, stats.HashMs, stats.BuildMs, stats.UploadMs, stats.DrawMs);
		ImGui::Text("Pointer over UI: %s, capturing input: %s, modal: %s", snapshot.PointerOverUI ? "yes" : "no", snapshot.CapturingInput ? "yes" : "no", snapshot.Modal ? "yes" : "no");
		ImGui::Text("Hover 0x%llx, active 0x%llx, focus 0x%llx%s", static_cast<unsigned long long>(snapshot.HoverId), static_cast<unsigned long long>(snapshot.ActiveId),
			static_cast<unsigned long long>(snapshot.FocusId), snapshot.FocusId && !snapshot.FocusVisible ? " (hidden)" : "");

		ImGui::Separator();
		ImGui::Checkbox("Show rects", &s_ShowRects);
		ImGui::SameLine();
		ImGui::Checkbox("Show hit-test/blocking rects", &s_ShowBlocking);
		ImGui::SameLine();
		ImGui::Checkbox("Show focus", &s_ShowFocus);
		ImGui::Checkbox("Ids on overlay", &s_ShowIds);
		ImGui::SameLine();
		ImGui::Checkbox("Element tree", &s_ShowTree);

		ImGui::Separator();
		if (ImGui::CollapsingHeader(std::format("Roots ({})", snapshot.Roots.size()).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::BeginTable("##DingoUIRoots", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 160.0f)))
			{
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableSetupColumn("Name");
				ImGui::TableSetupColumn("Layer");
				ImGui::TableSetupColumn("Space");
				ImGui::TableSetupColumn("Blocks");
				ImGui::TableSetupColumn("Modal");
				ImGui::TableSetupColumn("Nav");
				ImGui::TableSetupColumn("Transition");
				ImGui::TableHeadersRow();
				ImGuiListClipper clipper;
				clipper.Begin(static_cast<int>(snapshot.Roots.size()));
				while (clipper.Step())
				{
					for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
					{
						const DD::RootRow& root = snapshot.Roots[static_cast<size_t>(row)];
						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(root.Name.c_str());
						ImGui::TableNextColumn();
						ImGui::Text("%d", root.Layer);
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(root.World ? "world" : "screen");
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(root.BlocksInput ? "yes" : "no");
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(root.Modal ? "yes" : "no");
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(root.Navigable ? "yes" : "no");
						ImGui::TableNextColumn();
						if (!root.HasTransition)
							ImGui::TextDisabled("-");
						else
							ImGui::Text("%s %.0f%%", root.TransitionProgress >= 1.0f ? "shown" : "in", root.TransitionProgress * 100.0f);
					}
				}
				ImGui::EndTable();
			}
		}

		const auto kindName = [](DD::ElementKind kind)
		{
			switch (kind)
			{
			case DD::ElementKind::Root: return "Root";
			case DD::ElementKind::Box: return "Box";
			case DD::ElementKind::Text: return "Text";
			case DD::ElementKind::Image: return "Image";
			}
			return "?";
		};

		if (s_ShowTree && ImGui::CollapsingHeader(std::format("Elements ({}), last frame", snapshot.Elements.size()).c_str()))
		{
			if (ImGui::BeginChild("##DingoUITree", ImVec2(0.0f, 240.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar))
			{
				ImGuiListClipper clipper;
				clipper.Begin(static_cast<int>(snapshot.Elements.size()));
				while (clipper.Step())
				{
					for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
					{
						const DD::ElementRow& element = snapshot.Elements[static_cast<size_t>(row)];
						ImGui::Indent(static_cast<float>(element.Depth) * 12.0f);
						std::string text = std::format("{} 0x{:x}", kindName(element.Kind), element.Id);
						if (!element.Label.empty())
							text += std::format(" \"{}\"", element.Label.size() > 40 ? element.Label.substr(0, 40) : element.Label);
						text += std::format("  [{:.0f},{:.0f} - {:.0f},{:.0f}]", element.Min.x, element.Min.y, element.Max.x, element.Max.y);
						if (element.Interactive) text += " interactive";
						if (element.Scroll) text += " scroll";
						if (element.Focusable) text += " focusable";
						ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
						if (element.Active) color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
						else if (element.Hovered) color = ImVec4(1.0f, 0.9f, 0.3f, 1.0f);
						else if (element.Focused) color = ImVec4(0.4f, 0.9f, 1.0f, 1.0f);
						ImGui::TextColored(color, "%s", text.c_str());
						ImGui::Unindent(static_cast<float>(element.Depth) * 12.0f);
					}
				}
			}
			ImGui::EndChild();
		}

		if (!s_ShowRects && !s_ShowBlocking && !s_ShowFocus)
			return;

		ImDrawList* draw = ImGui::GetForegroundDrawList();
		const ImVec2 base = ImGui::GetMainViewport()->Pos;
		const auto map = [&](const glm::vec2& p)
		{
			return ImVec2(base.x + snapshot.Origin.x + p.x * snapshot.TargetToWindow.x, base.y + snapshot.Origin.y + p.y * snapshot.TargetToWindow.y);
		};

		if (s_ShowBlocking)
		{
			for (const auto& rect : snapshot.BlockingRects)
				draw->AddRectFilled(map(rect.first), map(rect.second), IM_COL32(255, 80, 80, 40));
		}

		if (s_ShowRects)
		{
			for (const DD::ElementRow& element : snapshot.Elements)
			{
				ImU32 color = IM_COL32(120, 200, 120, 150);
				float thickness = 1.0f;
				if (element.Kind == DD::ElementKind::Root) color = IM_COL32(255, 160, 40, 220);
				else if (element.Interactive || element.Scroll) color = IM_COL32(80, 160, 255, 200);
				if (element.Active) { color = IM_COL32(255, 70, 70, 255); thickness = 2.0f; }
				else if (element.Hovered) { color = IM_COL32(255, 230, 70, 255); thickness = 2.0f; }
				draw->AddRect(map(element.Min), map(element.Max), color, 0.0f, 0, thickness);
				if (s_ShowIds && (element.Interactive || element.Scroll || element.Kind == DD::ElementKind::Root))
				{
					const std::string id = std::format("{:x}", element.Id & 0xffffff);
					draw->AddText(map(element.Min), color, id.c_str());
				}
			}
		}

		if (s_ShowFocus && snapshot.FocusId != 0)
		{
			for (const DD::ElementRow& element : snapshot.Elements)
			{
				if (!element.Focused)
					continue;
				const ImU32 color = snapshot.FocusVisible ? IM_COL32(60, 230, 255, 255) : IM_COL32(60, 230, 255, 110);
				draw->AddRect(map(element.Min - glm::vec2(2.0f)), map(element.Max + glm::vec2(2.0f)), color, 0.0f, 0, 3.0f);
				const std::string text = std::format("focus 0x{:x}{}", element.Id, snapshot.FocusVisible ? "" : " (hidden)");
				draw->AddText(map(element.Min - glm::vec2(0.0f, 16.0f)), color, text.c_str());
				break;
			}
		}
	}

	void SceneStatsWindow(bool* open)
	{
		if (!ImGui::Begin("Scene", open))
		{
			ImGui::End();
			return;
		}

		SceneSection();

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

			tab("Scene", DebugTab::Scene, [] { SceneSection(); });

			tab("DingoUI", DebugTab::DingoUI, [] { DingoUISection(); });

			ImGui::EndTabBar();
		}

		ImGui::End();
		return active;
	}

}
