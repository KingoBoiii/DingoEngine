#include "Checks/Checks.h"
#include "Farm/Calendar.h"
#include "LaunchOptions.h"
#include "ValleyDirector.h"

#include <format>
#include <string>
#include <vector>

namespace Dingo
{

	namespace
	{
		void CheckClock(CheckReport& report)
		{
			Calendar calendar(1);
			report.Check(DAY_TICKS == 12 * 60 * 60 && calendar.GetClockText() == "06:00", "Calendar: a day starts at 06:00 and lasts 12 real minutes of sim ticks");

			calendar.Reset(1, DAY_TICKS / 2);
			report.Check(calendar.GetClockText() == "16:00" && calendar.GetHours() == 16.0f, "Calendar: half a day in is 16:00");
			calendar.Reset(1, DAY_TICKS - 1);
			report.Check(calendar.GetClockText() == "01:59" && !calendar.IsDayOver(), "Calendar: the last tick of the day is 01:59");
			calendar.AdvanceTick();
			report.Check(calendar.IsDayOver() && calendar.GetClockText() == "02:00", "Calendar: the day is over at 02:00");
			calendar.RollDay();
			report.Check(calendar.GetAbsoluteDay() == 2 && calendar.GetTickOfDay() == 0 && calendar.GetClockText() == "06:00", "Calendar: rolling the day returns to 06:00 of the next day");
		}

		void CheckSeasons(CheckReport& report)
		{
			report.Check(Calendar::SeasonOfDay(0) == Season::Spring && Calendar::DayOfSeasonOf(0) == 1, "Calendar: day 0 is spring day 1");
			report.Check(Calendar::SeasonOfDay(13) == Season::Spring && Calendar::DayOfSeasonOf(13) == 14, "Calendar: spring has 14 days");
			report.Check(Calendar::SeasonOfDay(14) == Season::Summer && Calendar::DayOfSeasonOf(14) == 1, "Calendar: day 14 is summer day 1");
			report.Check(Calendar::SeasonOfDay(28) == Season::Fall && Calendar::SeasonOfDay(42) == Season::Winter, "Calendar: fall then winter");

			Calendar calendar(1);
			calendar.Reset(DAYS_PER_YEAR - 1, 0);
			report.Check(calendar.GetYear() == 1 && calendar.GetSeason() == Season::Winter && calendar.GetDayOfSeason() == 14, "Calendar: the last day of the year is winter 14");
			calendar.RollDay();
			report.Check(calendar.GetYear() == 2 && calendar.GetSeason() == Season::Spring && calendar.GetDayOfSeason() == 1, "Calendar: winter 14 rolls into spring of year 2");

			bool roundTrip = true;
			for (int day = 0; day < DAYS_PER_YEAR * 3; ++day)
			{
				calendar.Reset(day, 0);
				roundTrip = roundTrip && Calendar::AbsoluteDayOf(calendar.GetYear(), calendar.GetSeason(), calendar.GetDayOfSeason()) == day;
			}
			report.Check(roundTrip, "Calendar: year, season and day of season round-trip to the absolute day");
		}

		void CheckWeather(CheckReport& report)
		{
			bool repeatable = true;
			bool seedsDiffer = false;
			bool winterOk = true;
			bool otherOk = true;
			int springRain = 0;
			constexpr int sample = DAYS_PER_YEAR * 40;
			for (int day = 1; day < sample; ++day)
			{
				const Weather a = Calendar::WeatherForDay(1, day);
				repeatable = repeatable && a == Calendar::WeatherForDay(1, day);
				seedsDiffer = seedsDiffer || a != Calendar::WeatherForDay(2, day);
				const Season season = Calendar::SeasonOfDay(day);
				if (season == Season::Winter)
					winterOk = winterOk && a != Weather::Rain;
				else
					otherOk = otherOk && a != Weather::Snow;
				if (season == Season::Spring && a == Weather::Rain)
					++springRain;
			}
			report.Check(repeatable, "Calendar: weather is a pure function of (seed, day)");
			report.Check(seedsDiffer, "Calendar: different seeds roll different weather");
			report.Check(winterOk && otherOk, "Calendar: snow only in winter, rain only outside it");
			const int springDays = sample / DAYS_PER_YEAR * DAYS_PER_SEASON;
			report.Check(springRain > springDays * 15 / 100 && springRain < springDays * 35 / 100,
				std::format("Calendar: about a quarter of spring days rain ({} of {})", springRain, springDays));
			report.Check(Calendar::WeatherForDay(1, 0) == Weather::Clear && Calendar::WeatherForDay(77, 0) == Weather::Clear, "Calendar: the first day is clear");

			Calendar calendar(1);
			calendar.Reset(3, 0);
			calendar.ForceWeather(Weather::Snow);
			const Weather natural = Calendar::WeatherForDay(1, 4);
			calendar.RollDay();
			report.Check(calendar.GetWeather() == natural, "Calendar: a forced weather lasts one day");
		}

		void CheckDirectorOrder(CheckReport& report)
		{
			ValleyDirector director(DirectorConfig{});
			std::vector<std::string> log;
			const char* const stageNames[] = { "Input", "PlayerIntent", "ToolsAndPlacement", "Calendar", "Villagers", "Animals", "Sky", "VfxAudio", "Camera", "Ui" };
			for (int i = 0; i < static_cast<int>(SimStage::Count); ++i)
				director.OnStage(static_cast<SimStage>(i), [&log, name = stageNames[i]](ValleyDirector&, const StepInfo&) { log.push_back(name); });
			const char* const rollNames[] = { "Crops", "Sprinklers", "Shipping", "Weather", "Board", "Autosave" };
			for (int i = 0; i < static_cast<int>(DayRollStep::Count); ++i)
				director.OnDayRoll(static_cast<DayRollStep>(i), [&log, name = rollNames[i]](ValleyDirector& d, DayRollReason) { log.push_back(std::format("roll:{}@{}", name, d.GetCalendar().GetAbsoluteDay())); });

			director.Step();
			const std::vector<std::string> expected = { "Input", "PlayerIntent", "ToolsAndPlacement", "Calendar", "Villagers", "Animals", "Sky", "VfxAudio", "Camera", "Ui" };
			report.Check(log == expected, "Director: a step runs the stages in the plan's order");

			log.clear();
			director.RequestSleep();
			director.Step();
			const std::vector<std::string> expectedRoll = { "Input", "PlayerIntent", "ToolsAndPlacement",
				"roll:Crops@0", "roll:Sprinklers@0", "roll:Shipping@0", "roll:Weather@1", "roll:Board@1", "roll:Autosave@1",
				"Calendar", "Villagers", "Animals", "Sky", "VfxAudio", "Camera", "Ui" };
			report.Check(log == expectedRoll, "Director: sleeping rolls the day between tools and the calendar stage, in the plan's order");
			report.Check(director.GetCalendar().GetAbsoluteDay() == 1 && director.GetCalendar().GetTickOfDay() == 0 && director.GetDayRollCount() == 1,
				"Director: a sleep lands on 06:00 of the next day");
		}

		void CheckDirectorTime(CheckReport& report)
		{
			ValleyDirector director(DirectorConfig{});
			int passOuts = 0;
			DayRollReason last = DayRollReason::Sleep;
			director.OnDayRoll(DayRollStep::Autosave, [&](ValleyDirector&, DayRollReason reason) { ++passOuts; last = reason; });

			for (int i = 0; i < DAY_TICKS - 1; ++i)
				director.Step();
			report.Check(director.GetDayRollCount() == 0 && director.GetCalendar().GetClockText() == "01:59", "Director: a day takes DAY_TICKS steps, not fewer");
			director.Step();
			report.Check(director.GetDayRollCount() == 1 && passOuts == 1 && last == DayRollReason::PassOut && director.GetCalendar().GetAbsoluteDay() == 1,
				"Director: 02:00 rolls the day as a pass-out");

			director.SetTimePaused(true);
			const int tick = director.GetCalendar().GetTickOfDay();
			for (int i = 0; i < 100; ++i)
				director.Step();
			report.Check(director.GetCalendar().GetTickOfDay() == tick && director.IsTimePaused(), "Director: paused time does not advance the clock");
			director.SetTimePaused(false);

			ValleyDirector fixed(DirectorConfig{});
			const int ran = fixed.Advance(SIM_STEP_SECONDS, SIM_MAX_STEPS_REAL_TIME);
			const int ranHalf = fixed.Advance(SIM_STEP_SECONDS * 0.5f, SIM_MAX_STEPS_REAL_TIME);
			const int ranRest = fixed.Advance(SIM_STEP_SECONDS * 0.5f, SIM_MAX_STEPS_REAL_TIME);
			const int ranBurst = fixed.Advance(10.0f, SIM_MAX_STEPS_REAL_TIME);
			report.Check(ran == 1 && ranHalf == 0 && ranRest == 1 && ranBurst == SIM_MAX_STEPS_REAL_TIME && fixed.GetStepCount() == 2u + SIM_MAX_STEPS_REAL_TIME,
				"Director: Advance turns real time into whole fixed steps and caps a burst");
		}

		void CheckDirectorConfig(CheckReport& report)
		{
			LaunchOptions options;
			options.Seed = 9;
			options.HasSeason = true;
			options.Season = LaunchSeason::Winter;
			options.Day = 3;
			options.TimeHours = 12.0f;
			options.Weather = LaunchWeather::Rain;
			const DirectorConfig config = MakeDirectorConfig(options);
			report.Check(config.Seed == 9 && config.StartDay == 44 && config.StartTick == 6 * 60 * TICKS_PER_MINUTE && config.HasForcedWeather && config.ForcedWeather == Weather::Rain,
				"Director: launch options map to winter day 3, 12:00, forced rain");

			ValleyDirector director(config);
			report.Check(director.GetCalendar().GetSeason() == Season::Winter && director.GetCalendar().GetDayOfSeason() == 3
				&& director.GetCalendar().GetClockText() == "12:00" && director.GetCalendar().GetWeather() == Weather::Rain,
				"Director: starts at the configured date, time and weather");

			LaunchOptions night;
			night.TimeHours = 1.0f;
			report.Check(MakeDirectorConfig(night).StartTick == 19 * 60 * TICKS_PER_MINUTE, "Director: --time=01:00 is the night after the day, not before it");
			report.Check(MakeDirectorConfig(LaunchOptions{}).StartDay == 0 && MakeDirectorConfig(LaunchOptions{}).StartTick == 0, "Director: default options start at spring 1, 06:00");
		}

		void CheckDirectorDeterminism(CheckReport& report)
		{
			DirectorConfig config;
			config.Seed = 5;
			ValleyDirector a(config);
			ValleyDirector b(config);
			std::string weatherA;
			std::string weatherB;
			a.OnDayRoll(DayRollStep::Board, [&](ValleyDirector& d, DayRollReason) { weatherA += ToString(d.GetCalendar().GetWeather()); });
			b.OnDayRoll(DayRollStep::Board, [&](ValleyDirector& d, DayRollReason) { weatherB += ToString(d.GetCalendar().GetWeather()); });
			for (int i = 0; i < DAY_TICKS * 3; ++i)
			{
				a.Step();
				b.Step();
			}
			report.Check(a.GetDayRollCount() == 3 && weatherA == weatherB && a.GetGrid().Hash() == b.GetGrid().Hash() && a.GetCalendar().GetAbsoluteDay() == 3,
				"Director: two runs of three days agree on weather, day and grid hash");
		}
	}

	void RunCalendarChecks(CheckReport& report)
	{
		SuiteScope scope(report, "Calendar");
		CheckClock(report);
		CheckSeasons(report);
		CheckWeather(report);
		CheckDirectorOrder(report);
		CheckDirectorTime(report);
		CheckDirectorConfig(report);
		CheckDirectorDeterminism(report);
	}

}
