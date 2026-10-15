#include "ValleyDirector.h"
#include "GameTuning.h"
#include "LaunchOptions.h"
#include "World/Valley.h"

#include <algorithm>

namespace Dingo
{

	namespace
	{
		Season ToSeason(LaunchSeason season)
		{
			switch (season)
			{
				case LaunchSeason::Spring: return Season::Spring;
				case LaunchSeason::Summer: return Season::Summer;
				case LaunchSeason::Fall:   return Season::Fall;
				case LaunchSeason::Winter: return Season::Winter;
			}
			return Season::Spring;
		}
	}

	DirectorConfig MakeDirectorConfig(const LaunchOptions& options)
	{
		DirectorConfig config;
		config.Seed = options.Seed;
		config.FreezeTime = options.Freeze;

		const Season season = options.HasSeason ? ToSeason(options.Season) : Season::Spring;
		const int dayOfSeason = options.Day > 0 ? options.Day : 1;
		config.StartDay = Calendar::AbsoluteDayOf(1, season, dayOfSeason);

		if (options.TimeHours >= 0.0f)
		{
			const float hours = options.TimeHours < DAY_START_HOUR ? options.TimeHours + 24.0f : options.TimeHours;
			const int minutes = static_cast<int>((hours - DAY_START_HOUR) * 60.0f + 0.5f);
			config.StartTick = std::clamp(minutes * TICKS_PER_MINUTE, 0, DAY_TICKS - 1);
		}

		switch (options.Weather)
		{
			case LaunchWeather::Auto:  break;
			case LaunchWeather::Clear: config.HasForcedWeather = true; config.ForcedWeather = Weather::Clear; break;
			case LaunchWeather::Rain:  config.HasForcedWeather = true; config.ForcedWeather = Weather::Rain; break;
			case LaunchWeather::Snow:  config.HasForcedWeather = true; config.ForcedWeather = Weather::Snow; break;
		}
		return config;
	}

	ValleyDirector::ValleyDirector(const DirectorConfig& config)
		: m_Seed(config.Seed), m_Grid(VALLEY_SIZE, VALLEY_SIZE), m_Calendar(config.Seed), m_TimePaused(config.FreezeTime)
	{
		GenerateValley(m_Grid, m_Seed);
		m_Calendar.Reset(config.StartDay, config.StartTick);
		if (config.HasForcedWeather)
			m_Calendar.ForceWeather(config.ForcedWeather);
	}

	void ValleyDirector::OnStage(SimStage stage, StageFn fn)
	{
		m_StageHandlers[static_cast<size_t>(stage)].push_back(std::move(fn));
	}

	void ValleyDirector::OnDayRoll(DayRollStep step, DayRollFn fn)
	{
		m_DayRollHandlers[static_cast<size_t>(step)].push_back(std::move(fn));
	}

	int ValleyDirector::Advance(float dt, int maxSteps)
	{
		m_Accumulator += static_cast<double>(dt);
		const double step = static_cast<double>(SIM_STEP_SECONDS);
		int ran = 0;
		while (m_Accumulator >= step && ran < maxSteps)
		{
			m_Accumulator -= step;
			Step();
			++ran;
		}
		if (m_Accumulator >= step)
			m_Accumulator = 0.0;
		return ran;
	}

	void ValleyDirector::Step()
	{
		const StepInfo info{ SIM_STEP_SECONDS, m_StepCount };

		const auto run = [this, &info](SimStage stage)
		{
			// Handlers may register more handlers, which a range-for over the vector would invalidate.
			std::vector<StageFn>& handlers = m_StageHandlers[static_cast<size_t>(stage)];
			for (size_t i = 0; i < handlers.size(); ++i)
				handlers[i](*this, info);
		};

		run(SimStage::Input);
		run(SimStage::PlayerIntent);
		run(SimStage::ToolsAndPlacement);

		if (m_SleepRequested)
		{
			m_SleepRequested = false;
			RollDay(DayRollReason::Sleep);
		}
		else if (!m_TimePaused)
		{
			m_Calendar.AdvanceTick();
			if (m_Calendar.IsDayOver())
				RollDay(DayRollReason::PassOut);
		}
		run(SimStage::Calendar);

		run(SimStage::Villagers);
		run(SimStage::Animals);
		run(SimStage::Sky);
		run(SimStage::VfxAudio);
		run(SimStage::Camera);
		run(SimStage::Ui);

		++m_StepCount;
	}

	void ValleyDirector::RunDayRoll(DayRollStep step, DayRollReason reason)
	{
		std::vector<DayRollFn>& handlers = m_DayRollHandlers[static_cast<size_t>(step)];
		for (size_t i = 0; i < handlers.size(); ++i)
			handlers[i](*this, reason);
	}

	void ValleyDirector::RollDay(DayRollReason reason)
	{
		RunDayRoll(DayRollStep::Crops, reason);
		RunDayRoll(DayRollStep::Sprinklers, reason);
		RunDayRoll(DayRollStep::Shipping, reason);
		m_Calendar.RollDay();
		RunDayRoll(DayRollStep::Weather, reason);
		RunDayRoll(DayRollStep::Board, reason);
		RunDayRoll(DayRollStep::Autosave, reason);
		++m_DayRollCount;
	}

}
