#include "Audio/GameAudio.h"
#include "GameTuning.h"
#include "Ui/OptionsModel.h"
#include "World/ValleyLayout.h"

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

	constexpr SfxDef k_Sfx[] =
	{
		{ "audio/ui_click.wav",       SFX_VOLUME_UI,      0.0f, 0.0f },
		{ "audio/ui_focus.wav",       SFX_VOLUME_UI,      0.0f, 0.0f },
		{ "audio/ui_open.wav",        SFX_VOLUME_UI,      0.0f, 0.0f },
		{ "audio/ui_close.wav",       SFX_VOLUME_UI,      0.0f, 0.0f },
		{ "audio/ui_coin.wav",        SFX_VOLUME_UI,      0.0f, 0.0f },
		{ "audio/ui_error.wav",       SFX_VOLUME_UI,      0.0f, 0.0f },
		{ "audio/tool_hoe.wav",       SFX_VOLUME_TOOL,    SFX_NEAR, SFX_FAR },
		{ "audio/tool_water.wav",     SFX_VOLUME_TOOL,    SFX_NEAR, SFX_FAR },
		{ "audio/tool_axe.wav",       SFX_VOLUME_TOOL,    SFX_NEAR, SFX_FAR },
		{ "audio/tool_pick.wav",      SFX_VOLUME_TOOL,    SFX_NEAR, SFX_FAR },
		{ "audio/tool_scythe.wav",    SFX_VOLUME_TOOL,    SFX_NEAR, SFX_FAR },
		{ "audio/place.wav",          SFX_VOLUME_PLACE,   SFX_NEAR, SFX_FAR },
		{ "audio/remove.wav",         SFX_VOLUME_PLACE,   SFX_NEAR, SFX_FAR },
		{ "audio/plant.wav",          SFX_VOLUME_TOOL,    SFX_NEAR, SFX_FAR },
		{ "audio/harvest.wav",        SFX_VOLUME_TOOL,    SFX_NEAR, SFX_FAR },
		{ "audio/shop_bell.wav",      SFX_VOLUME_PLACE,   SFX_NEAR, SFX_FAR },
		{ "audio/bin_thunk.wav",      SFX_VOLUME_PLACE,   SFX_NEAR, SFX_FAR },
		{ "audio/footstep_grass.wav", SFX_VOLUME_STEP,    SFX_NEAR, SFX_STEP_FAR },
		{ "audio/footstep_dirt.wav",  SFX_VOLUME_STEP,    SFX_NEAR, SFX_STEP_FAR },
		{ "audio/footstep_path.wav",  SFX_VOLUME_STEP,    SFX_NEAR, SFX_STEP_FAR },
		{ "audio/chicken.wav",        SFX_VOLUME_CHICKEN, CHICKEN_NEAR, CHICKEN_FAR },
	};
	static_assert(std::size(k_Sfx) == static_cast<size_t>(Sfx::Count));

	constexpr const char* k_MusicPaths[] =
	{
		"audio/music_spring.wav", "audio/music_summer.wav", "audio/music_fall.wav", "audio/music_winter.wav",
	};
	constexpr const char* k_AmbiencePaths[] =
	{
		"audio/amb_birds.wav", "audio/amb_crickets.wav", "audio/amb_wind.wav", "audio/amb_rain.wav",
	};
	constexpr const char* k_PondPath = "audio/pond_loop.wav";

	SoundAttenuation Falloff(float full, float silent)
	{
		SoundAttenuation attenuation;
		attenuation.Model = AudioAttenuationModel::Linear;
		attenuation.MinDistance = full;
		attenuation.MaxDistance = silent;
		return attenuation;
	}

	AudioBusId EnsureBus(AudioEngine& audio, const char* name)
	{
		const AudioBusId found = audio.FindBus(name);
		return found != k_InvalidBus ? found : audio.CreateBus(name);
	}
}

namespace Dingo
{

	bool IsUiSfx(Sfx sfx)
	{
		return sfx <= Sfx::UiError;
	}

	GameAudio::GameAudio(Season season)
		: m_Mixer(season), m_Steps(FOOTSTEP_STRIDE), m_Cluck(0x68656172ull, CLUCK_INTERVAL_MIN, CLUCK_INTERVAL_MAX)
	{
		AudioEngine& audio = Application::Get().GetAudioEngine();
		m_BusMusic = EnsureBus(audio, k_BusMusic);
		m_BusAmbience = EnsureBus(audio, k_BusAmbience);
		m_BusSfx = EnsureBus(audio, k_BusSfx);
		m_BusUi = EnsureBus(audio, k_BusUi);

		size_t wanted = 0;
		size_t loaded = 0;
		const auto load = [&](const char* path)
		{
			++wanted;
			std::shared_ptr<AudioClip> clip = audio.LoadClip(path);
			if (clip)
				++loaded;
			return clip;
		};
		for (size_t i = 0; i < m_Clips.size(); ++i)
			m_Clips[i] = load(k_Sfx[i].Path);
		for (size_t i = 0; i < m_Music.size(); ++i)
			m_Music[i].Clip = load(k_MusicPaths[i]);
		for (size_t i = 0; i < m_Ambience.size(); ++i)
			m_Ambience[i].Clip = load(k_AmbiencePaths[i]);
		m_Pond.Clip = load(k_PondPath);
		DE_INFO("Hearthvale: audio ready, {} of {} clips loaded", loaded, wanted);

		m_Pond = StartLoop(m_Pond.Clip, m_BusAmbience, SFX_VOLUME_POND, true,
			glm::vec3((static_cast<float>(POND_CENTER_X) + 0.5f) * TILE_SIZE, GROUND_TOP, (static_cast<float>(POND_CENTER_Y) + 0.5f) * TILE_SIZE),
			Falloff(POND_NEAR, POND_FAR));
		for (Loop& loop : m_Music)
			loop = StartLoop(loop.Clip, m_BusMusic, 0.0f, false, glm::vec3(0.0f), {});
		for (Loop& loop : m_Ambience)
			loop = StartLoop(loop.Clip, m_BusAmbience, 0.0f, false, glm::vec3(0.0f), {});
	}

	GameAudio::Loop GameAudio::StartLoop(const std::shared_ptr<AudioClip>& clip, AudioBusId bus, float volume, bool spatial, const glm::vec3& position, const SoundAttenuation& falloff)
	{
		Loop loop;
		loop.Clip = clip;
		if (!clip)
			return loop;
		SoundPlayParams params;
		params.Volume = volume;
		params.Looping = true;
		params.Bus = bus;
		params.Spatialized = spatial;
		params.Position = position;
		if (spatial)
			params.Attenuation = falloff;
		loop.Sound = Application::Get().GetAudioEngine().Play(clip, params);
		return loop;
	}

	GameAudio::~GameAudio()
	{
		AudioEngine& audio = Application::Get().GetAudioEngine();
		for (Loop& loop : m_Music)
			audio.Stop(loop.Sound);
		for (Loop& loop : m_Ambience)
			audio.Stop(loop.Sound);
		audio.Stop(m_Pond.Sound);
	}

	void GameAudio::Update(const AudioFrame& frame)
	{
		AudioEngine& audio = Application::Get().GetAudioEngine();
		m_Listener = frame.Listener;

		m_Mixer.Update(frame.Dt, frame.SeasonNow);
		const float envelope = AudioMix::MusicEnvelope(frame.Hours);
		for (size_t i = 0; i < m_Music.size(); ++i)
			audio.SetVolume(m_Music[i].Sound, m_Mixer.GetLevels()[i] * envelope * MUSIC_VOLUME);

		const AudioMix::AmbienceLevels target = AudioMix::AmbienceTargets(frame.Hours, frame.SeasonNow, frame.WeatherNow);
		m_Levels.Birds = AudioMix::Approach(m_Levels.Birds, target.Birds, frame.Dt, AMBIENCE_FADE_SECONDS);
		m_Levels.Crickets = AudioMix::Approach(m_Levels.Crickets, target.Crickets, frame.Dt, AMBIENCE_FADE_SECONDS);
		m_Levels.Wind = AudioMix::Approach(m_Levels.Wind, target.Wind, frame.Dt, AMBIENCE_FADE_SECONDS);
		m_Levels.Rain = AudioMix::Approach(m_Levels.Rain, target.Rain, frame.Dt, AMBIENCE_FADE_SECONDS);
		audio.SetVolume(m_Ambience[0].Sound, m_Levels.Birds * AMBIENCE_VOLUME);
		audio.SetVolume(m_Ambience[1].Sound, m_Levels.Crickets * AMBIENCE_VOLUME);
		audio.SetVolume(m_Ambience[2].Sound, m_Levels.Wind * AMBIENCE_VOLUME);
		audio.SetVolume(m_Ambience[3].Sound, m_Levels.Rain * AMBIENCE_VOLUME);

		if (frame.Walked > 0.0f)
		{
			if (m_Steps.Walk(frame.Walked))
			{
				constexpr Sfx k_Steps[] = { Sfx::StepGrass, Sfx::StepDirt, Sfx::StepPath };
				PlayAt(k_Steps[static_cast<size_t>(AudioMix::SurfaceFor(frame.Under))], frame.Feet);
			}
		}
		else
		{
			m_Steps.Reset();
		}

		if (m_Cluck.Update(frame.Dt) && !frame.Chickens.empty())
		{
			const size_t pick = static_cast<size_t>(m_CluckPick++ % frame.Chickens.size());
			PlayAt(Sfx::Chicken, frame.Chickens[pick]);
		}
	}

	void GameAudio::Play(Sfx sfx) const
	{
		const size_t index = static_cast<size_t>(sfx);
		if (!m_Clips[index])
			return;
		Application::Get().GetAudioEngine().PlayOneShot(m_Clips[index], IsUiSfx(sfx) ? m_BusUi : m_BusSfx, k_Sfx[index].Volume);
	}

	void GameAudio::PlayAt(Sfx sfx, const glm::vec3& position) const
	{
		const size_t index = static_cast<size_t>(sfx);
		if (!m_Clips[index] || glm::distance(position, m_Listener) >= k_Sfx[index].Far)
			return;

		SoundPlayParams params;
		params.Volume = k_Sfx[index].Volume;
		params.Spatialized = true;
		params.Position = position;
		params.Attenuation = Falloff(k_Sfx[index].Near, k_Sfx[index].Far);
		params.Bus = m_BusSfx;
		(void)Application::Get().GetAudioEngine().Play(m_Clips[index], params);
	}

}
