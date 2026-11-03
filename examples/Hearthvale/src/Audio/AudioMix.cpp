#include "Audio/AudioMix.h"
#include "GameTuning.h"
#include "Look/NightCurve.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Dingo
{

	namespace
	{
		float SmoothStep(float edge0, float edge1, float x)
		{
			const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}
	}

	namespace AudioMix
	{

		AmbienceLevels AmbienceTargets(float hours, Season season, Weather weather)
		{
			const float dark = NightCurve::Darkness(hours);
			const bool clear = weather == Weather::Clear;

			AmbienceLevels levels;
			levels.Birds = clear ? (1.0f - dark) * (season == Season::Winter ? 0.35f : 1.0f) : 0.0f;
			levels.Crickets = clear ? dark * (season == Season::Winter ? 0.0f : season == Season::Fall ? 0.6f : 1.0f) : 0.0f;
			float wind = 0.25f;
			wind += season == Season::Winter ? 0.35f : season == Season::Fall ? 0.15f : 0.0f;
			wind += weather == Weather::Snow ? 0.3f : weather == Weather::Rain ? 0.1f : 0.0f;
			levels.Wind = std::min(wind, 1.0f);
			levels.Rain = weather == Weather::Rain ? 1.0f : 0.0f;
			return levels;
		}

		float MusicEnvelope(float hours)
		{
			return SmoothStep(MUSIC_FADE_IN_START_HOUR, MUSIC_FADE_IN_END_HOUR, hours) *
				(1.0f - SmoothStep(MUSIC_FADE_OUT_START_HOUR, MUSIC_FADE_OUT_END_HOUR, hours));
		}

		float FadeIn(float progress)
		{
			return std::sin(std::clamp(progress, 0.0f, 1.0f) * std::numbers::pi_v<float> * 0.5f);
		}

		float FadeOut(float progress)
		{
			return std::cos(std::clamp(progress, 0.0f, 1.0f) * std::numbers::pi_v<float> * 0.5f);
		}

		float Approach(float current, float target, float dt, float seconds)
		{
			const float step = seconds > 0.0f ? dt / seconds : 1.0f;
			if (current < target)
				return std::min(current + step, target);
			return std::max(current - step, target);
		}

		StepSurface SurfaceFor(Ground ground)
		{
			switch (ground)
			{
				case Ground::Tilled:
				case Ground::Watered: return StepSurface::Dirt;
				case Ground::Path:    return StepSurface::Path;
				default:              break;
			}
			return StepSurface::Grass;
		}

	}

	MusicMixer::MusicMixer(Season start)
		: m_Target(start)
	{
		m_Progress[static_cast<size_t>(start)] = 1.0f;
		m_Level[static_cast<size_t>(start)] = 1.0f;
	}

	void MusicMixer::Update(float dt, Season target)
	{
		m_Target = target;
		for (size_t i = 0; i < m_Progress.size(); ++i)
		{
			m_Progress[i] = AudioMix::Approach(m_Progress[i], i == static_cast<size_t>(target) ? 1.0f : 0.0f, dt, MUSIC_CROSSFADE_SECONDS);
			m_Level[i] = AudioMix::FadeIn(m_Progress[i]);
		}
	}

	bool FootstepClock::Walk(float distance)
	{
		m_Walked += distance;
		if (m_Walked < m_Stride)
			return false;
		m_Walked = std::fmod(m_Walked, m_Stride);
		return true;
	}

	CluckTimer::CluckTimer(uint64_t seed, float shortest, float longest)
		: m_Random(seed), m_Shortest(shortest), m_Longest(longest), m_Left(0.0f)
	{
		m_Left = NextInterval();
	}

	float CluckTimer::NextInterval()
	{
		const float t = static_cast<float>(m_Random.NextInt(0, 1000)) / 1000.0f;
		return m_Shortest + (m_Longest - m_Shortest) * t;
	}

	bool CluckTimer::Update(float dt)
	{
		m_Left -= dt;
		if (m_Left > 0.0f)
			return false;
		m_Left = NextInterval();
		return true;
	}

}
