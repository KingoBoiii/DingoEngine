#include "AudioCaptureTest.h"

#include "DingoEngine/Audio/MiniAudio/MiniAudioEngine.h"

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <format>

namespace Dingo
{

	namespace
	{

		MiniAudioEngine& GetMiniAudio()
		{
			return static_cast<MiniAudioEngine&>(Application::Get().GetAudioEngine());
		}

		void Feed(MiniAudioEngine& audio, float value, std::uint32_t frames, std::uint32_t channels = 1)
		{
			const std::vector<float> samples(static_cast<std::size_t>(frames) * channels, value);
			audio.FeedCaptureForTest(samples.data(), frames);
		}

		void Drain(AudioEngine& audio)
		{
			float sink[256];
			while (audio.ReadCaptured(sink, 128) > 0) {}
		}

		const char* StateName(AudioCaptureState state)
		{
			switch (state)
			{
				case AudioCaptureState::Running: return "Running";
				case AudioCaptureState::Lost: return "Lost";
				default: return "Stopped";
			}
		}

	}

	void AudioCaptureTest::Initialize()
	{
		Renderer2DTest::Initialize();

		m_Font = Font::Create("fonts/Inter-Bold.ttf");
		m_Checks.clear();

		MiniAudioEngine& audio = GetMiniAudio();
		RunChecks(audio);

		AudioCaptureDesc mic;
		mic.NoiseGate = m_Gate;
		mic.GateThresholdDb = m_ThresholdDb;
		if (!audio.StartCapture(mic))
			DE_WARN("Audio Capture Test: no microphone to show; the checks above still ran");
	}

	void AudioCaptureTest::RunChecks(MiniAudioEngine& audio)
	{
		AudioCaptureDesc bad;
		bad.Channels = 0;
		const bool noChannels = !audio.StartCapture(bad);
		bad.Channels = 3;
		const bool tooManyChannels = !audio.StartCapture(bad);
		bad = {};
		bad.SampleRate = 4000;
		const bool lowRate = !audio.StartCapture(bad);
		bad = {};
		bad.BufferFrames = 0;
		const bool noBuffer = !audio.StartCapture(bad);
		Check(noChannels && tooManyChannels && lowRate && noBuffer && audio.GetCaptureState() == AudioCaptureState::Stopped,
			"StartCapture with 0 or 3 channels, a 4000 Hz rate or no buffer fails and stays Stopped");

		float sink[16] = {};
		Check(audio.ReadCaptured(sink, 16) == 0 && audio.GetCapturedFrames() == 0 && !audio.SetCaptureDevice(std::string()),
			"before StartCapture nothing reads and SetCaptureDevice fails");

		const std::vector<AudioCaptureDevice> devices = audio.EnumerateCaptureDevices();
		Check(devices.empty() || devices.front().IsDefault || std::none_of(devices.begin(), devices.end(), [](const AudioCaptureDevice& d) { return d.IsDefault; }),
			std::format("EnumerateCaptureDevices lists {} device(s), the default first", devices.size()));
		for (const AudioCaptureDevice& device : devices)
			DE_INFO("Capture device: {}{}", device.Name, device.IsDefault ? " (default)" : "");

		AudioCaptureDesc desc;
		desc.BufferFrames = 1000;
		const bool started = audio.StartCapture(desc);
		if (!started)
		{
			if (!devices.empty())
				DE_WARN("StartCapture failed with {} device(s) listed (microphone access denied?); skipping the capture checks", devices.size());
			return;
		}
		Check(audio.GetCaptureState() == AudioCaptureState::Running && audio.GetCaptureDeviceId().empty(),
			"a started capture is Running on the system default");

		audio.DetachCaptureDeviceForTest();
		Drain(audio);

		Feed(audio, 0.25f, 600);
		std::vector<float> read(1000, 0.0f);
		const std::uint32_t count = audio.ReadCaptured(read.data(), 1000);
		Check(count == 600 && std::all_of(read.begin(), read.begin() + 600, [](float s) { return s == 0.25f; }) && audio.GetCapturedFrames() == 0,
			std::format("ReadCaptured returns the frames fed, oldest first ({} frames)", count));

		Feed(audio, 0.1f, 1500);
		Check(audio.GetCapturedFrames() == 1000 && audio.GetCaptureOverruns() == 500,
			std::format("a full ring drops new frames and counts them ({} queued, {} overruns)", audio.GetCapturedFrames(), audio.GetCaptureOverruns()));
		Drain(audio);

		Feed(audio, 0.5f, 4);
		Feed(audio, -1.5f, 4);
		std::int16_t pcm[8] = {};
		const std::uint32_t pcmCount = audio.ReadCaptured(pcm, 8);
		Check(pcmCount == 8 && pcm[0] == 16384 && pcm[7] == -32767,
			std::format("ReadCaptured into int16 scales and clamps ({}, {})", pcm[0], pcm[7]));

		audio.SetCaptureGain(2.0f);
		Feed(audio, 0.25f, 64);
		float gained = 0.0f;
		audio.ReadCaptured(&gained, 1);
		const float level = audio.GetCaptureLevel();
		audio.SetCaptureGain(100.0f);
		Drain(audio);
		Feed(audio, 0.1f, 64);
		float capped = 0.0f;
		audio.ReadCaptured(&capped, 1);
		audio.SetCaptureGain(1.0f);
		Drain(audio);
		Check(gained == 0.5f && level == 0.5f && std::abs(capped - 0.8f) < 1e-6f,
			std::format("gain applies before the read and the level, capped at 8 ({}, level {}, {})", gained, level, capped));

		audio.SetCaptureNoiseGate(true, -20.0f, 0.0f, 0.0f);
		Feed(audio, 0.05f, 64);
		float quiet = 1.0f;
		audio.ReadCaptured(&quiet, 1);
		const bool closed = !audio.IsCaptureGateOpen() && audio.GetCaptureLevel() == 0.0f;
		Drain(audio);
		Feed(audio, 0.5f, 64);
		float loud = 0.0f;
		audio.ReadCaptured(&loud, 1);
		const bool opened = audio.IsCaptureGateOpen();
		Drain(audio);
		Check(quiet == 0.0f && closed && loud == 0.5f && opened,
			"the gate silences input below its threshold and passes input above it");

		audio.SetCaptureNoiseGate(true, -20.0f, 10.0f, 0.0f);
		Feed(audio, 0.0f, 64);
		Drain(audio);
		Feed(audio, 0.5f, 960);
		std::vector<float> ramp(960, 0.0f);
		audio.ReadCaptured(ramp.data(), 960);
		const bool rising = std::is_sorted(ramp.begin(), ramp.end());
		Check(ramp[0] < 0.01f && ramp[240] > 0.2f && ramp[240] < 0.3f && ramp[959] == 0.5f && rising,
			std::format("a 10 ms attack fades the gate in over 480 frames ({:.3f}, {:.3f}, {:.3f})", ramp[0], ramp[240], ramp[959]));
		audio.SetCaptureNoiseGate(false, -50.0f, 5.0f, 150.0f);
		Drain(audio);

		RunDeviceChecks(audio);
		RunLoopbackChecks(audio);

		audio.StopCapture();
		Check(audio.GetCaptureState() == AudioCaptureState::Stopped && audio.GetCapturedFrames() == 0 && audio.ReadCaptured(sink, 16) == 0
			&& audio.GetCaptureOverruns() == 0 && audio.GetCaptureLevel() == 0.0f,
			"StopCapture closes the device and drops the queue");
		audio.StopCapture();
	}

	void AudioCaptureTest::RunDeviceChecks(MiniAudioEngine& audio)
	{
		const std::vector<AudioCaptureDevice> devices = audio.EnumerateCaptureDevices();
		if (devices.empty())
			return;

		Feed(audio, 0.3f, 100);
		const bool switched = audio.SetCaptureDevice(devices.back().Id);
		std::vector<float> kept(100, 0.0f);
		const std::uint32_t keptCount = audio.ReadCaptured(kept.data(), 100);
		Check(switched && keptCount == 100 && std::all_of(kept.begin(), kept.end(), [](float s) { return s == 0.3f; })
			&& audio.GetCaptureDeviceId() == devices.back().Id && audio.GetCaptureState() == AudioCaptureState::Running,
			std::format("SetCaptureDevice switches to '{}' and keeps the queued frames", devices.back().Name));

		const bool refused = !audio.SetCaptureDevice("not-a-device");
		Check(refused && audio.GetCaptureState() == AudioCaptureState::Running && audio.GetCaptureDeviceId() == devices.back().Id,
			"SetCaptureDevice with an unknown id fails and keeps the old device");

		audio.SimulateCaptureLossForTest();
		Check(audio.GetCaptureState() == AudioCaptureState::Running && audio.GetCaptureDeviceId().empty(),
			"a lost device falls back on the system default");

		audio.DetachCaptureDeviceForTest();
		Drain(audio);
	}

	void AudioCaptureTest::RunLoopbackChecks(MiniAudioEngine& audio)
	{
		std::vector<float> tone(480);
		for (std::size_t i = 0; i < tone.size(); ++i)
			tone[i] = 0.5f * std::sin(static_cast<float>(i) * 2.0f * 3.14159265f * 440.0f / 48000.0f);
		audio.FeedCaptureForTest(tone.data(), static_cast<std::uint32_t>(tone.size()));

		std::vector<float> captured(480, 0.0f);
		const std::uint32_t count = audio.ReadCaptured(captured.data(), 480);

		SoundPlayParams quiet;
		quiet.Volume = 0.0f;
		const AudioSoundId stream = audio.CreateStream(AudioStreamDesc(), quiet);
		const std::uint32_t pushed = audio.PushStreamFrames(stream, captured.data(), count);
		const std::uint32_t queued = audio.GetQueuedStreamFrames(stream);
		audio.Stop(stream);
		Check(count == 480 && captured == tone && pushed == 480 && queued <= 480 && stream != k_InvalidSound,
			std::format("captured frames loop into a CreateStream sound ({} read, {} pushed)", count, pushed));
	}

	void AudioCaptureTest::Check(bool condition, const std::string& name)
	{
		m_Checks.push_back({ name, condition });
		if (condition)
			DE_INFO("[PASS] {}", name);
		else
			DE_ERROR("[FAIL] {}", name);
	}

	void AudioCaptureTest::Update(float deltaTime)
	{
		AudioEngine& audio = Application::Get().GetAudioEngine();

		m_Scratch.resize(1024);
		while (std::uint32_t frames = audio.ReadCaptured(m_Scratch.data(), static_cast<std::uint32_t>(m_Scratch.size())))
		{
			m_FramesRead += frames;
			if (m_Monitor && m_MonitorStream != k_InvalidSound)
				audio.PushStreamFrames(m_MonitorStream, m_Scratch.data(), frames);
		}

		m_Renderer->BeginScene(m_ProjectionViewMatrix);
		m_Renderer->Clear(m_ClearColor);

		const float level = audio.GetCaptureLevel();
		const bool open = audio.IsCaptureGateOpen();
		m_Renderer->DrawQuad(glm::vec2(-2.6f + 2.0f * 0.5f, -0.6f), glm::vec2(2.0f, 0.2f), glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
		m_Renderer->DrawQuad(glm::vec2(-2.6f + 2.0f * level * 0.5f, -0.6f), glm::vec2(2.0f * level, 0.2f), glm::vec4(0.2f, 0.8f, 0.3f, 1.0f));
		m_Renderer->DrawCircle(glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(-0.3f, -0.6f, 0.0f)), glm::vec3(0.24f)),
			open ? glm::vec4(0.2f, 0.9f, 0.3f, 1.0f) : glm::vec4(0.5f, 0.1f, 0.1f, 1.0f));

		if (m_Font)
		{
			const std::string lines[] = {
				std::format("Capture: {}", StateName(audio.GetCaptureState())),
				std::format("Frames read: {}, overruns: {}", m_FramesRead, audio.GetCaptureOverruns()),
				std::format("Level {:.2f}, gate {}", level, open ? "open" : "closed"),
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

	void AudioCaptureTest::Cleanup()
	{
		AudioEngine& audio = Application::Get().GetAudioEngine();
		audio.StopCapture();
		audio.Stop(m_MonitorStream);
		m_MonitorStream = k_InvalidSound;
		delete m_Font;
		m_Font = nullptr;
		Renderer2DTest::Cleanup();
	}

	void AudioCaptureTest::ImGuiRender()
	{
		Renderer2DTest::ImGuiRender();

		AudioEngine& audio = Application::Get().GetAudioEngine();

		ImGui::Separator();
		ImGui::Text("Capture: %s", StateName(audio.GetCaptureState()));
		if (ImGui::SmallButton("Refresh devices") || !m_DevicesListed)
		{
			m_Devices = audio.EnumerateCaptureDevices();
			m_DevicesListed = true;
		}
		const std::string current = audio.GetCaptureDeviceId();
		for (const AudioCaptureDevice& device : m_Devices)
		{
			const std::string label = std::format("{}{}##{}", device.Name, device.IsDefault ? " (default)" : "", device.Id);
			if (ImGui::RadioButton(label.c_str(), device.Id == current))
			{
				if (audio.GetCaptureState() == AudioCaptureState::Stopped)
				{
					AudioCaptureDesc mic;
					mic.DeviceId = device.Id;
					audio.StartCapture(mic);
				}
				else
					audio.SetCaptureDevice(device.Id);
			}
		}
		if (ImGui::SliderFloat("Gain", &m_Gain, 0.0f, 8.0f))
			audio.SetCaptureGain(m_Gain);
		bool gateChanged = ImGui::Checkbox("Noise gate", &m_Gate);
		gateChanged |= ImGui::SliderFloat("Threshold (dB)", &m_ThresholdDb, -80.0f, 0.0f);
		if (gateChanged)
			audio.SetCaptureNoiseGate(m_Gate, m_ThresholdDb);
		if (ImGui::Checkbox("Monitor (use headphones)", &m_Monitor))
		{
			audio.Stop(m_MonitorStream);
			m_MonitorStream = m_Monitor ? audio.CreateStream(AudioStreamDesc()) : k_InvalidSound;
		}
		if (ImGui::Button(audio.GetCaptureState() == AudioCaptureState::Stopped ? "Start capture" : "Stop capture"))
		{
			if (audio.GetCaptureState() == AudioCaptureState::Stopped)
			{
				AudioCaptureDesc mic;
				mic.Gain = m_Gain;
				mic.NoiseGate = m_Gate;
				mic.GateThresholdDb = m_ThresholdDb;
				audio.StartCapture(mic);
			}
			else
				audio.StopCapture();
		}
	}

}
