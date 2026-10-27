#pragma once
#include "Tests/Renderer2D/Renderer2DTest.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Dingo
{

	class MiniAudioEngine;

	// Microphone capture. Start-up checks open the default capture device (miniaudio's null
	// device on a machine without one), then close it and feed known frames through the device
	// thread's processing to check the ring buffer, reads, overruns, gain and the gate, then
	// switch devices, recover from a lost device and loop captured frames into a stream. After
	// that the default microphone runs with a meter and a gate light, for the ear check on a
	// real device.
	class AudioCaptureTest : public Renderer2DTest
	{
	public:
		AudioCaptureTest(Renderer2D* renderer)
			: Renderer2DTest(renderer)
		{}
		virtual ~AudioCaptureTest() = default;

	public:
		virtual void Initialize() override;
		virtual void Update(float deltaTime) override;
		virtual void Cleanup() override;
		virtual void ImGuiRender() override;

	private:
		void RunChecks(MiniAudioEngine& audio);
		void RunDeviceChecks(MiniAudioEngine& audio);
		void RunLoopbackChecks(MiniAudioEngine& audio);
		void Check(bool condition, const std::string& name);

	private:
		Font* m_Font = nullptr;
		std::vector<AudioCaptureDevice> m_Devices;
		bool m_DevicesListed = false;
		std::vector<float> m_Scratch;
		std::uint64_t m_FramesRead = 0;
		bool m_Monitor = false;
		AudioSoundId m_MonitorStream = k_InvalidSound;
		bool m_Gate = true;
		float m_Gain = 1.0f;
		float m_ThresholdDb = -50.0f;

		struct CheckResult
		{
			std::string Name;
			bool Passed = false;
		};
		std::vector<CheckResult> m_Checks;
	};

}
