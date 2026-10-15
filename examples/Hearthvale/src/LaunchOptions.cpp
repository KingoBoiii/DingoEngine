#include "LaunchOptions.h"
#include "GameTuning.h"

#include <charconv>
#include <cmath>
#include <optional>
#include <string_view>
#include <system_error>

namespace
{
	using namespace Dingo;

	template<typename T>
	bool ParseNumber(std::string_view text, T& out)
	{
		const char* end = text.data() + text.size();
		const auto [ptr, error] = std::from_chars(text.data(), end, out);
		return error == std::errc{} && ptr == end;
	}

	void ParseInt(const ApplicationCommandLineArgs& args, std::string_view name, int min, int max, int& out)
	{
		const std::optional<std::string_view> value = args.Get(name);
		if (!value)
			return;

		int parsed = 0;
		if (!ParseNumber(*value, parsed) || parsed < min || parsed > max)
		{
			DE_WARN("Hearthvale: ignoring --{}={} (expected {} to {})", name, *value, min, max);
			return;
		}

		out = parsed;
	}

	void ParseSeed(const ApplicationCommandLineArgs& args, uint32_t& out)
	{
		const std::optional<std::string_view> value = args.Get("seed");
		if (!value)
			return;

		uint32_t parsed = 0;
		if (!ParseNumber(*value, parsed))
		{
			DE_WARN("Hearthvale: ignoring --seed={} (expected a non-negative whole number)", *value);
			return;
		}

		out = parsed;
	}

	void ParseSeconds(const ApplicationCommandLineArgs& args, std::string_view name, float max, float& out)
	{
		const std::optional<std::string_view> value = args.Get(name);
		if (!value)
			return;

		float parsed = 0.0f;
		if (!ParseNumber(*value, parsed) || !std::isfinite(parsed) || !(parsed > 0.0f) || parsed > max)
		{
			DE_WARN("Hearthvale: ignoring --{}={} (expected seconds above 0 and up to {:.4f}); using the measured delta", name, *value, max);
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
			DE_WARN("Hearthvale: ignoring --{}={} (expected no value, 1/true/on or 0/false/off)", name, *value);
	}

	void ParseSeason(const ApplicationCommandLineArgs& args, LaunchOptions& options)
	{
		const std::optional<std::string_view> value = args.Get("season");
		if (!value)
			return;

		if (*value == "spring")
			options.Season = LaunchSeason::Spring;
		else if (*value == "summer")
			options.Season = LaunchSeason::Summer;
		else if (*value == "fall")
			options.Season = LaunchSeason::Fall;
		else if (*value == "winter")
			options.Season = LaunchSeason::Winter;
		else
		{
			DE_WARN("Hearthvale: ignoring --season={} (expected spring, summer, fall or winter)", *value);
			return;
		}
		options.HasSeason = true;
	}

	void ParseWeather(const ApplicationCommandLineArgs& args, LaunchWeather& out)
	{
		const std::optional<std::string_view> value = args.Get("weather");
		if (!value)
			return;

		if (*value == "clear")
			out = LaunchWeather::Clear;
		else if (*value == "rain")
			out = LaunchWeather::Rain;
		else if (*value == "snow")
			out = LaunchWeather::Snow;
		else
			DE_WARN("Hearthvale: ignoring --weather={} (expected clear, rain or snow)", *value);
	}

	void ParseTime(const ApplicationCommandLineArgs& args, float& out)
	{
		const std::optional<std::string_view> value = args.Get("time");
		if (!value)
			return;

		const size_t colon = value->find(':');
		int hours = 0;
		int minutes = 0;
		if (colon == std::string_view::npos || !ParseNumber(value->substr(0, colon), hours) || !ParseNumber(value->substr(colon + 1), minutes)
			|| hours < 0 || hours > 26 || minutes < 0 || minutes > 59)
		{
			DE_WARN("Hearthvale: ignoring --time={} (expected HH:MM, 00:00 to 26:59)", *value);
			return;
		}

		out = static_cast<float>(hours) + static_cast<float>(minutes) / 60.0f;
	}

	void ParseLayout(const ApplicationCommandLineArgs& args, bool& out)
	{
		const std::optional<std::string_view> value = args.Get("layout");
		if (!value)
			return;

		if (*value == "demo")
			out = true;
		else
			DE_WARN("Hearthvale: ignoring --layout={} (expected demo)", *value);
	}

	LaunchOptions Parse(const ApplicationCommandLineArgs& args)
	{
		LaunchOptions options;
		ParseFlag(args, "check", options.Check);
		ParseFlag(args, "autoplay", options.Autoplay);
		ParseFlag(args, "perf", options.Perf);
		ParseFlag(args, "freeze", options.Freeze);
		ParseFlag(args, "no-post", options.NoPost);
		ParseFlag(args, "no-shadows", options.NoShadows);
		ParseFlag(args, "no-particles", options.NoParticles);
		ParseFlag(args, "no-fog", options.NoFog);
		ParseFlag(args, "no-instancing", options.NoInstancing);
		ParseFlag(args, "no-static", options.NoStatic);
		ParseFlag(args, "no-cull", options.NoCull);
		ParseLayout(args, options.LayoutDemo);
		ParseInt(args, "days", 1, DAYS_MAX, options.Days);
		ParseInt(args, "day", 1, DAYS_PER_SEASON, options.Day);
		ParseSeason(args, options);
		ParseTime(args, options.TimeHours);
		ParseWeather(args, options.Weather);
		ParseSeed(args, options.Seed);
		if (const std::optional<std::string_view> load = args.Get("load"); load && !load->empty())
			options.Load = std::string(*load);

		ParseSeconds(args, "fixed-dt", FIXED_DT_MAX, options.FixedDt);
		if (options.Days > 0)
		{
			if (!options.Autoplay)
			{
				DE_WARN("Hearthvale: --days needs --autoplay; enabling it");
				options.Autoplay = true;
			}
			if (!(options.FixedDt > 0.0f))
				options.FixedDt = SIM_STEP_SECONDS;
		}
		ParseInt(args, "steps-per-frame", 1, STEPS_PER_FRAME_MAX, options.StepsPerFrame);
		if (options.StepsPerFrame > 1 && !(options.FixedDt > 0.0f))
		{
			DE_WARN("Hearthvale: --steps-per-frame needs --fixed-dt (or --days); using 1");
			options.StepsPerFrame = 1;
		}
		if (options.Perf && !options.Autoplay)
		{
			DE_WARN("Hearthvale: --perf measures an --autoplay run; ignoring it");
			options.Perf = false;
		}
		return options;
	}
}

namespace Dingo
{

	const char* ToString(LaunchSeason season)
	{
		switch (season)
		{
			case LaunchSeason::Summer: return "summer";
			case LaunchSeason::Fall:   return "fall";
			case LaunchSeason::Winter: return "winter";
			default:                   return "spring";
		}
	}

	const char* ToString(LaunchWeather weather)
	{
		switch (weather)
		{
			case LaunchWeather::Clear: return "clear";
			case LaunchWeather::Rain:  return "rain";
			case LaunchWeather::Snow:  return "snow";
			default:                   return "auto";
		}
	}

	bool IsScripted(const LaunchOptions& options)
	{
		return options.Check || options.Autoplay || options.Perf || options.FixedDt > 0.0f;
	}

	const LaunchOptions& ParseLaunchOptions(const ApplicationCommandLineArgs& args)
	{
		static const LaunchOptions s_Options = Parse(args);
		return s_Options;
	}

	const LaunchOptions& GetLaunchOptions()
	{
		return ParseLaunchOptions(Application::Get().GetCommandLineArgs());
	}

}
