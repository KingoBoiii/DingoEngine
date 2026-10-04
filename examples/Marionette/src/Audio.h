#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>

#include <memory>

namespace Dingo
{

	// The bout's sounds. A one-shot still playing when this dies is safe: the engine's sound slot
	// holds its clip.
	class GameAudio
	{
	public:
		explicit GameAudio(std::shared_ptr<AudioClip> footstep);

		GameAudio(const GameAudio&) = delete;
		GameAudio& operator=(const GameAudio&) = delete;

		// `intensity` is 0 to 1 (standing to full run) and `scale` the fighter's: a bigger one steps lower.
		void PlayStep(const glm::vec3& position, bool left, float intensity, float scale) const;

	private:
		std::shared_ptr<AudioClip> m_Footstep;
	};

}
