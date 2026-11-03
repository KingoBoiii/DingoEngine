#pragma once

#include "DingoEngine/Audio/AudioTypes.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Dingo
{

	/// An opaque, fully-decoded audio clip loaded from disk. The concrete type lives in
	/// the backend (miniaudio) and never appears in this header - clients only ever hold
	/// a shared_ptr<AudioClip>, exactly like Physics3D bodies are opaque PhysicsBodyId3D
	/// handles. A clip is a shareable template: playing it produces independent sound
	/// instances that reference the same decoded data (see AudioEngine::Play).
	///
	/// Bound to the AudioEngine that created it; do not use a clip after its engine is
	/// shut down. Freed automatically when the last shared_ptr is released.
	///
	/// This is an opaque polymorphic base: it carries no data and no backend types. The
	/// backend derives the real clip from it, so a shared_ptr<AudioClip> deletes the
	/// concrete object correctly through the virtual destructor.
	class AudioClip
	{
	public:
		/// Frees the decoded data; reached by releasing the last shared_ptr, never called directly.
		virtual ~AudioClip() = default;

		/// The decoded length in PCM frames at the clip's own sample rate.
		virtual uint64_t GetFrameCount() const = 0;
	};

	/// Abstraction over an audio device + mixer. An AudioEngine owns the output device,
	/// the loaded clips' decoded data, and every live sound instance. The concrete
	/// backend (miniaudio) is selected by Create() and never appears in this header -
	/// exactly like Physics3D hides Jolt and GraphicsContext hides NVRHI.
	///
	/// Sounds are instances: LoadClip() once, then Play()/PlayOneShot() as many times as
	/// you like - each call produces an independent live sound. Play() returns an
	/// AudioSoundId handle for sounds you want to keep controlling; PlayOneShot() is
	/// fire-and-forget and returns nothing. Finished one-shots are reaped by Update().
	///
	/// The Application owns the one engine; reach it with Application::GetAudioEngine().
	class AudioEngine
	{
	public:
		/// Creates an engine with no device open; the Application brings it up. Use
		/// Application::GetAudioEngine() instead of making one.
		AudioEngine() = default;
		/// Stops every sound and closes the device; the Application deletes the engine.
		virtual ~AudioEngine() = default;

		/// Not copyable: the engine owns the device and every live sound.
		AudioEngine(const AudioEngine&) = delete;
		/// Not copyable: the engine owns the device and every live sound.
		AudioEngine& operator=(const AudioEngine&) = delete;

		/// True while the engine's audio device is open.
		virtual bool IsValid() const = 0;

		// --- Clips ------------------------------------------------------------

		/// Loads and fully decodes a clip (.wav / .ogg). Returns nullptr on failure
		/// (error is logged) - never a broken object, matching Model::LoadFromFile.
		/// A relative filepath is looked up under the asset root first, then the working
		/// directory. Also nullptr when the engine has no open device. Not cached: loading the
		/// same file twice decodes it twice; AssetManager::LoadAsync shares and caches clips.
		[[nodiscard]] virtual std::shared_ptr<AudioClip> LoadClip(const std::filesystem::path& filepath) = 0;

		// --- Playback ---------------------------------------------------------

		/// Plays a clip and returns a handle to the live sound (k_InvalidSound on
		/// failure / null clip). The handle stays valid until the sound is stopped or,
		/// for a non-looping sound, finishes on its own - after which it is reaped and
		/// the handle goes stale (setters below become no-ops).
		virtual AudioSoundId Play(const std::shared_ptr<AudioClip>& clip, const SoundPlayParams& params = {}) = 0;

		/// Fire-and-forget: plays a clip once with no returnable handle. Reaped
		/// automatically by Update() when it finishes. Cheapest way to play SFX.
		/// `volume` is a linear gain; it plays as a 2D sound on the master bus.
		virtual void PlayOneShot(const std::shared_ptr<AudioClip>& clip, float volume = 1.0f) = 0;
		/// Spatialized fire-and-forget one-shot at a world position.
		virtual void PlayOneShot(const std::shared_ptr<AudioClip>& clip, const glm::vec3& position, float volume = 1.0f) = 0;
		/// The same, mixed into a bus.
		virtual void PlayOneShot(const std::shared_ptr<AudioClip>& clip, AudioBusId bus, float volume = 1.0f) = 0;
		/// Spatialized at a world position and mixed into a bus.
		virtual void PlayOneShot(const std::shared_ptr<AudioClip>& clip, const glm::vec3& position, AudioBusId bus, float volume = 1.0f) = 0;

		// --- Streams -----------------------------------------------------------

		/// Starts a sound that plays whatever is pushed into it, with the usual play params
		/// (Looping is ignored). It never finishes on its own: when its buffer runs dry it fades
		/// to silence and stays alive, and it fades back in when frames arrive. Only Stop(),
		/// StopBus, DestroyBus or Shutdown end it. Once silent it resumes when one device period
		/// (in frames, counted at the stream's rate, capped at BufferFrames) is queued. Returns
		/// k_InvalidSound for BufferFrames 0, Channels 0 or above 254, or SampleRate outside
		/// 8000-384000.
		[[nodiscard]] virtual AudioSoundId CreateStream(const AudioStreamDesc& desc, const SoundPlayParams& params = {}) = 0;

		/// Appends interleaved signed 16-bit frames and returns how many were taken: fewer when the
		/// buffer is full, 0 for a handle that is no live stream. Call on the thread that drives
		/// the AudioEngine, like every other call; only the device thread's read is lock-free.
		/// Data from a network thread goes through the game's own queue.
		virtual std::uint32_t PushStreamFrames(AudioSoundId stream, const std::int16_t* frames, std::uint32_t frameCount) = 0;
		/// The same for float frames in [-1, 1].
		virtual std::uint32_t PushStreamFrames(AudioSoundId stream, const float* frames, std::uint32_t frameCount) = 0;

		/// Frames pushed but not yet played, for a jitter buffer. 0 for a handle that is no live
		/// stream.
		virtual std::uint32_t GetQueuedStreamFrames(AudioSoundId stream) const = 0;

		// --- Capture (one microphone per engine) -------------------------------

		/// The capture devices the OS reports now, default first. Empty when there are none or
		/// the engine has no device.
		virtual std::vector<AudioCaptureDevice> EnumerateCaptureDevices() const = 0;
		/// Opens the device and starts filling the ring buffer, replacing any capture already
		/// running. False, with a logged reason and capture Stopped, when the sample rate, channel
		/// count or buffer size is invalid, the device can't be opened or the OS denies microphone
		/// access; never throws and never waits on a prompt. Gain and gate values are clamped.
		virtual bool StartCapture(const AudioCaptureDesc& desc = {}) = 0;
		/// Stops and closes the device and drops the queued frames. No-op when not capturing.
		virtual void StopCapture() = 0;
		/// Stopped, Running, or Lost after the device went away and the default couldn't replace it.
		virtual AudioCaptureState GetCaptureState() const = 0;
		/// The id of the open device (empty for the system default) while Running or Lost.
		virtual std::string GetCaptureDeviceId() const = 0;

		/// Copies up to maxFrames interleaved frames (frames holds maxFrames * Channels samples),
		/// after gain and gate, oldest first, and returns how many. 0 unless Running. The device thread writes without a lock; call this on the
		/// thread that drives the AudioEngine.
		virtual std::uint32_t ReadCaptured(std::int16_t* frames, std::uint32_t maxFrames) = 0;
		/// The same as float frames in [-1, 1].
		virtual std::uint32_t ReadCaptured(float* frames, std::uint32_t maxFrames) = 0;
		/// Frames waiting to be read; 0 unless Running.
		virtual std::uint32_t GetCapturedFrames() const = 0;
		/// Frames dropped since StartCapture because the ring buffer was full (the reader is too slow).
		virtual std::uint64_t GetCaptureOverruns() const = 0;

		/// Switches device and keeps the ring buffer: frames already queued stay readable. False,
		/// with a logged reason, leaves the old device running. Empty means the system default.
		virtual bool SetCaptureDevice(const std::string& deviceId) = 0;
		/// Linear input gain, 0 to 8, applied from the next device period.
		virtual void SetCaptureGain(float gain) = 0;
		/// Turns the noise gate on or off and sets it, from the next device period.
		virtual void SetCaptureNoiseGate(bool enabled, float thresholdDb = -50.0f, float attackMs = 5.0f, float releaseMs = 150.0f) = 0;
		/// Loudest sample of the last device period, linear 0 to 1, after gain and gate: for a mic meter.
		virtual float GetCaptureLevel() const = 0;
		/// Whether the gate is open now: a "speaking" indicator. True with the gate off, false
		/// unless Running.
		virtual bool IsCaptureGateOpen() const = 0;

		// --- Per-sound control (no-ops on an invalid / stale handle) ----------

		/// Stops the sound for good and frees it; the handle goes stale. Use Pause to keep the
		/// cursor.
		virtual void Stop(AudioSoundId sound) = 0;
		virtual void Pause(AudioSoundId sound) = 0;   ///< Keeps the playback cursor.
		virtual void Resume(AudioSoundId sound) = 0;  ///< Continues from where Pause() left off.
		/// False for a paused sound, and for an invalid or stale handle.
		virtual bool IsPlaying(AudioSoundId sound) const = 0;

		/// Linear gain, 1.0 = unchanged.
		virtual void SetVolume(AudioSoundId sound, float volume) = 0;
		/// Playback-rate multiplier, 1.0 = normal.
		virtual void SetPitch(AudioSoundId sound, float pitch) = 0;
		/// A sound set looping plays until Stop; one set back to false ends at the clip's end.
		virtual void SetLooping(AudioSoundId sound, bool looping) = 0;
		/// World-space position, heard only by a sound started with SoundPlayParams::Spatialized.
		virtual void SetPosition(AudioSoundId sound, const glm::vec3& position) = 0;
		/// Re-attenuates a live sound; no-op on an invalid / stale handle, same as above.
		virtual void SetAttenuation(AudioSoundId sound, const SoundAttenuation& attenuation) = 0;

		// --- Buses --------------------------------------------------------------

		/// A bus mixes the sounds routed to it (SoundPlayParams::Bus) and its sub-buses,
		/// then feeds its parent; k_MasterBus feeds the output. Its volume, mute and pause
		/// reach every sound under it, already playing or not. Names are unique: creating
		/// one that exists returns the existing bus (warned). Returns k_InvalidBus for an
		/// empty name or a stale parent. Every call below is a no-op (getters: 0 / false)
		/// on a stale id.
		[[nodiscard]] virtual AudioBusId CreateBus(std::string_view name, AudioBusId parent = k_MasterBus) = 0;
		/// Stops every sound on the bus and destroys its sub-buses with it. k_MasterBus
		/// can't be destroyed.
		virtual void DestroyBus(AudioBusId bus) = 0;
		/// k_InvalidBus when no live bus has that name. The master bus is "Master".
		virtual AudioBusId FindBus(std::string_view name) const = 0;
		/// True while the bus exists; false once it is destroyed, or for k_InvalidBus.
		virtual bool IsBusValid(AudioBusId bus) const = 0;
		/// Live buses, not counting the master bus.
		virtual std::uint32_t GetBusCount() const = 0;

		/// Linear gain. On k_MasterBus this is SetMasterVolume.
		virtual void SetBusVolume(AudioBusId bus, float volume) = 0;
		/// The linear gain last set (1.0 for a new bus), 0 for a stale id.
		virtual float GetBusVolume(AudioBusId bus) const = 0;
		/// Silences the bus without touching its volume, so unmuting restores the level.
		virtual void SetBusMuted(AudioBusId bus, bool muted) = 0;
		/// Whether SetBusMuted silenced the bus.
		virtual bool IsBusMuted(AudioBusId bus) const = 0;
		/// Freezes every sound under the bus where it is. Its sounds still report
		/// IsPlaying, a sound started on a paused bus waits for ResumeBus, and finished
		/// one-shots are reaped only after it.
		virtual void PauseBus(AudioBusId bus) = 0;
		/// Lets the bus's sounds continue from where PauseBus froze them.
		virtual void ResumeBus(AudioBusId bus) = 0;
		/// Whether PauseBus froze the bus.
		virtual bool IsBusPaused(AudioBusId bus) const = 0;
		/// Stops every sound on the bus and its sub-buses; the buses stay.
		virtual void StopBus(AudioBusId bus) = 0;

		// --- Global -----------------------------------------------------------

		/// Master output gain, applied on top of every sound's own volume (1.0 = unchanged).
		virtual void SetMasterVolume(float volume) = 0;
		/// The master gain last set; the same value as GetBusVolume(k_MasterBus).
		virtual float GetMasterVolume() const = 0;

		/// Number of currently-active live sounds (playing or paused). For debug/stats
		/// display only.
		virtual std::uint32_t GetActiveSoundCount() const = 0;

		/// Used by every spatialized sound started afterwards without its own
		/// SoundPlayParams::Attenuation, including the positional PlayOneShot.
		virtual void SetDefaultAttenuation(const SoundAttenuation& attenuation) = 0;
		/// The attenuation SetDefaultAttenuation set; the backend's own until then.
		virtual const SoundAttenuation& GetDefaultAttenuation() const = 0;

		// --- Listener (single listener; drives all spatialized sounds) --------

		/// World-space position of the listener.
		virtual void SetListenerPosition(const glm::vec3& position) = 0;
		/// Forward = the direction the listener faces; up = the listener's up vector.
		virtual void SetListenerOrientation(const glm::vec3& forward, const glm::vec3& up) = 0;

	private:
		// The Application owns the one engine (GetAudioEngine) and drives its lifecycle.
		static AudioEngine* Create();

		// Brings up the audio device + mixer. No-op if already live.
		virtual void Initialize() = 0;
		// Stops every sound, frees clips owned by live sounds, and closes the device.
		// Safe to call when idle.
		virtual void Shutdown() = 0;
		// Reaps finished fire-and-forget one-shots, once a frame.
		virtual void Update() = 0;

		friend class Application;
	};

}
