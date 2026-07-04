#include "depch.h"

#include "DingoEngine/UI/DebugPanels.h"

#include "DingoEngine/Core/Application.h"
#include "DingoEngine/Graphics/Renderer2D.h"
#include "DingoEngine/Graphics/Renderer3D.h"

#include <imgui.h>

#include <format>

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
	}

	void RendererStatsWindow(bool* open)
	{
		// Begin() returns false when collapsed/clipped; skip the body but still End().
		if (!ImGui::Begin("Renderer Stats", open))
		{
			ImGui::End();
			return;
		}

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
		ImGui::Text("%.1f FPS   %.2f ms/frame", io.Framerate, frameMs);

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

		ImGui::End();
	}

}
