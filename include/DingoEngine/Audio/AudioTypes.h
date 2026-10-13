#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <limits>
#include <optional>

namespace Dingo
{

	/// Opaque handle to a live sound instance inside an AudioEngine. No audio-backend
	/// type (miniaudio) ever appears in the public API; this is all the client holds.
	///
	/// A handle encodes a slot index plus a generation counter, so a handle to a sound
	/// that has since finished/been reaped will not accidentally control a newer sound
	/// that reused the slot (the generation won't match). See AudioEngine implementation.
	enum class AudioSoundId : std::uint32_t {};
	/// The null sound handle: AudioEngine::Play returns it on failure; control calls ignore it.
	inline constexpr AudioSoundId k_InvalidSound{ 0xFFFFFFFFu };

	/// Handle to a mix bus (AudioEngine::CreateBus), packed like AudioSoundId, so a
	/// destroyed bus's id goes stale. A strong type: a plain integer would make the
	/// PlayOneShot bus overloads ambiguous with the float-volume ones. k_MasterBus is the
	/// engine's root bus, always valid while the engine is.
	enum class AudioBusId : std::uint32_t {};
	/// The root bus every other bus feeds; it feeds the output and is never destroyed.
	inline constexpr AudioBusId k_MasterBus{ 0u };
	/// The null bus handle: CreateBus returns it on failure, FindBus when no bus has the name.
	inline constexpr AudioBusId k_InvalidBus{ 0xFFFFFFFFu };

	/// How a spatialized sound's volume falls off with its distance from the listener.
	enum class AudioAttenuationModel : std::uint8_t
	{
		None,        ///< No distance falloff; still panned from Position.
		Inverse,     ///< 1/distance, clamped.
		Linear,      ///< Hits MinGain at MaxDistance if Rolloff is 1; needs a finite MaxDistance.
		Exponential, ///< Falls off exponentially with distance, shaped by Rolloff.
	};

	/// Distance falloff for a spatialized sound. The defaults are the backend's own, so a
	/// sound that never sets one sounds exactly as it did before v0.6.2.
	struct SoundAttenuation
	{
		AudioAttenuationModel Model = AudioAttenuationModel::Inverse;	///< The falloff curve.
		float MinDistance = 1.0f; ///< Full volume inside this radius, in world units.
		/// Distance where falloff stops, in world units. Parenthesized: <Windows.h> max macro.
		float MaxDistance = (std::numeric_limits<float>::max)();
		float Rolloff = 1.0f;	///< How quickly the volume falls; 1 is the model's natural rate.
		float MinGain = 0.0f; ///< Floor, e.g. 0.3 keeps distant sounds audible.
		float MaxGain = 1.0f;	///< Ceiling on the linear gain the falloff can give.
	};

	/// Describes how a sound should play. Position is only honoured when Spatialized is
	/// true; otherwise the sound plays as a non-positional 2D sound (UI, music, etc.).
	struct SoundPlayParams
	{
		float Volume = 1.0f;    ///< Linear gain, 1.0 = unchanged.
		float Pitch = 1.0f;     ///< Playback-rate multiplier, 1.0 = normal.
		bool Looping = false;	///< Plays until Stop; a looping sound is never reaped.

		bool Spatialized = false;      ///< True = 3D positional audio via the listener.
		glm::vec3 Position{ 0.0f };    ///< World-space source position (Spatialized only).

		/// Per-sound attenuation override; nullopt = use the engine's current default
		/// (AudioEngine::GetDefaultAttenuation). Ignored when Spatialized is false.
		std::optional<SoundAttenuation> Attenuation;

		/// The bus the sound mixes into. A stale bus plays on k_MasterBus (warned once).
		AudioBusId Bus = k_MasterBus;

		/// Plays at full volume, normal pitch, once, as a 2D sound on the master bus.
		SoundPlayParams() = default;
	};

}
