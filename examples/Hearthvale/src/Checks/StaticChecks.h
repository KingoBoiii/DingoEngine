#pragma once
#include "Checks/CheckReport.h"
#include "World/TileGrid.h"

#include <cstdint>
#include <vector>

namespace Dingo
{

	class ValleyDirector;
	class ValleyWorld;

	// The Static suite (plan 5): edits the live grid one step a frame and reads what the next frame rebaked.
	// Needs a window and the renderer; never run headless.
	class StaticCheckRun
	{
	public:
		// Call once a frame before the world syncs. `bakedLastFrame` is Renderer3D's StaticCellsBaked of the
		// frame before. Returns true once the suite has logged its summary.
		bool Tick(CheckReport& report, ValleyDirector& director, ValleyWorld& world, uint32_t bakedLastFrame);

	private:
		enum class Step : uint8_t { Start, Settle, Place, Remove, Till, Water, Rewrite, Plant, DayRoll, Cleanup, Done };

		bool Finish(CheckReport& report);

	private:
		Step m_Step = Step::Start;
		int m_Frames = 0;
		uint32_t m_FirstBake = 0;
		int m_PassedAtStart = 0;
		int m_FailedAtStart = 0;
		uint32_t m_Placed = k_NoOccupant;
		int m_AX = -1;
		int m_AY = -1;
		int m_BX = -1;
		int m_BY = -1;
		int m_CellA = -1;
		std::vector<int> m_FarmCells;
		uint32_t m_FarmEngineCells = 0;
	};

}
