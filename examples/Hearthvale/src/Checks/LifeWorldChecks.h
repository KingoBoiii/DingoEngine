#pragma once
#include "Checks/CheckReport.h"

#include <cstdint>

namespace Dingo
{

	class AnimalWorld;
	class Herd;
	class ValleyDirector;
	class VillagerSim;
	class VillagerWorld;

	// The Life suite's scene half: villagers found by a physics ray, animal entities following the herd. Needs
	// a window and a scene; never run headless.
	class LifeWorldCheckRun
	{
	public:
		// Call once a frame before the villagers sync. Returns true once the suite has logged its summary.
		bool Tick(CheckReport& report, ValleyDirector& director, const VillagerSim& sim, VillagerWorld* villagers, AnimalWorld* animals, Herd& herd);

		// How many sim and scene steps the layer runs this frame: several fixed sim ticks while the drift check runs,
		// else one.
		int GetStepsPerFrame() const { return m_Step == Step::Drift ? k_DriftSteps : 1; }

	private:
		static constexpr int k_DriftSteps = 8;

		enum class Step : uint8_t { Start, Settle, Villagers, Drift, Animals, Replaced, Cleared, Done };

		bool StartAnimals(CheckReport& report, ValleyDirector& director, AnimalWorld* animals, Herd& herd);
		bool Finish(CheckReport& report, ValleyDirector& director);

	private:
		Step m_Step = Step::Start;
		int m_Frames = 0;
		int m_Measured = 0;
		float m_Drift = 0.0f;
		bool m_Walked = false;
		int m_PassedAtStart = 0;
		int m_FailedAtStart = 0;
		int m_Day = 0;
		int m_Tick = 0;
		int m_Weather = 0;
		bool m_Paused = false;
	};

}
