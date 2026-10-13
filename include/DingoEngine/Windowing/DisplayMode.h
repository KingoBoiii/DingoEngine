#pragma once
#include <cstdint>
#include <vector>

namespace Dingo
{

	struct DisplayMode
	{
		int32_t Width = 0;
		int32_t Height = 0;
		int32_t RefreshRate = 0;	// Hz; 0 = any

		bool IsValid() const { return Width > 0 && Height > 0; }
		bool operator==(const DisplayMode& other) const = default;
	};

	enum class WindowMode : uint8_t
	{
		Windowed,
		// A fullscreen window at the monitor's desktop mode: the display mode never changes.
		BorderlessFullscreen,
		// A fullscreen window that switches the monitor to a listed display mode.
		ExclusiveFullscreen
	};

	const char* WindowModeToString(WindowMode mode);

	// Duplicates removed, largest first (by area, then width, then refresh rate).
	std::vector<DisplayMode> SortDisplayModes(std::vector<DisplayMode> modes);

	// The listed mode nearest the request: its size if listed (else the size nearest in area,
	// then in aspect), then its refresh rate (else the nearest; 0 = the highest). An invalid
	// request or an empty list returns the request unchanged.
	DisplayMode ChooseDisplayMode(const std::vector<DisplayMode>& modes, const DisplayMode& request);

}
