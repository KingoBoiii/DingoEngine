#include "Look/WeatherFx.h"

#include <DingoEngine.h>

namespace Dingo
{

	bool WeatherFx::SetWeather(Weather weather)
	{
		if (weather == m_Weather)
			return false;
		m_Weather = weather;
		DE_INFO("[Weather] {}", ToString(weather));
		return true;
	}

}
