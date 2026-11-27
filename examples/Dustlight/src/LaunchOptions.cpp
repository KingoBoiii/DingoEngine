#include "LaunchOptions.h"

#include <DingoEngine.h>

#include <charconv>
#include <cmath>
#include <optional>
#include <string_view>
#include <system_error>

namespace
{
	using namespace Dingo;

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
			DE_WARN("Dustlight: ignoring --{}={} (expected no value, 1/true/on or 0/false/off)", name, *value);
	}

	template<typename T>
	void ParseNumber(const ApplicationCommandLineArgs& args, std::string_view name, T min, T max, T& out)
	{
		const std::optional<std::string_view> value = args.Get(name);
		if (!value)
			return;

		T parsed{};
		const char* end = value->data() + value->size();
		const auto [ptr, error] = std::from_chars(value->data(), end, parsed);
		if (error != std::errc{} || ptr != end || !(parsed >= min) || !(parsed <= max))
		{
			DE_WARN("Dustlight: ignoring --{}={} (expected {} to {})", name, *value, min, max);
			return;
		}
		out = parsed;
	}

	LaunchOptions Parse(const ApplicationCommandLineArgs& args)
	{
		LaunchOptions options;
		ParseFlag(args, "check", options.Check);
		ParseFlag(args, "no-raymarch", options.NoRaymarch);
		ParseFlag(args, "freeze", options.Freeze);
		ParseNumber(args, "env", 0, 2, options.Environment);
		ParseNumber(args, "view", 1, 3, options.View);
		ParseNumber(args, "time", 0.0f, 24.0f, options.TimeHours);
		return options;
	}
}

namespace Dingo
{

	LaunchOptions ParseLaunchOptions(const ApplicationCommandLineArgs& args)
	{
		return Parse(args);
	}

	const LaunchOptions& GetLaunchOptions()
	{
		static const LaunchOptions s_Options = ParseLaunchOptions(Application::Get().GetCommandLineArgs());
		return s_Options;
	}

}
