#pragma once
#include "Sim/Rng.h"
#include "Sim/SimTypes.h"
#include "World/TileGrid.h"

#include <array>
#include <cstddef>

namespace Dingo
{

	// What the game's sound does, as plain functions of the clock, season and weather: no device, no clip, so
	// --check runs them headless. Levels are linear gains 0..1 before the bus volumes.
	namespace AudioMix
	{
		struct AmbienceLevels
		{
			float Birds = 0.0f;
			float Crickets = 0.0f;
			float Wind = 0.0f;
			float Rain = 0.0f;
		};

		AmbienceLevels AmbienceTargets(float hours, Season season, Weather weather);

		// The music plays through the day only: it fades in at dawn and out at dusk.
		float MusicEnvelope(float hours);

		// Equal-power legs of a crossfade at progress 0..1: the two squares sum to 1.
		float FadeIn(float progress);
		float FadeOut(float progress);

		// Moves `current` toward `target` at 1 / `seconds` a second.
		float Approach(float current, float target, float dt, float seconds);

		enum class StepSurface : uint8_t { Grass, Dirt, Path, Count };
		StepSurface SurfaceFor(Ground ground);
	}

	// One loop per season; the season changing crossfades to the new one over MUSIC_CROSSFADE_SECONDS.
	class MusicMixer
	{
	public:
		explicit MusicMixer(Season start);

		void Update(float dt, Season target);

		// Per-season loop gain, before the day envelope.
		const std::array<float, 4>& GetLevels() const { return m_Level; }
		Season GetTarget() const { return m_Target; }

	private:
		std::array<float, 4> m_Progress{};
		std::array<float, 4> m_Level{};
		Season m_Target;
	};

	// Fires once per stride walked.
	class FootstepClock
	{
	public:
		explicit FootstepClock(float stride) : m_Stride(stride) {}

		bool Walk(float distance);
		void Reset() { m_Walked = 0.0f; }

	private:
		float m_Stride;
		float m_Walked = 0.0f;
	};

	// Fires at random intervals between the two bounds.
	class CluckTimer
	{
	public:
		CluckTimer(uint64_t seed, float shortest, float longest);

		bool Update(float dt);

	private:
		float NextInterval();

	private:
		Rng m_Random;
		float m_Shortest;
		float m_Longest;
		float m_Left;
	};

}
