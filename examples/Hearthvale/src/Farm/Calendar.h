#pragma once
#include "GameTuning.h"
#include "Sim/SimTypes.h"

#include <cstdint>
#include <string>

namespace Dingo
{

	inline constexpr int DAYS_PER_YEAR = DAYS_PER_SEASON * SEASON_COUNT;
	inline constexpr int DAY_START_MINUTES = static_cast<int>(DAY_START_HOUR * 60.0f);
	inline constexpr int DAY_MINUTES = static_cast<int>((DAY_END_HOUR - DAY_START_HOUR) * 60.0f);
	static_assert(DAY_TICKS % DAY_MINUTES == 0, "a game minute must be a whole number of sim ticks");
	inline constexpr int TICKS_PER_MINUTE = DAY_TICKS / DAY_MINUTES;

	// Time is integer sim ticks, so a simulated week is bit-identical run to run.
	class Calendar
	{
	public:
		explicit Calendar(uint32_t seed);

		void Reset(int absoluteDay, int tickOfDay);

		// Replaces today's rolled weather; the next day rolls normally.
		void ForceWeather(Weather weather) { m_Weather = weather; }

		void AdvanceTick() { ++m_Tick; }
		bool IsDayOver() const { return m_Tick >= DAY_TICKS; }

		// Moves to 6:00 of the next day and rolls its weather.
		void RollDay();

		int GetAbsoluteDay() const { return m_Day; }
		int GetTickOfDay() const { return m_Tick; }
		int GetYear() const { return m_Day / DAYS_PER_YEAR + 1; }
		Season GetSeason() const { return SeasonOfDay(m_Day); }
		int GetDayOfSeason() const { return DayOfSeasonOf(m_Day); }
		Weather GetWeather() const { return m_Weather; }

		// Clock hours since midnight in [6, 26].
		float GetHours() const { return DAY_START_HOUR + static_cast<float>(m_Tick) / static_cast<float>(TICKS_PER_MINUTE * 60); }
		int GetMinutesSinceMidnight() const { return DAY_START_MINUTES + m_Tick / TICKS_PER_MINUTE; }
		std::string GetClockText() const;

		static Season SeasonOfDay(int absoluteDay) { return static_cast<Season>((absoluteDay / DAYS_PER_SEASON) % SEASON_COUNT); }
		static int DayOfSeasonOf(int absoluteDay) { return absoluteDay % DAYS_PER_SEASON + 1; }
		static int AbsoluteDayOf(int year, Season season, int dayOfSeason);
		static Weather WeatherForDay(uint32_t seed, int absoluteDay);

	private:
		uint32_t m_Seed;
		int m_Day = 0;
		int m_Tick = 0;
		Weather m_Weather = Weather::Clear;
	};

}
