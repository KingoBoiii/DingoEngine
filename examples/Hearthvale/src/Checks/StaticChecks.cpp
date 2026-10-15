#include "Checks/StaticChecks.h"
#include "Defs/BuildDefs.h"
#include "GameTuning.h"
#include "ValleyDirector.h"
#include "World/ValleyLayout.h"
#include "World/ValleyWorld.h"

#include <cmath>
#include <format>
#include <set>

namespace Dingo
{

	namespace
	{
		constexpr int k_SettleFramesMax = 120;

		int EngineCellOfTile(int x, int y)
		{
			const int side = static_cast<int>(std::ceil(static_cast<float>(VALLEY_SIZE) * TILE_SIZE / ENGINE_STATIC_CELL));
			const int ex = static_cast<int>(std::floor((static_cast<float>(x) + 0.5f) * TILE_SIZE / ENGINE_STATIC_CELL));
			const int ey = static_cast<int>(std::floor((static_cast<float>(y) + 0.5f) * TILE_SIZE / ENGINE_STATIC_CELL));
			return ey * side + ex;
		}

		uint32_t EngineCellCount()
		{
			const int side = static_cast<int>(std::ceil(static_cast<float>(VALLEY_SIZE) * TILE_SIZE / ENGINE_STATIC_CELL));
			return static_cast<uint32_t>(side * side);
		}

		uint32_t EngineCellsOf(const DirtyCells& cells, const std::vector<int>& gameCells)
		{
			std::set<int> engine;
			for (const int cell : gameCells)
			{
				const TileRect rect = cells.CellRect(cell);
				engine.insert(EngineCellOfTile(rect.X, rect.Y));
			}
			return static_cast<uint32_t>(engine.size());
		}

		// A free grass tile in the farm plot, optionally in a different engine cell from `avoidEngineCell`.
		bool FindPlotTile(const TileGrid& grid, int avoidEngineCell, int& outX, int& outY)
		{
			for (int y = FARM_PLOT.Y; y < FARM_PLOT.Bottom(); ++y)
			{
				for (int x = FARM_PLOT.X; x < FARM_PLOT.Right(); ++x)
				{
					if (grid.GetGround(x, y) != Ground::Grass || !grid.IsAreaFree(x, y, 1, 1))
						continue;
					if (avoidEngineCell >= 0 && EngineCellOfTile(x, y) == avoidEngineCell)
						continue;
					outX = x;
					outY = y;
					return true;
				}
			}
			return false;
		}

		std::vector<int> FarmCells(const TileGrid& grid)
		{
			DirtyCells probe(grid.GetWidth(), grid.GetHeight(), CELL_TILES);
			probe.MarkFarmCells(grid);
			return probe.Drain();
		}

		bool Rebuilt(const WorldSyncStats& sync, std::initializer_list<int> cells, int created, int destroyed, int recoloured)
		{
			return sync.Cells == std::vector<int>(cells) && sync.Created == created && sync.Destroyed == destroyed && sync.Recoloured == recoloured;
		}

		std::string Describe(const WorldSyncStats& sync, uint32_t baked)
		{
			return std::format("{} game cells, +{} -{} ~{} entities, {} engine cells baked", sync.Cells.size(), sync.Created, sync.Destroyed, sync.Recoloured, baked);
		}
	}

	bool StaticCheckRun::Finish(CheckReport& report)
	{
		DE_INFO("Static checks: {} passed, {} failed", report.GetPassed() - m_PassedAtStart, report.GetFailed() - m_FailedAtStart);
		m_Step = Step::Done;
		return true;
	}

	bool StaticCheckRun::Tick(CheckReport& report, ValleyDirector& director, ValleyWorld& world, uint32_t bakedLastFrame)
	{
		TileGrid& grid = director.GetGrid();
		const WorldSyncStats& sync = world.GetLastSync();
		const DirtyCells& cells = world.GetCells();

		switch (m_Step)
		{
			case Step::Start:
			{
				m_PassedAtStart = report.GetPassed();
				m_FailedAtStart = report.GetFailed();
				if (!world.IsStatic())
				{
					DE_INFO("Static checks: skipped under --no-static");
					m_Step = Step::Done;
					return true;
				}
				m_Step = Step::Settle;
				return false;
			}

			case Step::Settle:
			{
				++m_Frames;
				if (m_Frames == 1)
					m_FirstBake = bakedLastFrame;
				if (m_Frames < 2 || bakedLastFrame != 0 || !sync.Cells.empty())
				{
					if (m_Frames <= k_SettleFramesMax)
						return false;
					report.Check(false, "Static: the valley's bake settles (a frame with no edit rebakes nothing)");
					return Finish(report);
				}
				report.Check(m_FirstBake == EngineCellCount(),
					std::format("Static: the first frame bakes every engine cell of the valley ({} of {})", m_FirstBake, EngineCellCount()));
				report.Check(true, "Static: the valley's bake settles (a frame with no edit rebakes nothing)");

				if (!FindPlotTile(grid, -1, m_AX, m_AY))
				{
					report.Check(false, "Static: the farm plot has a free grass tile");
					return Finish(report);
				}
				m_CellA = cells.CellOfTile(m_AX, m_AY);
				m_Placed = grid.AddOccupant(OccupantKind::Building, static_cast<uint16_t>(BuildId::Scarecrow), m_AX, m_AY, 1, 1, 0, 0);
				world.MarkArea(m_AX, m_AY, 1, 1);
				m_Step = Step::Place;
				return false;
			}

			case Step::Place:
			{
				report.Check(m_Placed != k_NoOccupant && Rebuilt(sync, { m_CellA }, 1, 0, 0),
					std::format("Static: placing a scarecrow rebuilds its one game cell with one new entity ({})", Describe(sync, bakedLastFrame)));
				report.Check(bakedLastFrame == 1, std::format("Static: placing one object rebakes exactly one cell ({} baked)", bakedLastFrame));
				grid.RemoveOccupant(m_Placed);
				world.MarkArea(m_AX, m_AY, 1, 1);
				m_Step = Step::Remove;
				return false;
			}

			case Step::Remove:
			{
				report.Check(Rebuilt(sync, { m_CellA }, 0, 1, 0),
					std::format("Static: removing it rebuilds the same game cell and destroys its entity ({})", Describe(sync, bakedLastFrame)));
				report.Check(bakedLastFrame == 1, std::format("Static: removing it rebakes exactly one cell, the same one ({} baked)", bakedLastFrame));
				grid.SetGround(m_AX, m_AY, Ground::Tilled);
				world.MarkTile(m_AX, m_AY);
				m_Step = Step::Till;
				return false;
			}

			case Step::Till:
			{
				report.Check(Rebuilt(sync, { m_CellA }, 1, 0, 0) && bakedLastFrame == 1,
					std::format("Static: tilling a tile adds one soil entity and rebakes one cell ({})", Describe(sync, bakedLastFrame)));
				grid.SetGround(m_AX, m_AY, Ground::Watered);
				world.MarkTile(m_AX, m_AY);
				m_Step = Step::Water;
				return false;
			}

			case Step::Water:
			{
				report.Check(Rebuilt(sync, { m_CellA }, 0, 0, 1),
					std::format("Static: watering changes only the soil's colour, no entity added or removed ({})", Describe(sync, bakedLastFrame)));
				report.Check(bakedLastFrame == 1, std::format("Static: the colour-only change rebakes its cell ({} baked)", bakedLastFrame));
				world.MarkTile(m_AX, m_AY);
				m_Step = Step::Rewrite;
				return false;
			}

			case Step::Rewrite:
			{
				report.Check(Rebuilt(sync, { m_CellA }, 0, 0, 0) && bakedLastFrame == 0,
					std::format("Static: rebuilding an unchanged cell writes identical draw state and rebakes nothing ({})", Describe(sync, bakedLastFrame)));

				if (!FindPlotTile(grid, EngineCellOfTile(m_AX, m_AY), m_BX, m_BY))
				{
					report.Check(false, "Static: the farm plot has a free grass tile in a second engine cell");
					return Finish(report);
				}
				grid.At(m_AX, m_AY).Crop = CropRecord{ static_cast<uint8_t>(CropId::Parsnip), 0, 0, 0 };
				grid.SetGround(m_BX, m_BY, Ground::Tilled);
				grid.At(m_BX, m_BY).Crop = CropRecord{ static_cast<uint8_t>(CropId::Parsnip), 0, 0, 0 };
				world.MarkTile(m_AX, m_AY);
				world.MarkTile(m_BX, m_BY);
				m_FarmCells = FarmCells(grid);
				m_FarmEngineCells = EngineCellsOf(cells, m_FarmCells);
				m_Step = Step::Plant;
				return false;
			}

			case Step::Plant:
			{
				report.Check(bakedLastFrame == m_FarmEngineCells && m_FarmEngineCells == 2,
					std::format("Static: planting in two engine cells rebakes those two ({} baked)", bakedLastFrame));

				// Stand-in for P6's growth: the stages change with no tile marked, so only the day roll can find them.
				grid.At(m_AX, m_AY).Crop.Stage = 1;
				grid.At(m_BX, m_BY).Crop.Stage = 1;
				const int rolls = director.GetDayRollCount();
				director.RequestSleep();
				director.Step();
				report.Check(director.GetDayRollCount() == rolls + 1, "Static: sleeping rolls the day");
				m_Step = Step::DayRoll;
				return false;
			}

			case Step::DayRoll:
			{
				report.Check(sync.Cells == m_FarmCells && sync.Created == 2 && sync.Destroyed == 2,
					std::format("Static: a day roll rebuilds only the game cells with crops ({})", Describe(sync, bakedLastFrame)));
				report.Check(bakedLastFrame == m_FarmEngineCells && m_FarmEngineCells < EngineCellCount(),
					std::format("Static: a day roll rebakes only the cells with crops ({} of {} baked)", bakedLastFrame, EngineCellCount()));

				for (const auto [x, y] : { std::pair{ m_AX, m_AY }, std::pair{ m_BX, m_BY } })
				{
					grid.At(x, y).Crop = CropRecord{};
					grid.SetGround(x, y, Ground::Grass);
					world.MarkTile(x, y);
				}
				m_Step = Step::Cleanup;
				return false;
			}

			case Step::Cleanup:
			{
				report.Check(sync.Destroyed == 4 && sync.Created == 0 && bakedLastFrame == m_FarmEngineCells,
					std::format("Static: clearing the test crops and soil rebakes just their cells ({})", Describe(sync, bakedLastFrame)));
				return Finish(report);
			}

			case Step::Done:
				return true;
		}
		return true;
	}

}
