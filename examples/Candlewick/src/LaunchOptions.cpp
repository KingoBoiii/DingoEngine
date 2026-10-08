#include "LaunchOptions.h"

#include "GameTuning.h"

#include <DingoEngine.h>

#include <charconv>
#include <cmath>
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

	void ParseSeconds(const ApplicationCommandLineArgs& args, std::string_view name, float max, float& out)
	{
		const std::optional<std::string_view> value = args.Get(name);
		if (!value)
			return;

		float parsed = 0.0f;
		const char* end = value->data() + value->size();
		const auto [ptr, error] = std::from_chars(value->data(), end, parsed);
		if (error != std::errc{} || ptr != end || !std::isfinite(parsed) || !(parsed > 0.0f) || parsed > max)
		{
			DE_WARN("Candlewick: ignoring --{}={} (expected seconds above 0 and up to {:.4f})", name, *value, max);
			return;
		}

		out = parsed;
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
		ParseFlag(args, "all-lit", options.AllLit);
		ParseTile(args, "spawn", options.Spawn);
		ParseSeconds(args, "fixed-dt", FIXED_DT_MAX, options.FixedDt);
		if (options.Freeze && !(options.FixedDt > 0.0f))
			options.FixedDt = FIXED_DT_FREEZE;
		ParseFlag(args, "perf", options.Perf);
		ParseFlag(args, "no-post", options.NoPost);
		ParseFlag(args, "no-shadows", options.NoShadows);
		ParseFlag(args, "no-particles", options.NoParticles);
		ParseFlag(args, "hide-check", options.HideCheck);
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
