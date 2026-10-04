#include "Audio.h"
#include "GameTuning.h"

#include <algorithm>
#include <utility>

namespace Dingo
{

	GameAudio::GameAudio(std::shared_ptr<AudioClip> footstep)
		: m_Footstep(std::move(footstep))
	{}

	void GameAudio::PlayStep(const glm::vec3& position, bool left, float intensity, float scale) const
	{
		if (!m_Footstep)
			return;

		SoundAttenuation attenuation;
		attenuation.Model = AudioAttenuationModel::Linear;
		attenuation.MinDistance = AUDIO_STEP_NEAR;
		attenuation.MaxDistance = AUDIO_STEP_FAR;

		SoundPlayParams params;
		params.Volume = AUDIO_FOOTSTEP_VOLUME * glm::mix(AUDIO_FOOTSTEP_QUIET, 1.0f, std::clamp(intensity, 0.0f, 1.0f));
		params.Pitch = (left ? 1.0f : AUDIO_FOOTSTEP_PITCH_RIGHT) / std::max(scale, 0.1f);
		params.Spatialized = true;
		params.Position = position;
		params.Attenuation = attenuation;
		Application::Get().GetAudioEngine().Play(m_Footstep, params);
	}

}
