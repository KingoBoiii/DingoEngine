#include "LaunchOptions.h"

#include <DingoEngine.h>

#include <charconv>
#include <optional>
#include <string_view>
#include <system_error>

namespace
{
	using namespace Dingo;

	void ParseInt(const ApplicationCommandLineArgs& args, std::string_view name, int min, int max, int& out)
	{
		const std::optional<std::string_view> value = args.Get(name);
		if (!value)
			return;

		int parsed = 0;
		const char* end = value->data() + value->size();
		const auto [ptr, error] = std::from_chars(value->data(), end, parsed);
		if (error != std::errc{} || ptr != end || parsed < min || parsed > max)
		{
			DE_WARN("Candlewick: ignoring --{}={} (expected {} to {})", name, *value, min, max);
			return;
		}

		out = parsed;
	}

	void ParseFlag(const ApplicationCommandLineArgs& args, std::string_view name, bool& out)
	{
		const std::optional<std::string_view> value = args.Get(name);
		if (!value)
			return;

		if (value->empty() || *value == "1" || *value == "true" || *value == "on")
			out = true;
		else if (*value == "0" || *value == "false" || *value == "off")
			out = false;
		else
			DE_WARN("Candlewick: ignoring --{}={} (expected no value, 1/true/on or 0/false/off)", name, *value);
	}

	bool ParseCoordinate(std::string_view text, int& out)
	{
		const char* end = text.data() + text.size();
		const auto [ptr, error] = std::from_chars(text.data(), end, out);
		return error == std::errc{} && ptr == end && out >= 0;
	}

	void ParseTile(const ApplicationCommandLineArgs& args, std::string_view name, std::optional<glm::ivec2>& out)
	{
		const std::optional<std::string_view> value = args.Get(name);
		if (!value)
			return;

		const size_t comma = value->find(',');
		glm::ivec2 tile(0);
		if (comma == std::string_view::npos || !ParseCoordinate(value->substr(0, comma), tile.x) || !ParseCoordinate(value->substr(comma + 1), tile.y))
		{
			DE_WARN("Candlewick: ignoring --{}={} (expected <col>,<row>)", name, *value);
			return;
		}

		out = tile;
	}

	LaunchOptions Parse(const ApplicationCommandLineArgs& args)
	{
		LaunchOptions options;
		ParseInt(args, "room", 0, 4, options.Room);
		ParseInt(args, "oil", 0, 100, options.Oil);
		ParseFlag(args, "freeze", options.Freeze);
		ParseFlag(args, "overview", options.Overview);
		ParseFlag(args, "no-light-lod", options.NoLightLod);
		ParseFlag(args, "debug-cone", options.DebugCone);
		ParseFlag(args, "no-range-clamp", options.NoRangeClamp);
		ParseTile(args, "spawn", options.Spawn);
		return options;
	}
}

namespace Dingo
{

	const LaunchOptions& GetLaunchOptions()
	{
		static const LaunchOptions s_Options = Parse(Application::Get().GetCommandLineArgs());
		return s_Options;
	}

}
