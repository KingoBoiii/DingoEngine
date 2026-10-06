#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>

#include <array>
#include <memory>

namespace Dingo
{

	enum class Sfx
	{
		Footstep,
		WardenStep,
		Strike,
		Snuff,
		Flask,
		Alert,
		Caught,
		Ignite,
		Win,
		Count
	};

	// The Keep's sounds. Clips are decoded here and shared. Only the looping drone is stopped when this
	// dies; a one-shot still playing is safe, because the engine's sound slot holds its clip.
	class GameAudio
	{
	public:
		GameAudio();
		~GameAudio();

		GameAudio(const GameAudio&) = delete;
		GameAudio& operator=(const GameAudio&) = delete;

		void Play(Sfx sfx) const;
		// Skipped when the listener is already past the sound's falloff, so far-off wardens take no voice.
		void PlayAt(Sfx sfx, const glm::vec3& position) const;
		void SetListener(const glm::vec3& position) { m_Listener = position; }

		void StartDrone();
		void SetMuted(bool muted);

		const std::shared_ptr<AudioClip>& GetCrackle() const { return m_Crackle; }
		static SoundAttenuation CrackleFalloff();

	private:
		std::array<std::shared_ptr<AudioClip>, static_cast<size_t>(Sfx::Count)> m_Clips;
		std::shared_ptr<AudioClip> m_Crackle;
		std::shared_ptr<AudioClip> m_Drone;
		AudioSoundId m_DroneSound = k_InvalidSound;
		float m_MasterVolume = 1.0f;
		glm::vec3 m_Listener{ 0.0f };
	};

}
