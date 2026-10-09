#include "AudioBusTest.h"

#include <imgui.h>

#include <format>

namespace Dingo
{

	void AudioBusTest::Initialize()
	{
		Renderer2DTest::Initialize();

		m_Font = Font::Create("assets/fonts/arialbd.ttf");

		AudioEngine& audio = Application::Get().GetAudioEngine();
		m_MasterVolume = audio.GetMasterVolume();
		m_Checks.clear();
		m_Clip = audio.LoadClip("assets/audio/beep.wav");
		if (!m_Clip)
		{
			Check(false, "assets/audio/beep.wav loads");
			return;
		}

		RunChecks();

		m_Sfx = audio.CreateBus("Bus Test SFX");
		m_Music = audio.CreateBus("Bus Test Music");
		SoundPlayParams loop;
		loop.Looping = true;
		loop.Bus = m_Music;
		m_Loop = audio.Play(m_Clip, loop);
	}

	void AudioBusTest::RunChecks()
	{
		AudioEngine& audio = Application::Get().GetAudioEngine();
		const uint32_t buses = audio.GetBusCount();
		const uint32_t sounds = audio.GetActiveSoundCount();

		Check(audio.IsBusValid(k_MasterBus) && audio.FindBus("Master") == k_MasterBus, "the master bus is valid and named \"Master\"");
		Check(!audio.IsBusValid(k_InvalidBus), "k_InvalidBus is not a bus");

		const AudioBusId sfx = audio.CreateBus("Bus Check SFX");
		const AudioBusId ui = audio.CreateBus("Bus Check UI", sfx);
		const AudioBusId music = audio.CreateBus("Bus Check Music");
		Check(sfx != k_InvalidBus && ui != k_InvalidBus && music != k_InvalidBus && audio.GetBusCount() == buses + 3,
			std::format("CreateBus makes three buses ({} buses, was {})", audio.GetBusCount(), buses));
		Check(audio.FindBus("Bus Check UI") == ui, "FindBus finds a bus by name");
		Check(audio.CreateBus("Bus Check SFX") == sfx && audio.GetBusCount() == buses + 3, "CreateBus with a name in use returns that bus");
		Check(audio.CreateBus("") == k_InvalidBus, "CreateBus without a name fails");

		audio.SetBusVolume(sfx, 0.25f);
		Check(audio.GetBusVolume(sfx) == 0.25f, "SetBusVolume / GetBusVolume round-trip");
		audio.SetBusMuted(sfx, true);
		Check(audio.IsBusMuted(sfx) && audio.GetBusVolume(sfx) == 0.25f, "muting keeps the bus volume");
		audio.SetBusMuted(sfx, false);
		Check(!audio.IsBusMuted(sfx), "unmute");
		audio.PauseBus(sfx);
		Check(audio.IsBusPaused(sfx), "PauseBus");
		audio.ResumeBus(sfx);
		Check(!audio.IsBusPaused(sfx), "ResumeBus");

		audio.SetBusVolume(k_MasterBus, 0.5f);
		Check(audio.GetMasterVolume() == 0.5f && audio.GetBusVolume(k_MasterBus) == 0.5f, "the master bus volume is the master volume");
		audio.SetMasterVolume(m_MasterVolume);
		audio.SetBusMuted(k_MasterBus, true);
		Check(audio.IsBusMuted(k_MasterBus) && audio.GetMasterVolume() == m_MasterVolume, "muting the master bus keeps the master volume");
		audio.SetBusMuted(k_MasterBus, false);

		SoundPlayParams loop;
		loop.Looping = true;
		loop.Volume = 0.0f;
		loop.Bus = ui;
		const AudioSoundId onUi = audio.Play(m_Clip, loop);
		loop.Bus = music;
		const AudioSoundId onMusic = audio.Play(m_Clip, loop);
		audio.PlayOneShot(m_Clip, sfx, 0.0f);
		Check(audio.GetActiveSoundCount() == sounds + 3, std::format("three sounds play on the buses ({})", audio.GetActiveSoundCount() - sounds));

		audio.StopBus(sfx);
		Check(!audio.IsPlaying(onUi) && audio.IsPlaying(onMusic) && audio.GetActiveSoundCount() == sounds + 1,
			"StopBus stops the bus's sounds and its sub-bus's, not another bus's");
		Check(audio.IsBusValid(sfx) && audio.IsBusValid(ui), "StopBus keeps the buses");

		loop.Bus = ui;
		const AudioSoundId again = audio.Play(m_Clip, loop);
		audio.DestroyBus(sfx);
		Check(!audio.IsBusValid(sfx) && !audio.IsBusValid(ui) && !audio.IsPlaying(again) && audio.GetBusCount() == buses + 1,
			"DestroyBus takes its sub-buses and their sounds");

		audio.SetBusVolume(sfx, 0.9f);
		audio.SetBusMuted(sfx, true);
		audio.PauseBus(sfx);
		Check(audio.GetBusVolume(sfx) == 0.0f && !audio.IsBusMuted(sfx) && !audio.IsBusPaused(sfx), "a destroyed bus's id is stale: setters are no-ops");
		Check(audio.CreateBus("Bus Check Child", sfx) == k_InvalidBus, "CreateBus under a destroyed parent fails");
		audio.DestroyBus(k_MasterBus);
		Check(audio.IsBusValid(k_MasterBus), "the master bus can't be destroyed");

		loop.Bus = sfx;
		const AudioSoundId stale = audio.Play(m_Clip, loop);
		Check(audio.IsPlaying(stale), "a sound on a destroyed bus plays on the master bus");
		audio.Stop(stale);

		Scene scene("Audio Bus Test");
		Entity source = scene.CreateEntity("Source");
		AudioSourceComponent& component = source.AddComponent<AudioSourceComponent>();
		Check(component.Bus == k_MasterBus, "AudioSourceComponent::Bus defaults to the master bus");
		component.Clip = m_Clip;
		component.Looping = true;
		component.Volume = 0.0f;
		component.Spatialized = false;
		component.Bus = music;
		scene.PlayAudioSource(source);
		const AudioSoundId fromScene = scene.GetRuntimeSound(source);
		audio.StopBus(music);
		Check(fromScene != k_InvalidSound && !audio.IsPlaying(fromScene) && !audio.IsPlaying(onMusic),
			"an AudioSourceComponent plays on its Bus");

		audio.DestroyBus(music);
		Check(audio.GetBusCount() == buses && audio.GetActiveSoundCount() == sounds, "the checks leave no bus or sound behind");
	}

	void AudioBusTest::Check(bool condition, const std::string& name)
	{
		m_Checks.push_back({ name, condition });
		if (condition)
			DE_INFO("[PASS] {}", name);
		else
			DE_ERROR("[FAIL] {}", name);
	}

	void AudioBusTest::Update(float deltaTime)
	{
		const AudioEngine& audio = Application::Get().GetAudioEngine();

		m_Renderer->BeginScene(m_ProjectionViewMatrix);
		m_Renderer->Clear(m_ClearColor);
		if (m_Font)
		{
			auto busLine = [&audio](const char* name, AudioBusId bus)
			{
				return std::format("{}: volume {:.2f}{}{}", name, audio.GetBusVolume(bus),
					audio.IsBusMuted(bus) ? ", muted" : "", audio.IsBusPaused(bus) ? ", paused" : "");
			};
			const std::string lines[] = {
				std::format("Buses: {}, sounds: {}", audio.GetBusCount(), audio.GetActiveSoundCount()),
				busLine("Master", k_MasterBus),
				busLine("Music (looping beep)", m_Music),
				busLine("SFX (one-shots)", m_Sfx),
			};

			constexpr float textSize = 0.16f;
			constexpr float lineHeight = 0.26f;
			glm::vec2 position(-2.6f, 2.1f);
			for (const std::string& line : lines)
			{
				m_Renderer->DrawText(line, m_Font, position, textSize);
				position.y -= lineHeight;
			}
		}
		m_Renderer->EndScene();
	}

	void AudioBusTest::BusControls(const char* label, AudioBusId bus)
	{
		AudioEngine& audio = Application::Get().GetAudioEngine();

		ImGui::PushID(label);
		ImGui::TextUnformatted(label);
		float volume = audio.GetBusVolume(bus);
		if (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f))
			audio.SetBusVolume(bus, volume);
		bool muted = audio.IsBusMuted(bus);
		if (ImGui::Checkbox("Muted", &muted))
			audio.SetBusMuted(bus, muted);
		ImGui::SameLine();
		bool paused = audio.IsBusPaused(bus);
		if (ImGui::Checkbox("Paused", &paused))
		{
			if (paused)
				audio.PauseBus(bus);
			else
				audio.ResumeBus(bus);
		}
		ImGui::PopID();
	}

	void AudioBusTest::ImGuiRender()
	{
		Renderer2DTest::ImGuiRender();

		AudioEngine& audio = Application::Get().GetAudioEngine();

		ImGui::Separator();
		BusControls("Master", k_MasterBus);
		BusControls("Music", m_Music);
		if (ImGui::Button("Restart loop"))
		{
			audio.Stop(m_Loop);
			SoundPlayParams loop;
			loop.Looping = true;
			loop.Bus = m_Music;
			m_Loop = audio.Play(m_Clip, loop);
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop Music bus"))
			audio.StopBus(m_Music);
		BusControls("SFX", m_Sfx);
		if (ImGui::Button("One-shot on SFX"))
			audio.PlayOneShot(m_Clip, m_Sfx);

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

	void AudioBusTest::Cleanup()
	{
		AudioEngine& audio = Application::Get().GetAudioEngine();
		audio.DestroyBus(m_Sfx);
		audio.DestroyBus(m_Music);
		m_Sfx = k_InvalidBus;
		m_Music = k_InvalidBus;
		m_Loop = k_InvalidSound;
		audio.SetMasterVolume(m_MasterVolume);
		audio.SetBusMuted(k_MasterBus, false);
		audio.ResumeBus(k_MasterBus);
		m_Clip.reset();

		DestroyAndDelete(m_Font);
		m_Checks.clear();

		Renderer2DTest::Cleanup();
	}

}
