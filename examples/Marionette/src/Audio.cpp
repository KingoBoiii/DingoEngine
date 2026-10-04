#include "Audio.h"
#include "GameTuning.h"

#include <algorithm>
#include <utility>

namespace Dingo
{

	GameAudio::GameAudio(GameSounds sounds)
		: m_Sounds(std::move(sounds))
	{}

	void GameAudio::PlayAt(const std::shared_ptr<AudioClip>& clip, const glm::vec3& position, float volume, float pitch, float minDistance, float maxDistance) const
	{
		if (!clip)
			return;

		SoundAttenuation attenuation;
		attenuation.Model = AudioAttenuationModel::Linear;
		attenuation.MinDistance = minDistance;
		attenuation.MaxDistance = maxDistance;

		SoundPlayParams params;
		params.Volume = volume;
		params.Pitch = pitch;
		params.Spatialized = true;
		params.Position = position;
		params.Attenuation = attenuation;
		Application::Get().GetAudioEngine().Play(clip, params);
	}

	void GameAudio::PlayStep(const glm::vec3& position, bool left, float intensity, float scale) const
	{
		const float volume = AUDIO_FOOTSTEP_VOLUME * glm::mix(AUDIO_FOOTSTEP_QUIET, 1.0f, std::clamp(intensity, 0.0f, 1.0f));
		const float pitch = (left ? 1.0f : AUDIO_FOOTSTEP_PITCH_RIGHT) / std::max(scale, 0.1f);
		PlayAt(m_Sounds.Footstep, position, volume, pitch, AUDIO_STEP_NEAR, AUDIO_STEP_FAR);
	}

	void GameAudio::PlayCombat(CombatSound sound, const glm::vec3& position, float scale) const
	{
		const std::shared_ptr<AudioClip>* clip = &m_Sounds.Swing;
		float volume = AUDIO_SWING_VOLUME;
		switch (sound)
		{
			case CombatSound::Hit:   clip = &m_Sounds.Hit;   volume = AUDIO_HIT_VOLUME; break;
			case CombatSound::Block: clip = &m_Sounds.Block; volume = AUDIO_BLOCK_VOLUME; break;
			case CombatSound::Parry: clip = &m_Sounds.Parry; volume = AUDIO_PARRY_VOLUME; break;
			case CombatSound::Dodge: clip = &m_Sounds.Dodge; volume = AUDIO_DODGE_VOLUME; break;
			default: break;
		}
		PlayAt(*clip, position, volume, 1.0f / std::max(scale, 0.1f), AUDIO_COMBAT_NEAR, AUDIO_COMBAT_FAR);
	}

}
