#include "Farm/SleepFlow.h"
#include "GameTuning.h"

#include <algorithm>

namespace Dingo
{

	SleepFlow::SleepFlow(ValleyDirector& director)
		: m_Director(director)
	{
		m_Director.OnStage(SimStage::Input, [this](ValleyDirector&, const StepInfo& step)
		{
			Tick(step.Dt);
		});
		m_Director.OnDayRoll(DayRollStep::Autosave, [this](ValleyDirector&, DayRollReason reason)
		{
			OnRolled(reason);
		});
	}

	bool SleepFlow::RequestSleep()
	{
		if (m_Phase != FadePhase::Idle)
			return false;
		m_WasPaused = m_Director.IsTimePaused();
		m_Director.SetTimePaused(true);
		Begin(FadePhase::Out);
		return true;
	}

	void SleepFlow::Begin(FadePhase phase)
	{
		m_Phase = phase;
		m_Time = 0.0f;
	}

	void SleepFlow::OnRolled(DayRollReason reason)
	{
		if (m_Phase == FadePhase::Idle && reason == DayRollReason::PassOut)
		{
			m_WasPaused = m_Director.IsTimePaused();
			m_Director.SetTimePaused(true);
		}
		else if (m_Phase == FadePhase::Idle)
		{
			return;
		}
		Begin(FadePhase::Hold);
	}

	void SleepFlow::Tick(float dt)
	{
		if (m_Phase == FadePhase::Idle)
			return;

		m_Time += dt;
		switch (m_Phase)
		{
			case FadePhase::Out:
				if (m_Time >= FADE_OUT_SECONDS)
				{
					Begin(FadePhase::Hold);
					m_Director.RequestSleep();
				}
				break;
			case FadePhase::Hold:
				if (m_Time >= FADE_HOLD_SECONDS)
					Begin(FadePhase::In);
				break;
			case FadePhase::In:
				if (m_Time >= FADE_IN_SECONDS)
				{
					m_Phase = FadePhase::Idle;
					m_Director.SetTimePaused(m_WasPaused);
				}
				break;
			case FadePhase::Idle:
				break;
		}
	}

	float SleepFlow::GetAlpha() const
	{
		switch (m_Phase)
		{
			case FadePhase::Out:  return std::clamp(m_Time / FADE_OUT_SECONDS, 0.0f, 1.0f);
			case FadePhase::Hold: return 1.0f;
			case FadePhase::In:   return 1.0f - std::clamp(m_Time / FADE_IN_SECONDS, 0.0f, 1.0f);
			case FadePhase::Idle: break;
		}
		return 0.0f;
	}

}
