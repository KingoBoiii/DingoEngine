#pragma once

// ----------------------------------------------------------------------
// DingoEngine immediate-mode UI
// ----------------------------------------------------------------------
//
// A thin, engine-owned facade over the underlying UI backend (currently
// Dear ImGui). Client applications talk only to this header and never include
// or link the backend directly -- backend-specific code lives under
// src/DingoEngine/UI/<Backend>/ (e.g. src/DingoEngine/UI/ImGui/).
//
// Usage: call these only from Layer::OnImGuiRender(), i.e. between the engine's
// UI new-frame and render. They require ApplicationParams::EnableImGui = true.

/// The engine's immediate-mode UI facade (Dear ImGui underneath). Call it only from
/// Layer::OnImGuiRender; it needs ApplicationParams::EnableImGui = true.
namespace Dingo::UI
{

	/// RGBA colour, channels in the 0..1 range. Engine-owned so the public API
	/// does not leak any backend vector type.
	struct Color
	{
		float R = 1.0f;	///< Red, 0 to 1.
		float G = 1.0f;	///< Green, 0 to 1.
		float B = 1.0f;	///< Blue, 0 to 1.
		float A = 1.0f;	///< Alpha, 0 (transparent) to 1 (opaque).
	};

	// --- Windows ---------------------------------------------------------

	/// Begin a window. Returns false when the window is collapsed or fully
	/// clipped -- you may then skip drawing its contents -- but End() must be
	/// called regardless of the return value. When 'open' is non-null a close
	/// button is shown and *open is set to false when the user clicks it.
	bool Begin(const char* title, bool* open = nullptr);
	/// Ends the window started by Begin; call it once per Begin, even when Begin returned false.
	void End();

	// --- Text ------------------------------------------------------------

	/// printf-style formatted text.
	void Text(const char* fmt, ...);
	/// Like Text, drawn in the given colour.
	void TextColored(const Color& color, const char* fmt, ...);
	/// Like Text, wrapped at the window's right edge.
	void TextWrapped(const char* fmt, ...);

	/// Unformatted text -- safe to call with arbitrary strings (no printf
	/// parsing, so stray '%' characters are harmless).
	void Label(const char* text);

	// --- Widgets ---------------------------------------------------------

	/// Returns true on the frame the button is pressed.
	bool Button(const char* label);

	/// Toggle. Returns true on the frame the value changes.
	bool Checkbox(const char* label, bool* value);

	/// Sliders. Return true on the frame the value changes. Bounds are named
	/// minValue/maxValue (not min/max) on purpose: this header is compiled in
	/// engine translation units that include <Windows.h> without NOMINMAX,
	/// where min/max are macros.
	///@{
	bool SliderFloat(const char* label, float* value, float minValue, float maxValue);
	bool SliderInt(const char* label, int* value, int minValue, int maxValue);
	///@}

	// --- Layout ----------------------------------------------------------

	/// Draws a horizontal line.
	void Separator();
	/// Puts the next widget on the same line as the previous one, offsetFromStart pixels from the
	/// window's content start (0 = right after the previous widget); spacing < 0 uses the default.
	void SameLine(float offsetFromStart = 0.0f, float spacing = -1.0f);
	/// Adds a vertical gap.
	void Spacing();
	/// Starts a new line after a SameLine.
	void NewLine();

}
