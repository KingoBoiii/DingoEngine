#include "LaunchOptions.h"
#include "GameTuning.h"

#include <charconv>
#include <cmath>
#include <optional>
#include <string>
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
			DE_WARN("Marionette: ignoring --{}={} (expected {} to {})", name, *value, min, max);
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
			DE_WARN("Marionette: ignoring --seed={} (expected a non-negative whole number)", *value);
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
			DE_WARN("Marionette: ignoring --{}={} (expected seconds above 0 and up to {:.4f}); using the measured delta", name, *value, max);
			return;
		}

		out = parsed;
	}

	void ParseRange(const ApplicationCommandLineArgs& args, std::string_view name, float min, float max, float& out)
	{
		const std::optional<std::string_view> value = args.Get(name);
		if (!value)
			return;

		float parsed = 0.0f;
		if (!ParseNumber(*value, parsed) || !std::isfinite(parsed) || parsed < min || parsed > max)
		{
			DE_WARN("Marionette: ignoring --{}={} (expected {} to {})", name, *value, min, max);
			return;
		}

		out = parsed;
	}

	void ParseDrive(const ApplicationCommandLineArgs& args, DriveMode& out)
	{
		const std::optional<std::string_view> value = args.Get("drive");
		if (!value)
			return;

		if (*value == "ramp")
			out = DriveMode::Ramp;
		else if (*value == "circle")
			out = DriveMode::Circle;
		else if (*value == "strafe")
			out = DriveMode::Strafe;
		else if (*value == "wall")
			out = DriveMode::Wall;
		else if (*value == "duel")
			out = DriveMode::Duel;
		else
			DE_WARN("Marionette: ignoring --drive={} (expected ramp, circle, strafe, wall or duel)", *value);
	}

	void ParsePose(const ApplicationCommandLineArgs& args, std::string& clip, float& time)
	{
		const std::optional<std::string_view> value = args.Get("pose");
		if (!value || value->empty())
			return;

		const size_t at = value->find('@');
		float parsed = 0.0f;
		if (at != std::string_view::npos && (!ParseNumber(value->substr(at + 1), parsed) || !std::isfinite(parsed) || parsed < 0.0f))
		{
			DE_WARN("Marionette: ignoring --pose={} (expected <clip>@<seconds>)", *value);
			return;
		}

		clip = std::string(value->substr(0, at));
		time = parsed;
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
			DE_WARN("Marionette: ignoring --{}={} (expected no value, 1/true/on or 0/false/off)", name, *value);
	}

	LaunchOptions Parse(const ApplicationCommandLineArgs& args)
	{
		LaunchOptions options;
		ParseInt(args, "bout", 0, 3, options.Bout);
		ParseFlag(args, "lineup", options.Lineup);
		ParseFlag(args, "freeze", options.Freeze);
		ParseFlag(args, "overview", options.Overview);
		ParseFlag(args, "end", options.End);
		ParseFlag(args, "check", options.Check);
		ParseFlag(args, "autoplay", options.Autoplay);
		ParseFlag(args, "debug-hitbox", options.DebugHitbox);
		ParseFlag(args, "break-hitbox", options.BreakHitbox);
		ParseFlag(args, "hot-reload", options.HotReload);
		ParseFlag(args, "live-edit-demo", options.LiveEditDemo);
		if (options.LiveEditDemo)
			options.HotReload = true;
		ParseDrive(args, options.Drive);
		ParseInt(args, "tournament", 0, TOURNAMENT_MAX, options.Tournament);
		if (options.Tournament > 0)
		{
			options.Autoplay = true;
			if (options.Freeze || options.Lineup || options.Drive != DriveMode::None)
			{
				DE_WARN("Marionette: --tournament plays real bouts; ignoring --freeze, --lineup and --drive");
				options.Freeze = false;
				options.Lineup = false;
				options.Drive = DriveMode::None;
			}
		}
		if (!args.Get("bout") && (options.Drive != DriveMode::None || options.Freeze || options.Autoplay))
			options.Bout = 1;
		if (options.Tournament > 0 && options.Bout < 1)
		{
			DE_WARN("Marionette: --tournament plays a bout, not the title; using --bout=1");
			options.Bout = 1;
		}
		ParsePose(args, options.PoseClip, options.PoseTime);
		ParseRange(args, "move", 0.0f, MOVE_PARAMETER_MAX, options.Move);
		ParseRange(args, "phase", 0.0f, 1.0f, options.Phase);
		ParseSeconds(args, "fixed-dt", FIXED_DT_MAX, options.FixedDt);
		if (options.Tournament > 0 && !(options.FixedDt > 0.0f))
			options.FixedDt = FRAME_SECONDS;
		ParseSeed(args, options.Seed);

		if (options.Tournament > 0)
			options.StepsPerFrame = TOURNAMENT_STEPS_PER_FRAME;
		ParseInt(args, "steps-per-frame", 1, STEPS_PER_FRAME_MAX, options.StepsPerFrame);
		if (options.StepsPerFrame > 1 && (!(options.FixedDt > 0.0f) || !options.Autoplay))
		{
			DE_WARN("Marionette: --steps-per-frame needs --fixed-dt and --autoplay (a key press would reach every step); using 1");
			options.StepsPerFrame = 1;
		}

		if (!options.Freeze && (options.Move >= 0.0f || options.Phase >= 0.0f))
			DE_WARN("Marionette: --move and --phase only apply with --freeze");
		if (options.Freeze && options.Drive != DriveMode::None)
			DE_WARN("Marionette: --drive is ignored with --freeze");
		if (!options.Freeze && !options.PoseClip.empty())
			DE_WARN("Marionette: --pose only applies with --freeze");
		return options;
	}
}

namespace Dingo
{

	const char* ToString(DriveMode mode)
	{
		switch (mode)
		{
			case DriveMode::Ramp:   return "ramp";
			case DriveMode::Circle: return "circle";
			case DriveMode::Strafe: return "strafe";
			case DriveMode::Wall:   return "wall";
			case DriveMode::Duel:   return "duel";
			default:                return "none";
		}
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
