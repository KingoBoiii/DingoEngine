#include "depch.h"
#include "DingoEngine/Windowing/DisplayMode.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <tuple>

namespace Dingo
{

	const char* ToString(WindowMode mode)
	{
		switch (mode)
		{
			case WindowMode::Windowed: return "Windowed";
			case WindowMode::BorderlessFullscreen: return "BorderlessFullscreen";
			case WindowMode::ExclusiveFullscreen: return "ExclusiveFullscreen";
		}
		return "Unknown";
	}

	std::vector<DisplayMode> SortDisplayModes(std::vector<DisplayMode> modes)
	{
		std::erase_if(modes, [](const DisplayMode& mode) { return !mode.IsValid(); });
		std::sort(modes.begin(), modes.end(), [](const DisplayMode& a, const DisplayMode& b)
		{
			const int64_t areaA = int64_t(a.Width) * a.Height;
			const int64_t areaB = int64_t(b.Width) * b.Height;
			if (areaA != areaB)
				return areaA > areaB;
			if (a.Width != b.Width)
				return a.Width > b.Width;
			return a.RefreshRate > b.RefreshRate;
		});
		modes.erase(std::unique(modes.begin(), modes.end()), modes.end());
		return modes;
	}

	DisplayMode ChooseDisplayMode(const std::vector<DisplayMode>& modes, const DisplayMode& request)
	{
		if (!request.IsValid() || modes.empty())
			return request;

		const int64_t requestArea = int64_t(request.Width) * request.Height;
		const double requestAspect = double(request.Width) / double(request.Height);

		const DisplayMode* best = nullptr;
		auto sizeCost = [&](const DisplayMode& mode)
		{
			const bool exact = mode.Width == request.Width && mode.Height == request.Height;
			const int64_t area = std::llabs(int64_t(mode.Width) * mode.Height - requestArea);
			const double aspect = std::abs(double(mode.Width) / double(mode.Height) - requestAspect);
			return std::tuple(exact ? 0 : 1, area, aspect);
		};
		auto refreshCost = [&](const DisplayMode& mode)
		{
			return request.RefreshRate > 0 ? std::abs(mode.RefreshRate - request.RefreshRate) : -mode.RefreshRate;
		};

		for (const DisplayMode& mode : modes)
		{
			if (!mode.IsValid())
				continue;
			if (!best)
			{
				best = &mode;
				continue;
			}
			const auto cost = sizeCost(mode);
			const auto bestCost = sizeCost(*best);
			if (cost < bestCost || (cost == bestCost && refreshCost(mode) < refreshCost(*best)))
				best = &mode;
		}
		return best ? *best : request;
	}

}
