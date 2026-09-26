#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <limits>
#include <optional>

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

	enum class AudioAttenuationModel : std::uint8_t
	{
		None,
		Inverse,     // 1/distance, clamped
		Linear,      // reaches MinGain at MaxDistance when Rolloff == 1; needs a finite MaxDistance
		Exponential,
	};

	// Distance falloff for a spatialized sound. The defaults are the backend's own, so a
	// sound that never sets one sounds exactly as it did before v0.6.2.
	struct SoundAttenuation
	{
		AudioAttenuationModel Model = AudioAttenuationModel::Inverse;
		float MinDistance = 1.0f;                              // full volume inside this radius
		float MaxDistance = (std::numeric_limits<float>::max)(); // parenthesized: <Windows.h> max macro
		float Rolloff = 1.0f;
		float MinGain = 0.0f; // floor, e.g. 0.3 keeps distant sounds audible
		float MaxGain = 1.0f;
	};

	// Describes how a sound should play. Position is only honoured when Spatialized is
	// true; otherwise the sound plays as a non-positional 2D sound (UI, music, etc.).
	struct SoundPlayParams
	{
		float Volume = 1.0f;    // linear gain, 1.0 = unchanged
		float Pitch = 1.0f;     // playback-rate multiplier, 1.0 = normal
		bool Looping = false;

		bool Spatialized = false;      // true = 3D positional audio via the listener
		glm::vec3 Position{ 0.0f };    // world-space source position (Spatialized only)

		// Per-sound attenuation override; nullopt = use the engine's current default
		// (AudioEngine::GetDefaultAttenuation). Ignored when Spatialized is false.
		std::optional<SoundAttenuation> Attenuation;

		SoundPlayParams() = default;
	};

}
