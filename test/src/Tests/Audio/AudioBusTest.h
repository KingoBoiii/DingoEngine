#pragma once
#include "Tests/Renderer2D/Renderer2DTest.h"

#include <memory>
#include <string>
#include <vector>

namespace Dingo
{

	// Audio buses. Start-up checks run against the application's AudioEngine (log and the
	// Properties panel); then a looping beep plays on a "Music" bus and the panel drives
	// Master, Music and SFX, for the ear check that a bus change reaches a sound already playing.
	class AudioBusTest : public Renderer2DTest
	{
	public:
		AudioBusTest(Renderer2D* renderer)
			: Renderer2DTest(renderer)
		{}
		virtual ~AudioBusTest() = default;

	public:
		virtual void Initialize() override;
		virtual void Update(float deltaTime) override;
		virtual void Cleanup() override;
		virtual void ImGuiRender() override;

	private:
		void RunChecks();
		void Check(bool condition, const std::string& name);
		void BusControls(const char* label, AudioBusId bus);

	private:
		std::shared_ptr<AudioClip> m_Clip;
		Font* m_Font = nullptr;
		AudioBusId m_Sfx = k_InvalidBus;
		AudioBusId m_Music = k_InvalidBus;
		AudioSoundId m_Loop = k_InvalidSound;
		float m_MasterVolume = 1.0f;

		struct CheckResult
		{
			std::string Name;
			bool Passed = false;
		};
		std::vector<CheckResult> m_Checks;
	};

}
