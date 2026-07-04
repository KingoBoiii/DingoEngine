#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace Dingo
{

	// Opaque handle to a live sound instance inside an AudioEngine. No audio-backend
	// type (miniaudio) ever appears in the public API; this is all the client holds.
	//
	// A handle encodes a slot index plus a generation counter, so a handle to a sound
	// that has since finished/been reaped will not accidentally control a newer sound
	// that reused the slot (the generation won't match). See AudioEngine implementation.
	using AudioSoundId = std::uint32_t;
	inline constexpr AudioSoundId k_InvalidSound = 0xFFFFFFFFu;

	// Describes how a sound should play. Position is only honoured when Spatialized is
	// true; otherwise the sound plays as a non-positional 2D sound (UI, music, etc.).
	struct SoundPlayParams
	{
		float Volume = 1.0f;    // linear gain, 1.0 = unchanged
		float Pitch = 1.0f;     // playback-rate multiplier, 1.0 = normal
		bool Looping = false;

		bool Spatialized = false;      // true = 3D positional audio via the listener
		glm::vec3 Position{ 0.0f };    // world-space source position (Spatialized only)

		SoundPlayParams() = default;
	};

}
