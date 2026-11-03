#include "Audio/AudioMix.h"
#include "Checks/Checks.h"
#include "GameTuning.h"
#include "Look/LampLod.h"
#include "Look/NightCurve.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <vector>

namespace Dingo
{

	namespace
	{
		constexpr float k_Eps = 1e-4f;

		bool Near(float a, float b, float eps = k_Eps)
		{
			return std::abs(a - b) <= eps;
		}

		void CheckNightCurve(CheckReport& report)
		{
			bool bounded = true;
			bool dusk = true;
			bool dawn = true;
			float lastDusk = 0.0f;
			float lastDawn = 1.0f;
			for (float hours = 6.0f; hours <= 26.0f; hours += 0.05f)
			{
				const float dark = NightCurve::Darkness(hours);
				bounded = bounded && dark >= 0.0f && dark <= 1.0f;
				for (Season season : { Season::Spring, Season::Summer, Season::Fall, Season::Winter })
				{
					for (Weather weather : { Weather::Clear, Weather::Rain, Weather::Snow })
					{
						for (float value : { NightCurve::LampAmount(hours, weather), NightCurve::FireflyAmount(hours, season, weather),
							NightCurve::MistAmount(hours, season, weather), NightCurve::ChimneyAmount(hours, season) })
							bounded = bounded && value >= 0.0f && value <= 1.0f;
					}
				}
				if (hours >= NIGHT_DUSK_START_HOUR && hours <= NIGHT_DUSK_END_HOUR)
				{
					dusk = dusk && dark >= lastDusk - k_Eps;
					lastDusk = dark;
				}
				if (hours >= NIGHT_DAWN_START_HOUR && hours <= NIGHT_DAWN_END_HOUR)
				{
					dawn = dawn && dark <= lastDawn + k_Eps;
					lastDawn = dark;
				}
			}
			report.Check(bounded, "Look: every night, firefly, mist and chimney amount is within 0..1 all day");
			report.Check(dusk && dawn, "Look: darkness only grows through dusk and only shrinks through dawn");
			report.Check(NightCurve::Darkness(12.0f) == 0.0f && NightCurve::Darkness(22.0f) == 1.0f && NightCurve::Darkness(6.0f) == 1.0f,
				"Look: noon is bright, 22:00 and the 6:00 start are dark");
			report.Check(NightCurve::LampAmount(12.0f, Weather::Clear) == 0.0f && NightCurve::LampAmount(23.0f, Weather::Clear) == 1.0f,
				"Look: lamps are off at a clear noon and on at night");
			report.Check(NightCurve::LampAmount(12.0f, Weather::Rain) == NIGHT_GLOOM_RAIN && NightCurve::LampAmount(12.0f, Weather::Snow) == NIGHT_GLOOM_SNOW,
				"Look: rain and snow glow the lamps by day");
			report.Check(NightCurve::FireflyAmount(21.0f, Season::Summer, Weather::Clear) > 0.9f
				&& NightCurve::FireflyAmount(21.0f, Season::Winter, Weather::Clear) == 0.0f
				&& NightCurve::FireflyAmount(21.0f, Season::Summer, Weather::Rain) == 0.0f
				&& NightCurve::FireflyAmount(12.0f, Season::Summer, Weather::Clear) == 0.0f,
				"Look: fireflies come out on summer evenings and not in winter, rain or daylight");
			report.Check(NightCurve::MistAmount(6.0f, Season::Fall, Weather::Clear) > 0.9f && NightCurve::MistAmount(12.0f, Season::Fall, Weather::Clear) == 0.0f
				&& NightCurve::MistAmount(6.0f, Season::Fall, Weather::Rain) == 0.0f,
				"Look: mist lies at dawn, lifts by noon and never comes with precipitation");
			report.Check(NightCurve::ChimneyAmount(7.0f, Season::Winter) > NightCurve::ChimneyAmount(7.0f, Season::Summer)
				&& NightCurve::ChimneyAmount(7.0f, Season::Winter) > NightCurve::ChimneyAmount(13.0f, Season::Winter),
				"Look: chimneys smoke more in winter and more in the morning than at midday");
		}

		struct LodRun
		{
			bool UnderCapacity = true;
			bool Bounded = true;
			bool SawFade = false;
			int MaxInView = 0;
			std::vector<float> Trace;
		};

		LodRun RunLod(int capacity, float fade)
		{
			constexpr int k_Lamps = 40;
			constexpr float k_HalfView = 12.0f;
			std::vector<LampLodInput> lights(k_Lamps);
			for (int i = 0; i < k_Lamps; ++i)
				lights[static_cast<size_t>(i)].Position = glm::vec3(static_cast<float>(i) * 3.0f, 2.0f, 0.0f);

			LampLod lod(fade, LIGHT_LOD_VIEW_MARGIN, LIGHT_LOD_STICKINESS);
			LodRun run;
			for (int frame = 0; frame < 900; ++frame)
			{
				const float focus = 10.0f + 100.0f * (0.5f + 0.5f * std::sin(static_cast<float>(frame) * 0.011f));
				for (LampLodInput& light : lights)
					light.Overshoot = (std::max)(0.0f, std::abs(light.Position.x - focus) - k_HalfView);
				lod.Update(1.0f / 60.0f, lights, glm::vec3(focus, 0.0f, 0.0f), capacity);

				const int inView = lod.CountInView(lights);
				run.MaxInView = (std::max)(run.MaxInView, inView);
				run.UnderCapacity = run.UnderCapacity && inView <= capacity;
				for (float weight : lod.GetWeights())
				{
					run.Bounded = run.Bounded && weight >= 0.0f && weight <= 1.0f;
					run.SawFade = run.SawFade || (weight > 0.0f && weight < 1.0f);
				}
				run.Trace.push_back(lod.GetWeights()[static_cast<size_t>(frame % k_Lamps)]);
			}
			return run;
		}

		void CheckLampLod(CheckReport& report)
		{
			bool allUnder = true;
			for (int capacity : { 0, 1, 4, 8, 14, 30 })
			{
				const LodRun run = RunLod(capacity, LIGHT_LOD_FADE_TIME);
				allUnder = allUnder && run.UnderCapacity && run.Bounded;
			}
			report.Check(allUnder, "Look: lamps in view never exceed the capacity over a long walk, for capacities 0 to 30");

			const LodRun run = RunLod(8, LIGHT_LOD_FADE_TIME);
			report.Check(run.MaxInView >= 6, std::format("Look: the LOD still lights lamps in view (peak {} of capacity 8)", run.MaxInView));
			report.Check(run.SawFade, "Look: a lamp fades through partial weights instead of jumping");

			const LodRun again = RunLod(8, LIGHT_LOD_FADE_TIME);
			report.Check(run.Trace == again.Trace, "Look: the LOD is deterministic");

			std::vector<LampLodInput> lights(5);
			for (size_t i = 0; i < lights.size(); ++i)
				lights[i].Position = glm::vec3(static_cast<float>(i), 0.0f, 0.0f);
			LampLod lod(0.4f, 2.0f, 0.6f);
			lod.Update(0.0f, lights, glm::vec3(0.0f), 3);
			int on = 0;
			bool exact = true;
			for (float weight : lod.GetWeights())
			{
				on += weight > 0.0f ? 1 : 0;
				exact = exact && (weight == 0.0f || weight == 1.0f);
			}
			report.Check(on == 3 && exact && lod.GetWeights()[0] == 1.0f && lod.GetWeights()[4] == 0.0f,
				"Look: the first update snaps to the nearest capacity lamps");

			for (LampLodInput& light : lights)
				light.Overshoot = 50.0f;
			lod.Update(1.0f, lights, glm::vec3(0.0f), 1);
			bool outside = true;
			for (float weight : lod.GetWeights())
				outside = outside && weight == 1.0f;
			report.Check(outside, "Look: lamps outside the view margin keep full weight");
		}

		void CheckAudioMix(CheckReport& report)
		{
			using namespace AudioMix;
			const AmbienceLevels day = AmbienceTargets(12.0f, Season::Spring, Weather::Clear);
			const AmbienceLevels night = AmbienceTargets(23.0f, Season::Summer, Weather::Clear);
			const AmbienceLevels rain = AmbienceTargets(12.0f, Season::Spring, Weather::Rain);
			const AmbienceLevels snow = AmbienceTargets(12.0f, Season::Winter, Weather::Snow);
			report.Check(day.Birds == 1.0f && day.Crickets == 0.0f && day.Rain == 0.0f, "Audio: a clear spring noon has birds, no crickets, no rain");
			report.Check(night.Crickets == 1.0f && night.Birds == 0.0f, "Audio: a clear summer night has crickets and no birds");
			report.Check(rain.Rain == 1.0f && rain.Birds == 0.0f && rain.Crickets == 0.0f, "Audio: rain plays the rain layer and silences birds and crickets");
			report.Check(snow.Rain == 0.0f && snow.Wind > day.Wind && snow.Wind <= 1.0f, "Audio: snow has no rain layer and more wind than a calm day");
			report.Check(AmbienceTargets(23.0f, Season::Winter, Weather::Clear).Crickets == 0.0f, "Audio: winter nights have no crickets");

			bool bounded = true;
			for (float hours = 6.0f; hours <= 26.0f; hours += 0.1f)
			{
				for (Season season : { Season::Spring, Season::Summer, Season::Fall, Season::Winter })
				{
					for (Weather weather : { Weather::Clear, Weather::Rain, Weather::Snow })
					{
						const AmbienceLevels levels = AmbienceTargets(hours, season, weather);
						for (float value : { levels.Birds, levels.Crickets, levels.Wind, levels.Rain })
							bounded = bounded && value >= 0.0f && value <= 1.0f;
					}
				}
			}
			report.Check(bounded, "Audio: every ambience level is within 0..1 for every hour, season and weather");

			bool monotone = true;
			float last = 0.0f;
			for (float hours = MUSIC_FADE_IN_START_HOUR; hours <= MUSIC_FADE_IN_END_HOUR; hours += 0.01f)
			{
				const float value = MusicEnvelope(hours);
				monotone = monotone && value >= last - k_Eps;
				last = value;
			}
			last = 1.0f;
			for (float hours = MUSIC_FADE_OUT_START_HOUR; hours <= MUSIC_FADE_OUT_END_HOUR; hours += 0.01f)
			{
				const float value = MusicEnvelope(hours);
				monotone = monotone && value <= last + k_Eps;
				last = value;
			}
			report.Check(monotone && MusicEnvelope(6.0f) == 0.0f && MusicEnvelope(12.0f) == 1.0f && MusicEnvelope(20.0f) == 0.0f && MusicEnvelope(24.0f) == 0.0f,
				"Audio: the music fades in at dawn, plays through the day and fades out by 20:00");

			bool power = true;
			for (float p = 0.0f; p <= 1.0f; p += 0.05f)
				power = power && Near(FadeIn(p) * FadeIn(p) + FadeOut(p) * FadeOut(p), 1.0f);
			report.Check(power && FadeIn(0.0f) == 0.0f && Near(FadeIn(1.0f), 1.0f), "Audio: the crossfade legs are equal power");
			report.Check(Approach(0.0f, 1.0f, 0.5f, 2.0f) == 0.25f && Approach(0.9f, 1.0f, 5.0f, 1.0f) == 1.0f && Approach(0.2f, 0.0f, 5.0f, 1.0f) == 0.0f,
				"Audio: a level approaches its target at 1/seconds and never overshoots");
			report.Check(SurfaceFor(Ground::Grass) == StepSurface::Grass && SurfaceFor(Ground::Tilled) == StepSurface::Dirt
				&& SurfaceFor(Ground::Watered) == StepSurface::Dirt && SurfaceFor(Ground::Path) == StepSurface::Path,
				"Audio: footsteps follow the ground under the farmer");
		}

		void CheckMusicMixer(CheckReport& report)
		{
			MusicMixer mixer(Season::Spring);
			report.Check(mixer.GetLevels()[0] == 1.0f && mixer.GetLevels()[1] == 0.0f && mixer.GetLevels()[2] == 0.0f && mixer.GetLevels()[3] == 0.0f,
				"Audio: the music starts on the current season's loop alone");

			bool power = true;
			bool inside = true;
			float seconds = 0.0f;
			while (seconds < MUSIC_CROSSFADE_SECONDS * 0.5f)
			{
				mixer.Update(0.05f, Season::Summer);
				seconds += 0.05f;
				const std::array<float, 4>& levels = mixer.GetLevels();
				power = power && Near(levels[0] * levels[0] + levels[1] * levels[1], 1.0f, 1e-3f);
				inside = inside && levels[0] >= 0.0f && levels[1] <= 1.0f;
			}
			report.Check(power && inside, "Audio: a season change crossfades the two loops at constant power");
			report.Check(mixer.GetLevels()[0] > 0.0f && mixer.GetLevels()[1] > 0.0f && mixer.GetLevels()[0] < 1.0f && mixer.GetLevels()[1] < 1.0f,
				"Audio: halfway through the crossfade both loops are audible");

			for (int i = 0; i < static_cast<int>(MUSIC_CROSSFADE_SECONDS / 0.05f) + 5; ++i)
				mixer.Update(0.05f, Season::Summer);
			report.Check(mixer.GetLevels()[1] == 1.0f && mixer.GetLevels()[0] == 0.0f && mixer.GetTarget() == Season::Summer,
				"Audio: the crossfade ends on the new season's loop alone");
		}

		void CheckClocks(CheckReport& report)
		{
			FootstepClock clock(FOOTSTEP_STRIDE);
			int steps = 0;
			for (int i = 0; i < 100; ++i)
				steps += clock.Walk(0.1f) ? 1 : 0;
			report.Check(steps == static_cast<int>(10.0f / FOOTSTEP_STRIDE), std::format("Audio: ten metres walked make {} footsteps", steps));
			report.Check(!clock.Walk(0.0f), "Audio: standing still makes no footstep");

			CluckTimer first(7, CLUCK_INTERVAL_MIN, CLUCK_INTERVAL_MAX);
			CluckTimer second(7, CLUCK_INTERVAL_MIN, CLUCK_INTERVAL_MAX);
			int clucks = 0;
			bool same = true;
			bool spaced = true;
			float since = 0.0f;
			for (int i = 0; i < 6000; ++i)
			{
				const bool a = first.Update(0.1f);
				same = same && a == second.Update(0.1f);
				since += 0.1f;
				if (a)
				{
					++clucks;
					spaced = spaced && since >= CLUCK_INTERVAL_MIN - 0.15f;
					since = 0.0f;
				}
			}
			report.Check(same, "Audio: the cluck timer is deterministic for a seed");
			report.Check(spaced && clucks >= 600 / static_cast<int>(CLUCK_INTERVAL_MAX) && clucks <= 600 / static_cast<int>(CLUCK_INTERVAL_MIN) + 1,
				std::format("Audio: {} clucks in ten minutes, never closer than the minimum interval", clucks));
		}
	}

	void RunLookChecks(CheckReport& report)
	{
		SuiteScope scope(report, "Look");
		CheckNightCurve(report);
		CheckLampLod(report);
		CheckAudioMix(report);
		CheckMusicMixer(report);
		CheckClocks(report);
	}

}
