#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>

namespace Dingo
{

	struct GameSounds
	{
		std::shared_ptr<AudioClip> Footstep;
		std::shared_ptr<AudioClip> Swing;
		std::shared_ptr<AudioClip> Hit;
		std::shared_ptr<AudioClip> Block;
		std::shared_ptr<AudioClip> Parry;
		std::shared_ptr<AudioClip> Dodge;
		std::shared_ptr<AudioClip> Ko;
		std::shared_ptr<AudioClip> Win;
		std::shared_ptr<AudioClip> Lose;
		std::shared_ptr<AudioClip> Crackle;
	};

	enum class CombatSound : uint8_t
	{
		Swing,
		Hit,
		Block,
		Parry,
		Dodge
	};

	enum class Sting : uint8_t
	{
		Ko,
		Win,
		Lose
	};

	// The bout's sounds. A one-shot still playing when this dies is safe: the engine's sound slot
	// holds its clip.
	class GameAudio
	{
	public:
		explicit GameAudio(GameSounds sounds);

		GameAudio(const GameAudio&) = delete;
		GameAudio& operator=(const GameAudio&) = delete;

		// `intensity` is 0 to 1 (standing to full run) and `scale` the fighter's: a bigger one steps lower.
		void PlayStep(const glm::vec3& position, bool left, float intensity, float scale) const;
		void PlayCombat(CombatSound sound, const glm::vec3& position, float scale) const;
		// Not positional: the bout's own cues.
		void PlaySting(Sting sting) const;
		// A looping brazier crackle at a position; the caller stops it. k_InvalidSound when there is none to play.
		AudioSoundId StartCrackle(const glm::vec3& position, float pitch) const;

		// A run that steps faster than real time would play every sound over the next.
		void SetMuted(bool muted) { m_Muted = muted; }

	private:
		void PlayAt(const std::shared_ptr<AudioClip>& clip, const glm::vec3& position, float volume, float pitch, float minDistance, float maxDistance) const;

	private:
		GameSounds m_Sounds;
		bool m_Muted = false;
	};

}
