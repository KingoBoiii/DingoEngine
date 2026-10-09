#pragma once

#include "DingoEngine/Audio/AudioTypes.h"

#include <glm/glm.hpp>

#include <filesystem>
#include <memory>
#include <string_view>

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
		// A relative filepath is looked up under the asset root first, then the working
		// directory.
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
		// The same, mixed into a bus.
		virtual void PlayOneShot(const std::shared_ptr<AudioClip>& clip, AudioBusId bus, float volume = 1.0f) = 0;
		virtual void PlayOneShot(const std::shared_ptr<AudioClip>& clip, const glm::vec3& position, AudioBusId bus, float volume = 1.0f) = 0;

		// --- Per-sound control (no-ops on an invalid / stale handle) ----------

		virtual void Stop(AudioSoundId sound) = 0;
		virtual void Pause(AudioSoundId sound) = 0;   // keeps the playback cursor
		virtual void Resume(AudioSoundId sound) = 0;  // continues from where Pause() left off
		virtual bool IsPlaying(AudioSoundId sound) const = 0;

		virtual void SetVolume(AudioSoundId sound, float volume) = 0;
		virtual void SetPitch(AudioSoundId sound, float pitch) = 0;
		virtual void SetLooping(AudioSoundId sound, bool looping) = 0;
		virtual void SetPosition(AudioSoundId sound, const glm::vec3& position) = 0;
		// Re-attenuates a live sound; no-op on an invalid / stale handle, same as above.
		virtual void SetAttenuation(AudioSoundId sound, const SoundAttenuation& attenuation) = 0;

		// --- Buses --------------------------------------------------------------

		// A bus mixes the sounds routed to it (SoundPlayParams::Bus) and its sub-buses,
		// then feeds its parent; k_MasterBus feeds the output. Its volume, mute and pause
		// reach every sound under it, already playing or not. Names are unique: creating
		// one that exists returns the existing bus (warned). Returns k_InvalidBus for an
		// empty name or a stale parent. Every call below is a no-op (getters: 0 / false)
		// on a stale id.
		virtual AudioBusId CreateBus(std::string_view name, AudioBusId parent = k_MasterBus) = 0;
		// Stops every sound on the bus and destroys its sub-buses with it. k_MasterBus
		// can't be destroyed.
		virtual void DestroyBus(AudioBusId bus) = 0;
		// k_InvalidBus when no live bus has that name. The master bus is "Master".
		virtual AudioBusId FindBus(std::string_view name) const = 0;
		virtual bool IsBusValid(AudioBusId bus) const = 0;
		// Live buses, not counting the master bus.
		virtual std::uint32_t GetBusCount() const = 0;

		// Linear gain. On k_MasterBus this is SetMasterVolume.
		virtual void SetBusVolume(AudioBusId bus, float volume) = 0;
		virtual float GetBusVolume(AudioBusId bus) const = 0;
		// Silences the bus without touching its volume, so unmuting restores the level.
		virtual void SetBusMuted(AudioBusId bus, bool muted) = 0;
		virtual bool IsBusMuted(AudioBusId bus) const = 0;
		// Freezes every sound under the bus where it is. Its sounds still report
		// IsPlaying, a sound started on a paused bus waits for ResumeBus, and finished
		// one-shots are reaped only after it.
		virtual void PauseBus(AudioBusId bus) = 0;
		virtual void ResumeBus(AudioBusId bus) = 0;
		virtual bool IsBusPaused(AudioBusId bus) const = 0;
		// Stops every sound on the bus and its sub-buses; the buses stay.
		virtual void StopBus(AudioBusId bus) = 0;

		// --- Global -----------------------------------------------------------

		// Master output gain, applied on top of every sound's own volume (1.0 = unchanged).
		virtual void SetMasterVolume(float volume) = 0;
		virtual float GetMasterVolume() const = 0;

		// Number of currently-active live sounds (playing or paused). For debug/stats
		// display only.
		virtual std::uint32_t GetActiveSoundCount() const = 0;

		// Used by every spatialized sound started afterwards without its own
		// SoundPlayParams::Attenuation, including the positional PlayOneShot.
		virtual void SetDefaultAttenuation(const SoundAttenuation& attenuation) = 0;
		virtual const SoundAttenuation& GetDefaultAttenuation() const = 0;

		// --- Listener (single listener; drives all spatialized sounds) --------

		virtual void SetListenerPosition(const glm::vec3& position) = 0;
		// Forward = the direction the listener faces; up = the listener's up vector.
		virtual void SetListenerOrientation(const glm::vec3& forward, const glm::vec3& up) = 0;

		template<typename T>
		T& As() { return static_cast<T&>(*this); }
	};

}
