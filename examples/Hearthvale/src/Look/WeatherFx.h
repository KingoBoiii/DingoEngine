#pragma once
#include "Sim/SimTypes.h"

namespace Dingo
{

	// The hook P11 attaches rain and snow emitters to. Until then it only tracks the current weather and logs a
	// change. Plain C++, so the sim can drive it headless.
	class WeatherFx
	{
	public:
		// Returns true when the weather changed.
		bool SetWeather(Weather weather);

		Weather GetWeather() const { return m_Weather; }
		bool IsPrecipitating() const { return m_Weather != Weather::Clear; }

	private:
		Weather m_Weather = Weather::Clear;
	};

}
