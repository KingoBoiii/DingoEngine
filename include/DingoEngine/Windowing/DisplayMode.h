#pragma once
#include <cstdint>
#include <vector>

namespace Dingo
{

	/// A monitor video mode: a resolution and refresh rate. A default-constructed one is invalid,
	/// which the window APIs read as "the desktop's mode".
	struct DisplayMode
	{
		int32_t Width = 0;			///< Width in pixels.
		int32_t Height = 0;			///< Height in pixels.
		int32_t RefreshRate = 0;	///< In Hz; 0 = any

		/// True when both sides are above 0; RefreshRate is not checked.
		bool IsValid() const { return Width > 0 && Height > 0; }
		/// Compares all three fields.
		bool operator==(const DisplayMode& other) const = default;
	};

	/// How the window uses the monitor; switch at runtime with Window::SetWindowMode.
	enum class WindowMode : uint8_t
	{
		Windowed,
		/// A fullscreen window at the monitor's desktop mode: the display mode never changes.
		BorderlessFullscreen,
		/// A fullscreen window that switches the monitor to a listed display mode.
		ExclusiveFullscreen
	};

	/// The mode's name ("Windowed", "BorderlessFullscreen" or "ExclusiveFullscreen"), as Settings
	/// writes it to a save.
	const char* ToString(WindowMode mode);
	/// Deprecated name for ToString(WindowMode).
	[[deprecated("use ToString(WindowMode); removed in 2.0")]] inline const char* WindowModeToString(WindowMode mode) { return ToString(mode); }

	/// Duplicates removed, largest first (by area, then width, then refresh rate). Invalid modes
	/// are dropped.
	std::vector<DisplayMode> SortDisplayModes(std::vector<DisplayMode> modes);

	/// The listed mode nearest the request: its size if listed (else the size nearest in area,
	/// then in aspect), then its refresh rate (else the nearest; 0 = the highest). An invalid
	/// request or an empty list returns the request unchanged.
	DisplayMode ChooseDisplayMode(const std::vector<DisplayMode>& modes, const DisplayMode& request);

}
