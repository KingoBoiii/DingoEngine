#include "Audio.h"
#include "GameTuning.h"

#include <iterator>

namespace
{
	using namespace Dingo;

	struct SfxDef
	{
		const char* Path;
		float Volume;
		float Near;
		float Far;
	};

	// Near/Far only matter to PlayAt; Caught and Win are always played 2D.
	constexpr SfxDef k_Sfx[] =
	{
		{ "audio/footstep.wav",    AUDIO_FOOTSTEP_VOLUME,    AUDIO_STEP_NEAR,    AUDIO_STEP_FAR },
		{ "audio/warden_step.wav", AUDIO_WARDEN_STEP_VOLUME, AUDIO_WARDEN_NEAR,  AUDIO_WARDEN_FAR },
		{ "audio/strike.wav",      AUDIO_STRIKE_VOLUME,      AUDIO_STEP_NEAR,    AUDIO_STEP_FAR },
		{ "audio/snuff.wav",       AUDIO_SNUFF_VOLUME,       AUDIO_STEP_NEAR,    AUDIO_STEP_FAR },
		{ "audio/flask.wav",       AUDIO_FLASK_VOLUME,       AUDIO_STEP_NEAR,    AUDIO_STEP_FAR },
		{ "audio/alert.wav",       AUDIO_ALERT_VOLUME,       AUDIO_ALERT_NEAR,   AUDIO_ALERT_FAR },
		{ "audio/caught.wav",      AUDIO_CAUGHT_VOLUME,      0.0f,               0.0f },
		{ "audio/ignite.wav",      AUDIO_IGNITE_VOLUME,      AUDIO_IGNITE_NEAR,  AUDIO_IGNITE_FAR },
		{ "audio/win.wav",         AUDIO_WIN_VOLUME,         0.0f,               0.0f },
	};
	static_assert(std::size(k_Sfx) == static_cast<size_t>(Sfx::Count));

	constexpr const char* k_CracklePath = "audio/crackle_loop.wav";
	constexpr const char* k_DronePath = "audio/ambient_drone.wav";

	SoundAttenuation Falloff(float full, float silent)
	{
		SoundAttenuation attenuation;
		attenuation.Model = AudioAttenuationModel::Linear;
		attenuation.MinDistance = full;
		attenuation.MaxDistance = silent;
		return attenuation;
	}

	std::shared_ptr<AudioClip> Load(AudioEngine& audio, const char* path, size_t& loaded)
	{
		std::shared_ptr<AudioClip> clip = audio.LoadClip(path);
		if (clip)
			++loaded;
		else
			DE_ERROR("Candlewick: failed to load audio clip '{}'", path);
		return clip;
	}
}

namespace Dingo
{

	GameAudio::GameAudio()
	{
		AudioEngine& audio = Application::Get().GetAudioEngine();
		m_MasterVolume = audio.GetMasterVolume();

		size_t loaded = 0;
		for (size_t i = 0; i < m_Clips.size(); ++i)
			m_Clips[i] = Load(audio, k_Sfx[i].Path, loaded);
		m_Crackle = Load(audio, k_CracklePath, loaded);
		m_Drone = Load(audio, k_DronePath, loaded);

		DE_INFO("Candlewick: audio ready, {} of {} clips loaded", loaded, m_Clips.size() + 2);
	}

	GameAudio::~GameAudio()
	{
		AudioEngine& audio = Application::Get().GetAudioEngine();
		audio.Stop(m_DroneSound);
		audio.SetMasterVolume(m_MasterVolume);
	}

	void GameAudio::Play(Sfx sfx) const
	{
		const size_t index = static_cast<size_t>(sfx);
		Application::Get().GetAudioEngine().PlayOneShot(m_Clips[index], k_Sfx[index].Volume);
	}

	void GameAudio::PlayAt(Sfx sfx, const glm::vec3& position) const
	{
		const size_t index = static_cast<size_t>(sfx);
		if (glm::distance(position, m_Listener) >= k_Sfx[index].Far)
			return;

		SoundPlayParams params;
		params.Volume = k_Sfx[index].Volume;
		params.Spatialized = true;
		params.Position = position;
		params.Attenuation = Falloff(k_Sfx[index].Near, k_Sfx[index].Far);
		Application::Get().GetAudioEngine().Play(m_Clips[index], params);
	}

	void GameAudio::StartDrone()
	{
		SoundPlayParams params;
		params.Volume = AUDIO_DRONE_VOLUME;
		params.Looping = true;
		m_DroneSound = Application::Get().GetAudioEngine().Play(m_Drone, params);
	}

	void GameAudio::SetMuted(bool muted)
	{
		Application::Get().GetAudioEngine().SetMasterVolume(muted ? 0.0f : m_MasterVolume);
	}

	SoundAttenuation GameAudio::CrackleFalloff()
	{
		return Falloff(AUDIO_CRACKLE_NEAR, AUDIO_CRACKLE_FAR);
	}

}
