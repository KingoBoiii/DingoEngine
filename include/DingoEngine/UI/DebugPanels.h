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

	// ----------------------------------------------------------------------
	// EngineStatsWindow sections
	// ----------------------------------------------------------------------
	//
	// Each function below draws one logical section's widgets (a heading plus its
	// rows) into whatever ImGui window is currently open -- it does not Begin/End a
	// window of its own. Call one from Layer::OnUIRender() (requires
	// ApplicationParams::EnableUI) to embed that section into your own debug window,
	// or use EngineStatsWindow() below to get all of them composed into one.

	// Engine version (major.minor.patch) and build number, from Version.h/BuildInfo.h.
	void EngineInfoSection();

	// Active graphics API and, when available, the adapter (GPU) name, vendor, and
	// dedicated video memory. Reads Application::GetGraphicsContext().
	void GraphicsInfoSection();

	// Window client size. Reads Application::GetWindow().
	void WindowInfoSection();

	// Compact FPS + frame-time line, from ImGui's own frame timing (io.Framerate).
	// Deliberately no rolling graph here -- that's RendererStatsWindow's job; this is
	// a one-line summary so it doesn't duplicate that window's content.
	void FrameTimingSection();

	// Audio engine status: validity and master volume, plus the active sound count
	// when the backend exposes one. Reads Application::GetAudioEngine().
	void AudioStatsSection();

	// A window composing every section above: engine info, graphics, window, frame
	// timing, and audio. Read-only -- no editing widgets.
	//
	// When 'open' is non-null a close button is shown and *open is set to false when
	// clicked -- pass the address of your own visibility bool to make it toggleable.
	void EngineStatsWindow(bool* open = nullptr);

	// ----------------------------------------------------------------------
	// InputStatsWindow sections
	// ----------------------------------------------------------------------
	//
	// Live views of the Input snapshot; same embedding rules as the sections above.

	// Cursor position, per-frame delta, scroll delta, and held mouse buttons.
	void MouseInputSection();

	// Every currently held key by name.
	void KeyboardInputSection();

	// One block per connected gamepad: slot, type, device name, held buttons, stick
	// and trigger values (deadzone-filtered vs raw), plus a live deadzone slider
	// (the panel's only editing widget -- it calls Input::SetGamepadDeadzone).
	void GamepadInputSection();

	// A window composing the three input sections above.
	//
	// When 'open' is non-null a close button is shown and *open is set to false when
	// clicked -- pass the address of your own visibility bool to make it toggleable.
	void InputStatsWindow(bool* open = nullptr);

}
