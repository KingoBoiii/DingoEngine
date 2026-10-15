#include "Checks/Checks.h"
#include "GameTuning.h"
#include "Look/ArtRegistry.h"
#include "World/Valley.h"
#include "World/WorldCells.h"

#include <algorithm>
#include <format>
#include <vector>

namespace Dingo
{

	namespace
	{
		bool HasArt(std::string_view id)
		{
			return !id.empty() && Art::Find(id) != nullptr;
		}

		bool CheckArt()
		{
			bool ok = true;
			for (int look = 0; look < static_cast<int>(GroundLook::Count); ++look)
				ok = ok && HasArt(Art::ForGround(static_cast<GroundLook>(look)));
			for (int kind = 0; kind < DECOR_KIND_COUNT; ++kind)
				ok = ok && HasArt(Art::ForDecor(static_cast<DecorKind>(kind)));
			for (int build = 0; build < BUILD_COUNT; ++build)
				ok = ok && HasArt(Art::ForBuild(static_cast<BuildId>(build)));
			for (int landmark = 0; landmark < LANDMARK_COUNT; ++landmark)
				ok = ok && HasArt(Art::ForLandmark(static_cast<Landmark>(landmark)));
			for (int crop = 0; crop < CROP_COUNT; ++crop)
				for (int stage = 0; stage < CROP_STAGES; ++stage)
					ok = ok && HasArt(Art::ForCrop(static_cast<CropId>(crop), stage));
			return ok;
		}

		void CheckMerge(CheckReport& report, const DirtyCells& cells)
		{
			TileGrid valley(VALLEY_SIZE, VALLEY_SIZE);
			GenerateValley(valley, 1);

			bool covered = true;
			bool uniform = true;
			bool stable = true;
			size_t total = 0;
			std::vector<GroundRect> rects;
			std::vector<GroundRect> again;
			for (int cell = 0; cell < cells.GetCellCount(); ++cell)
			{
				const TileRect area = cells.CellRect(cell);
				MergeGroundRects(valley, area, rects);
				MergeGroundRects(valley, area, again);
				stable = stable && rects == again;
				total += rects.size();

				std::vector<int> hits(static_cast<size_t>(area.W) * static_cast<size_t>(area.H), 0);
				for (const GroundRect& rect : rects)
				{
					for (int y = rect.Y; y < rect.Y + rect.H; ++y)
					{
						for (int x = rect.X; x < rect.X + rect.W; ++x)
						{
							if (!area.Contains(x, y))
							{
								covered = false;
								continue;
							}
							++hits[static_cast<size_t>(y - area.Y) * static_cast<size_t>(area.W) + static_cast<size_t>(x - area.X)];
							uniform = uniform && GroundLookOf(valley.GetGround(x, y)) == rect.Look;
						}
					}
				}
				for (const int hit : hits)
					covered = covered && hit == 1;
			}
			report.Check(covered, "World: each cell's ground rectangles cover every tile exactly once");
			report.Check(uniform, "World: every ground rectangle holds one ground look");
			report.Check(stable, "World: merging a cell twice gives the same rectangles");
			report.Check(total * 4 < static_cast<size_t>(VALLEY_SIZE * VALLEY_SIZE),
				std::format("World: merging leaves far fewer ground rectangles than tiles ({} for {} tiles)", total, VALLEY_SIZE * VALLEY_SIZE));

			const TileRect plotCell = cells.CellRect(cells.CellOfTile(FARM_PLOT.X, FARM_PLOT.Y));
			int tx = -1;
			int ty = -1;
			for (int y = plotCell.Y; y < plotCell.Bottom() && tx < 0; ++y)
			{
				for (int x = plotCell.X; x < plotCell.Right() && tx < 0; ++x)
				{
					if (valley.GetGround(x, y) == Ground::Grass)
					{
						tx = x;
						ty = y;
					}
				}
			}
			bool unchanged = tx >= 0;
			if (unchanged)
			{
				MergeGroundRects(valley, plotCell, rects);
				valley.SetGround(tx, ty, Ground::Tilled);
				MergeGroundRects(valley, plotCell, again);
				unchanged = rects == again;
				valley.SetGround(tx, ty, Ground::Watered);
				MergeGroundRects(valley, plotCell, again);
				unchanged = unchanged && rects == again;
			}
			report.Check(unchanged, "World: tilling and watering leave a cell's ground rectangles as they were (watering is colour-only)");
		}
	}

	void RunWorldChecks(CheckReport& report)
	{
		SuiteScope scope(report, "World");

		DirtyCells cells(VALLEY_SIZE, VALLEY_SIZE, CELL_TILES);
		const int side = VALLEY_SIZE / CELL_TILES;
		report.Check(cells.GetCellCount() == side * side, std::format("World: the valley is {} x {} cells of {} tiles", side, side, CELL_TILES));
		report.Check(cells.CellOfTile(0, 0) == 0 && cells.CellOfTile(CELL_TILES, 0) == 1 && cells.CellOfTile(0, CELL_TILES) == side &&
			cells.CellOfTile(VALLEY_SIZE - 1, VALLEY_SIZE - 1) == side * side - 1, "World: each tile maps to its cell");

		cells.MarkTile(3, 3);
		cells.MarkTile(7, 9);
		cells.MarkTile(3, 3);
		report.Check(cells.Drain() == std::vector<int>{ 0 }, "World: three edits in one cell queue it once");
		report.Check(cells.Empty() && !cells.IsQueued(0), "World: draining empties the queue");

		cells.MarkArea(CELL_TILES - 1, CELL_TILES - 1, 2, 2);
		report.Check(cells.Drain() == std::vector<int>{ 0, 1, side, side + 1 }, "World: an area across a cell corner queues its four cells, ascending");

		cells.MarkTile(-1, 5);
		cells.MarkTile(VALLEY_SIZE, 0);
		cells.MarkArea(-5, -5, 3, 3);
		report.Check(cells.Empty(), "World: tiles outside the valley queue nothing");

		cells.MarkAll();
		report.Check(cells.Drain().size() == static_cast<size_t>(side * side), "World: MarkAll queues every cell once");

		Occupant wide;
		wide.X = static_cast<int16_t>(2 * CELL_TILES - 1);
		wide.Y = 2;
		wide.W = 2;
		wide.H = 2;
		report.Check(cells.CellOfOccupant(wide) == 2, "World: an occupant across a cell edge belongs to its centre tile's cell");

		TileGrid grid(VALLEY_SIZE, VALLEY_SIZE);
		grid.SetGround(25, 43, Ground::Tilled);
		grid.At(25, 43).Crop.Def = static_cast<uint8_t>(CropId::Parsnip);
		grid.SetGround(70, 80, Ground::Watered);
		grid.At(5, 90).Crop.Def = static_cast<uint8_t>(CropId::Radish);
		cells.MarkFarmCells(grid);
		std::vector<int> farm = { cells.CellOfTile(25, 43), cells.CellOfTile(70, 80), cells.CellOfTile(5, 90) };
		std::sort(farm.begin(), farm.end());
		report.Check(cells.Drain() == farm, "World: a day roll queues only the cells holding crops or soil");

		CheckMerge(report, cells);

		report.Check(CheckArt(), "World: every ground look, decoration, buildable, landmark and crop stage has an art entry");
		const ArtEntry* dry = Art::Find(Art::SOIL);
		const ArtEntry* wet = Art::Find(Art::SOIL_WATERED);
		report.Check(dry && wet && dry->Primitive == wet->Primitive && dry->Size == wet->Size && dry->ModelPath == wet->ModelPath && dry->Color != wet->Color,
			"World: dry and watered soil differ in colour only");
	}

}
