#pragma once

#include "DingoEngine/Audio/AudioTypes.h"

#include <glm/glm.hpp>

#include <filesystem>
#include <memory>

namespace Dingo
{

	// An opaque, fully-decoded audio clip loaded from disk. The concrete type lives in
	// the backend (miniaudio) and never appears in this header — clients only ever hold
	// a shared_ptr<AudioClip>, exactly like Physics3D bodies are opaque PhysicsBodyId3D
	// handles. A clip is a shareable template: playing it produces independent sound
	// instances that reference the same decoded data (see AudioEngine::Play).
	//
	// Bound to the AudioEngine that created it; do not use a clip after its engine is
	// shut down. Freed automatically when the last shared_ptr is released.
	//
	// This is an opaque polymorphic base: it carries no data and no backend types. The
	// backend derives the real clip from it, so a shared_ptr<AudioClip> deletes the
	// concrete object correctly through the virtual destructor.
	class AudioClip
	{
	public:
		virtual ~AudioClip() = default;
	};

	// Abstraction over an audio device + mixer. An AudioEngine owns the output device,
	// the loaded clips' decoded data, and every live sound instance. The concrete
	// backend (miniaudio) is selected by Create() and never appears in this header —
	// exactly like Physics3D hides Jolt and GraphicsContext hides NVRHI.
	//
	// Sounds are instances: LoadClip() once, then Play()/PlayOneShot() as many times as
	// you like — each call produces an independent live sound. Play() returns an
	// AudioSoundId handle for sounds you want to keep controlling; PlayOneShot() is
	// fire-and-forget and returns nothing. Finished one-shots are reaped by Update().
	class AudioEngine
	{
	public:
		// Creates the default audio backend (currently miniaudio). Not live until
		// Initialize() is called. Mirrors Physics3D::Create / GraphicsContext::Create.
		static AudioEngine* Create();

	public:
		AudioEngine() = default;
		virtual ~AudioEngine() = default;

		AudioEngine(const AudioEngine&) = delete;
		AudioEngine& operator=(const AudioEngine&) = delete;

		// --- Lifecycle --------------------------------------------------------

		// Brings up the audio device + mixer. No-op if already live.
		virtual void Initialize() = 0;
		// Stops every sound, frees clips owned by live sounds, and closes the device.
		// Safe to call when idle.
		virtual void Shutdown() = 0;
		// True between Initialize() and Shutdown().
		virtual bool IsValid() const = 0;

		// Reaps finished fire-and-forget one-shots. Cheap to call once per frame; the
		// backend has its own device thread, so this only recycles handles/slots.
		virtual void Update() = 0;

		// --- Clips ------------------------------------------------------------

		// Loads and fully decodes a clip (.wav / .ogg). Returns nullptr on failure
		// (error is logged) — never a broken object, matching Model::LoadFromFile.
		virtual std::shared_ptr<AudioClip> LoadClip(const std::filesystem::path& filepath) = 0;

		// --- Playback ---------------------------------------------------------

		// Plays a clip and returns a handle to the live sound (k_InvalidSound on
		// failure / null clip). The handle stays valid until the sound is stopped or,
		// for a non-looping sound, finishes on its own — after which it is reaped and
		// the handle goes stale (setters below become no-ops).
		virtual AudioSoundId Play(const std::shared_ptr<AudioClip>& clip, const SoundPlayParams& params = {}) = 0;

		// Fire-and-forget: plays a clip once with no returnable handle. Reaped
		// automatically by Update() when it finishes. Cheapest way to play SFX.
		virtual void PlayOneShot(const std::shared_ptr<AudioClip>& clip, float volume = 1.0f) = 0;
		// Spatialized fire-and-forget one-shot at a world position.
		virtual void PlayOneShot(const std::shared_ptr<AudioClip>& clip, const glm::vec3& position, float volume = 1.0f) = 0;

		// --- Per-sound control (no-ops on an invalid / stale handle) ----------

		virtual void Stop(AudioSoundId sound) = 0;
		virtual void Pause(AudioSoundId sound) = 0;   // keeps the playback cursor
		virtual void Resume(AudioSoundId sound) = 0;  // continues from where Pause() left off
		virtual bool IsPlaying(AudioSoundId sound) const = 0;

		virtual void SetVolume(AudioSoundId sound, float volume) = 0;
		virtual void SetPitch(AudioSoundId sound, float pitch) = 0;
		virtual void SetLooping(AudioSoundId sound, bool looping) = 0;
		virtual void SetPosition(AudioSoundId sound, const glm::vec3& position) = 0;

		// --- Global -----------------------------------------------------------

		// Master output gain, applied on top of every sound's own volume (1.0 = unchanged).
		virtual void SetMasterVolume(float volume) = 0;
		virtual float GetMasterVolume() const = 0;

		// Number of currently-active live sounds (playing or paused). For debug/stats
		// display only.
		virtual std::uint32_t GetActiveSoundCount() const = 0;

		// --- Listener (single listener; drives all spatialized sounds) --------

		virtual void SetListenerPosition(const glm::vec3& position) = 0;
		// Forward = the direction the listener faces; up = the listener's up vector.
		virtual void SetListenerOrientation(const glm::vec3& forward, const glm::vec3& up) = 0;

		template<typename T>
		T& As() { return static_cast<T&>(*this); }
	};

}
