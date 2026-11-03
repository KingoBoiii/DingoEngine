#pragma once
#include <DingoEngine.h>

#include "Look/SkyCurve.h"
#include "Sim/SimTypes.h"

namespace Dingo
{

	// The sun, moon, ambient light, fog and clear colour of one scene, driven by SkyCurve. GPU side: never
	// constructed headless.
	class Sky
	{
	public:
		Sky(Scene& scene, bool shadows, bool fog);

		// The first call snaps to the time of day; later calls ease the sun's stepped angles in.
		void Update(float dt, float hours, Weather weather);

		const SkySample& GetSample() const { return m_Sample; }
		const SkyBlend& GetBlend() const { return m_Blend; }
		Entity GetSun() const { return m_Sun; }
		Entity GetMoon() const { return m_Moon; }

	private:
		Scene& m_Scene;
		Entity m_Ambient;
		Entity m_Sun;
		Entity m_Moon;
		Entity m_Fog;
		bool m_Shadows;
		SkySample m_Sample;
		SkyBlend m_Blend{ SKY_ANGLE_STEP_DEG, SKY_BLEND_SECONDS };
	};

}
