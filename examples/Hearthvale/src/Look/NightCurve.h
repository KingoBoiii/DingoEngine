#pragma once
#include "Sim/SimTypes.h"

namespace Dingo
{

	// How much of the night's lighting is on, as plain functions of the clock and the day's weather: no scene, no GPU,
	// so --check runs them headless. All amounts are 0..1.
	namespace NightCurve
	{
		float Darkness(float hours);

		// Lamps and windows glow a little by day when it rains or snows.
		float Gloom(Weather weather);

		// What the lamp lights, the bulbs and the window glow follow.
		float LampAmount(float hours, Weather weather);

		// Fireflies come out after dusk in the warm seasons and never in precipitation.
		float FireflyAmount(float hours, Season season, Weather weather);

		// Morning mist over the grass: dawn to mid-morning, never in precipitation, thickest in the damp seasons.
		float MistAmount(float hours, Season season, Weather weather);

		// Chimney smoke: colder seasons burn more, mornings and evenings most.
		float ChimneyAmount(float hours, Season season);
	}

}
