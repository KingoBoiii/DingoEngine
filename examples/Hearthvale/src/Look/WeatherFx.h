#pragma once
#include "Sim/SimTypes.h"

namespace Dingo
{

	// The current weather as the look and sound layer last saw it; Vfx and GameAudio read it from here. Plain C++, so
	// the sim can drive it headless.
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
