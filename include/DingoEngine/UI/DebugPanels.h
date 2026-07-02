#pragma once

// ----------------------------------------------------------------------
// DingoEngine debug panels
// ----------------------------------------------------------------------
//
// Ready-made ImGui debug windows built on the engine's UI backend. Like the
// Dingo::UI primitive facade (DingoEngine/UI/UI.h), clients include only this
// header and never the backend -- the ImGui implementation lives under
// src/DingoEngine/UI/ImGui/.
//
// Call these from Layer::OnUIRender() (requires ApplicationParams::EnableUI).

namespace Dingo::UI
{

	// A window showing frame timing (FPS + a rolling frame-time graph) and the most
	// recent scene's Renderer2D and Renderer3D statistics: draw calls, primitive
	// counts, meshes submitted/dropped, and the 3D vertex/index budget usage. Reads
	// the engine's active renderers (Application::GetRenderer2D/3D), so no arguments
	// are needed.
	//
	// When 'open' is non-null a close button is shown and *open is set to false when
	// clicked -- pass the address of your own visibility bool to make it toggleable.
	void RendererStatsWindow(bool* open = nullptr);

}
