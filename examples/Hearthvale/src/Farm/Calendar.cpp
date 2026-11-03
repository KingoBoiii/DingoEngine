#include "Farm/Calendar.h"
#include "Sim/Rng.h"

#include <format>

namespace Dingo
{

	const char* ToString(Season season)
	{
		switch (season)
		{
			case Season::Spring: return "spring";
			case Season::Summer: return "summer";
			case Season::Fall:   return "fall";
			case Season::Winter: return "winter";
		}
		return "spring";
	}

	const char* ToString(Weather weather)
	{
		switch (weather)
		{
			case Weather::Clear: return "clear";
			case Weather::Rain:  return "rain";
			case Weather::Snow:  return "snow";
		}
		return "clear";
	}

	Calendar::Calendar(uint32_t seed)
		: m_Seed(seed)
	{
		Reset(0, 0);
	}

	void Calendar::Reset(int absoluteDay, int tickOfDay)
	{
		m_Day = absoluteDay;
		m_Tick = tickOfDay;
		m_Weather = WeatherForDay(m_Seed, m_Day);
	}

	void Calendar::RollDay()
	{
		Reset(m_Day + 1, 0);
	}

	std::string Calendar::GetClockText() const
	{
		const int minutes = GetMinutesSinceMidnight();
		return std::format("{:02}:{:02}", (minutes / 60) % 24, minutes % 60);
	}

	int Calendar::AbsoluteDayOf(int year, Season season, int dayOfSeason)
	{
		return (year - 1) * DAYS_PER_YEAR + static_cast<int>(season) * DAYS_PER_SEASON + (dayOfSeason - 1);
	}

	Weather Calendar::WeatherForDay(uint32_t seed, int absoluteDay)
	{
		if (absoluteDay <= 0)
			return Weather::Clear;

		int permille = WEATHER_PRECIP_PERMILLE_SPRING;
		switch (SeasonOfDay(absoluteDay))
		{
			case Season::Spring: permille = WEATHER_PRECIP_PERMILLE_SPRING; break;
			case Season::Summer: permille = WEATHER_PRECIP_PERMILLE_SUMMER; break;
			case Season::Fall:   permille = WEATHER_PRECIP_PERMILLE_FALL; break;
			case Season::Winter: permille = WEATHER_PRECIP_PERMILLE_WINTER; break;
		}

		const int roll = static_cast<int>(HashMix(seed, static_cast<uint64_t>(absoluteDay)) % 1000ull);
		if (roll >= permille)
			return Weather::Clear;
		return SeasonOfDay(absoluteDay) == Season::Winter ? Weather::Snow : Weather::Rain;
	}

}
