#pragma once
#include "ValleyDirector.h"

#include <cstdint>

namespace Dingo
{

	enum class FadePhase : uint8_t { Idle, Out, Hold, In };

	// Sleep and pass-out as a screen fade (plain C++; the overlay that draws GetAlpha is GPU-side). Sleeping fades to
	// black, requests the day roll at full black, holds, then fades back in. A pass-out rolls the day on its own at 2:00,
	// so the screen cuts to black and fades in. Time is paused for the whole fade, then restored.
	class SleepFlow
	{
	public:
		explicit SleepFlow(ValleyDirector& director);
		SleepFlow(const SleepFlow&) = delete;
		SleepFlow& operator=(const SleepFlow&) = delete;

		// False while a fade is already running.
		bool RequestSleep();

		FadePhase GetPhase() const { return m_Phase; }
		bool BlocksInput() const { return m_Phase != FadePhase::Idle; }
		// 0 = clear, 1 = black.
		float GetAlpha() const;

	private:
		void Tick(float dt);
		void OnRolled(DayRollReason reason);
		void Begin(FadePhase phase);

	private:
		ValleyDirector& m_Director;
		FadePhase m_Phase = FadePhase::Idle;
		float m_Time = 0.0f;
		bool m_WasPaused = false;
	};

}
