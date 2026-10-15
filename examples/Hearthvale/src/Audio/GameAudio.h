#pragma once
#include <DingoEngine.h>

#include "Audio/AudioMix.h"

#include <array>
#include <memory>
#include <vector>

namespace Dingo
{

	enum class Sfx : uint8_t
	{
		UiClick, UiFocus, UiOpen, UiClose, UiCoin, UiError,
		ToolHoe, ToolWater, ToolAxe, ToolPick, ToolScythe,
		Place, Remove, Plant, Harvest, ShopBell, BinThunk,
		StepGrass, StepDirt, StepPath, Chicken,
		Count
	};

	bool IsUiSfx(Sfx sfx);

	struct AudioFrame
	{
		float Dt = 0.0f;
		float Hours = 12.0f;
		Season SeasonNow = Season::Spring;
		Weather WeatherNow = Weather::Clear;
		glm::vec3 Listener{ 0.0f };
		glm::vec3 Feet{ 0.0f };
		float Walked = 0.0f;
		Ground Under = Ground::Grass;
		std::vector<glm::vec3> Chickens;
	};

	// The game's mix: the four option buses, the season music, the ambience layers, the pond, and the one-shots.
	// A clip that failed to load is logged once and its sound skipped; a machine with no audio device plays nothing.
	// Device side: never constructed headless.
	class GameAudio
	{
	public:
		explicit GameAudio(Season season);
		~GameAudio();
		GameAudio(const GameAudio&) = delete;
		GameAudio& operator=(const GameAudio&) = delete;

		void Update(const AudioFrame& frame);

		// 2D on the UI bus for the UI sounds, else on the SFX bus.
		void Play(Sfx sfx) const;
		void PlayAt(Sfx sfx, const glm::vec3& position) const;

	private:
		struct Loop
		{
			std::shared_ptr<AudioClip> Clip;
			AudioSoundId Sound = k_InvalidSound;
		};

		static Loop StartLoop(const std::shared_ptr<AudioClip>& clip, AudioBusId bus, float volume, bool spatial, const glm::vec3& position, const SoundAttenuation& falloff);

	private:
		std::array<std::shared_ptr<AudioClip>, static_cast<size_t>(Sfx::Count)> m_Clips;
		std::array<Loop, 4> m_Music;
		std::array<Loop, 4> m_Ambience;
		Loop m_Pond;
		MusicMixer m_Mixer;
		AudioMix::AmbienceLevels m_Levels;
		FootstepClock m_Steps;
		CluckTimer m_Cluck;
		glm::vec3 m_Listener{ 0.0f };
		uint32_t m_CluckPick = 0;
		AudioBusId m_BusMusic = k_InvalidBus;
		AudioBusId m_BusAmbience = k_InvalidBus;
		AudioBusId m_BusSfx = k_InvalidBus;
		AudioBusId m_BusUi = k_InvalidBus;
	};

}
