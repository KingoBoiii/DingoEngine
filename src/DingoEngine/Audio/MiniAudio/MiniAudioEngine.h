#pragma once

#include "DingoEngine/Audio/AudioEngine.h"

namespace Dingo
{

	namespace Internal { struct MiniAudioData; }

	// miniaudio implementation of the AudioEngine interface. miniaudio is fully hidden
	// behind an opaque pointer (Internal::MiniAudioData) — no miniaudio type appears in
	// this header; every ma_* type and call is confined to MiniAudioEngine.cpp and the
	// engine-internal MiniAudioData.h. This is the audio counterpart to JoltPhysics3D.
	class MiniAudioEngine final : public AudioEngine
	{
	public:
		MiniAudioEngine() = default;
		~MiniAudioEngine() override;

		void Initialize() override;
		void Shutdown() override;
		bool IsValid() const override;
		void Update() override;

		std::shared_ptr<AudioClip> LoadClip(const std::filesystem::path& filepath) override;

		AudioSoundId Play(const std::shared_ptr<AudioClip>& clip, const SoundPlayParams& params) override;
		void PlayOneShot(const std::shared_ptr<AudioClip>& clip, float volume) override;
		void PlayOneShot(const std::shared_ptr<AudioClip>& clip, const glm::vec3& position, float volume) override;

		void Stop(AudioSoundId sound) override;
		void Pause(AudioSoundId sound) override;
		void Resume(AudioSoundId sound) override;
		bool IsPlaying(AudioSoundId sound) const override;

		void SetVolume(AudioSoundId sound, float volume) override;
		void SetPitch(AudioSoundId sound, float pitch) override;
		void SetLooping(AudioSoundId sound, bool looping) override;
		void SetPosition(AudioSoundId sound, const glm::vec3& position) override;

		void SetMasterVolume(float volume) override;
		float GetMasterVolume() const override;
		std::uint32_t GetActiveSoundCount() const override;

		void SetListenerPosition(const glm::vec3& position) override;
		void SetListenerOrientation(const glm::vec3& forward, const glm::vec3& up) override;

	private:
		Internal::MiniAudioData* m_Data = nullptr;
	};

}
