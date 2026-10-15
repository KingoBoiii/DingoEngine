#pragma once
#include "Farm/Calendar.h"
#include "World/TileGrid.h"

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace Dingo
{

	struct LaunchOptions;

	// The per-step order of plan 3.2. Nothing else updates game state. Crops grow only at the day roll,
	// so they sit in DayRollStep, not here.
	enum class SimStage : uint8_t
	{
		Input,
		PlayerIntent,
		ToolsAndPlacement,
		Calendar,       // the director advances the clock (and rolls the day) first, then runs these handlers
		Villagers,
		Animals,
		Sky,
		VfxAudio,
		Camera,
		Ui,
		Count
	};

	// The day roll, in order. Weather is the calendar's own roll between Shipping and Board.
	enum class DayRollStep : uint8_t { Crops, Sprinklers, Shipping, Weather, Board, Autosave, Count };

	enum class DayRollReason : uint8_t { Sleep, PassOut };

	struct StepInfo
	{
		float Dt = 0.0f;
		uint64_t Step = 0;
	};

	struct DirectorConfig
	{
		uint32_t Seed = 1;
		int StartDay = 0;
		int StartTick = 0;
		bool HasForcedWeather = false;
		Weather ForcedWeather = Weather::Clear;
		bool FreezeTime = false;
	};

	DirectorConfig MakeDirectorConfig(const LaunchOptions& options);

	class ValleyDirector
	{
	public:
		using StageFn = std::function<void(ValleyDirector&, const StepInfo&)>;
		using DayRollFn = std::function<void(ValleyDirector&, DayRollReason)>;

		explicit ValleyDirector(const DirectorConfig& config);

		// Handlers of one stage run in registration order.
		void OnStage(SimStage stage, StageFn fn);
		void OnDayRoll(DayRollStep step, DayRollFn fn);

		// Accumulates real time into whole fixed steps; returns how many ran. Anything beyond maxSteps is dropped.
		int Advance(float dt, int maxSteps);

		void Step();

		// The day roll runs at the next step's Calendar stage, behind whatever fade the caller shows.
		void RequestSleep() { m_SleepRequested = true; }
		void SetTimePaused(bool paused) { m_TimePaused = paused; }
		bool IsTimePaused() const { return m_TimePaused; }

		uint32_t GetSeed() const { return m_Seed; }
		TileGrid& GetGrid() { return m_Grid; }
		const TileGrid& GetGrid() const { return m_Grid; }
		Calendar& GetCalendar() { return m_Calendar; }
		const Calendar& GetCalendar() const { return m_Calendar; }
		uint64_t GetStepCount() const { return m_StepCount; }
		int GetDayRollCount() const { return m_DayRollCount; }

	private:
		void RollDay(DayRollReason reason);
		void RunDayRoll(DayRollStep step, DayRollReason reason);

	private:
		uint32_t m_Seed;
		TileGrid m_Grid;
		Calendar m_Calendar;
		std::array<std::vector<StageFn>, static_cast<size_t>(SimStage::Count)> m_StageHandlers;
		std::array<std::vector<DayRollFn>, static_cast<size_t>(DayRollStep::Count)> m_DayRollHandlers;
		double m_Accumulator = 0.0;
		uint64_t m_StepCount = 0;
		int m_DayRollCount = 0;
		bool m_SleepRequested = false;
		bool m_TimePaused = false;
	};

}
