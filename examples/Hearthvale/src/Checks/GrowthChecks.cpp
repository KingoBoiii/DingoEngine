#include "Checks/Checks.h"
#include "Farm/Calendar.h"
#include "Farm/Crops.h"
#include "Farm/FarmSim.h"
#include "Farm/SleepFlow.h"
#include "Look/WeatherFx.h"
#include "Placing/BuildController.h"
#include "Placing/Placement.h"
#include "ValleyDirector.h"

#include <format>
#include <optional>
#include <string>

namespace Dingo
{

	namespace
	{
		constexpr int k_PX = 24;
		constexpr int k_PY = 42;

		struct Farm
		{
			ValleyDirector Director;
			std::optional<Weather> Dawn = Weather::Clear;
			bool DawnHooked;
			Inventory Stock;
			FarmSim Sim;

			explicit Farm(int startDay, uint32_t seed = 1, bool freeze = false)
				: Director(MakeConfig(startDay, seed, freeze)), DawnHooked(HookDawn()), Sim(Director, Stock)
			{
				Stock.AddCoins(5000);
				for (const ItemId item : { ItemId::Wood, ItemId::Stone, ItemId::Fibre })
					Stock.Add(item, 500);
				for (int crop = 0; crop < CROP_COUNT; ++crop)
					Stock.Add(SeedItem(static_cast<CropId>(crop)), 99);
			}

			bool HookDawn()
			{
				Director.OnDayRoll(DayRollStep::Weather, [this](ValleyDirector& d, DayRollReason)
				{
					if (Dawn)
						d.GetCalendar().ForceWeather(*Dawn);
				});
				return true;
			}

			static DirectorConfig MakeConfig(int startDay, uint32_t seed, bool freeze)
			{
				DirectorConfig config;
				config.Seed = seed;
				config.StartDay = startDay;
				config.FreezeTime = freeze;
				return config;
			}

			TileGrid& Grid() { return Director.GetGrid(); }
			Calendar& Cal() { return Director.GetCalendar(); }
			const Tile& At(int x, int y) { return Director.GetGrid().At(x, y); }

			void Roll(Weather weather)
			{
				Cal().ForceWeather(weather);
				Director.RequestSleep();
				Director.Step();
			}

			bool Sow(int x, int y, CropId crop)
			{
				TillTile(Grid(), x, y, Weather::Clear);
				return PlantSeed(Grid(), Stock, Cal().GetAbsoluteDay(), Cal().GetSeason(), x, y, crop).Result == FarmResult::Ok;
			}

			bool Greenhouse(int x, int y)
			{
				return PlaceBuilding(Grid(), Stock, PlaceSpec{ BuildId::Greenhouse, x, y, 0 }).Result == PlaceResult::Ok;
			}
		};

		int StartOf(Season season, int dayOfSeason = 1)
		{
			return Calendar::AbsoluteDayOf(1, season, dayOfSeason);
		}

		bool PlotIsClear(Farm& f)
		{
			for (int y = k_PY; y < k_PY + 12; ++y)
			{
				for (int x = k_PX; x < k_PX + 12; ++x)
				{
					const Tile& tile = f.At(x, y);
					if (tile.GroundState != Ground::Grass || tile.Occupant != k_NoOccupant || tile.Crop.Present())
						return false;
				}
			}
			return true;
		}

		void CheckFarmActions(CheckReport& report)
		{
			Farm f(StartOf(Season::Spring));
			report.Check(PlotIsClear(f), "Growth: the farm plot starts as empty grass");

			const FarmOutcome till = TillTile(f.Grid(), k_PX, k_PY, Weather::Clear);
			report.Check(till.Result == FarmResult::Ok && f.At(k_PX, k_PY).GroundState == Ground::Tilled && till.Dirty.W == 1 && till.Dirty.H == 1, "Growth: the hoe turns grass into tilled soil and reports the tile");
			report.Check(TillTile(f.Grid(), k_PX, k_PY, Weather::Clear).Result == FarmResult::AlreadyDone, "Growth: tilling tilled soil does nothing");

			const FarmOutcome water = WaterTile(f.Grid(), k_PX, k_PY);
			report.Check(water.Result == FarmResult::Ok && f.At(k_PX, k_PY).GroundState == Ground::Watered, "Growth: the can waters tilled soil");
			report.Check(WaterTile(f.Grid(), k_PX, k_PY).Result == FarmResult::AlreadyDone && WaterTile(f.Grid(), k_PX + 1, k_PY).Result == FarmResult::NotTilled,
				"Growth: watering watered soil or untilled grass does nothing");

			const uint64_t hash = f.Grid().Hash();
			report.Check(TillTile(f.Grid(), -1, 0, Weather::Clear).Result == FarmResult::OutOfBounds && WaterTile(f.Grid(), 0, 500).Result == FarmResult::OutOfBounds
				&& PlantSeed(f.Grid(), f.Stock, 0, Season::Spring, 500, 0, CropId::Parsnip).Result == FarmResult::OutOfBounds && f.Grid().Hash() == hash,
				"Growth: farm actions outside the map change nothing");

			f.Grid().SetGround(k_PX + 2, k_PY, Ground::Path);
			f.Grid().SetGround(k_PX + 3, k_PY, Ground::Water);
			report.Check(TillTile(f.Grid(), k_PX + 2, k_PY, Weather::Clear).Result == FarmResult::BadGround && TillTile(f.Grid(), k_PX + 3, k_PY, Weather::Clear).Result == FarmResult::BadGround,
				"Growth: paths and water cannot be tilled");
			f.Grid().SetGround(k_PX + 2, k_PY, Ground::Grass);
			f.Grid().SetGround(k_PX + 3, k_PY, Ground::Grass);

			f.Grid().AddOccupant(OccupantKind::Decoration, static_cast<uint16_t>(DecorKind::Grass), k_PX + 4, k_PY, 1, 1, 0, 0);
			f.Grid().AddOccupant(OccupantKind::Decoration, static_cast<uint16_t>(DecorKind::Rock), k_PX + 5, k_PY, 1, 1, 0, 0);
			f.Grid().AddOccupant(OccupantKind::Building, static_cast<uint16_t>(BuildId::Scarecrow), k_PX + 6, k_PY, 1, 1, 0, 0);
			const int decorBefore = f.Grid().CountOccupants(OccupantKind::Decoration);
			const FarmOutcome grass = TillTile(f.Grid(), k_PX + 4, k_PY, Weather::Clear);
			report.Check(grass.Result == FarmResult::Ok && f.At(k_PX + 4, k_PY).GroundState == Ground::Tilled && f.Grid().CountOccupants(OccupantKind::Decoration) == decorBefore - 1,
				"Growth: tilling clears cosmetic grass for free");
			report.Check(TillTile(f.Grid(), k_PX + 5, k_PY, Weather::Clear).Result == FarmResult::Occupied && TillTile(f.Grid(), k_PX + 6, k_PY, Weather::Clear).Result == FarmResult::Occupied
				&& f.At(k_PX + 5, k_PY).GroundState == Ground::Grass,
				"Growth: a rock or a building blocks the hoe");

			report.Check(TillTile(f.Grid(), k_PX + 7, k_PY, Weather::Rain).Result == FarmResult::Ok && f.At(k_PX + 7, k_PY).GroundState == Ground::Watered,
				"Growth: soil tilled in the rain is already watered");

			const int parsnips = f.Stock.Count(SeedItem(CropId::Parsnip));
			report.Check(PlantSeed(f.Grid(), f.Stock, 0, Season::Spring, k_PX + 8, k_PY, CropId::Parsnip).Result == FarmResult::NotTilled && f.Stock.Count(SeedItem(CropId::Parsnip)) == parsnips,
				"Growth: seeds need tilled soil and are not used up when refused");
			const FarmOutcome plant = PlantSeed(f.Grid(), f.Stock, 3, Season::Spring, k_PX, k_PY, CropId::Parsnip);
			const CropRecord& crop = f.At(k_PX, k_PY).Crop;
			report.Check(plant.Result == FarmResult::Ok && crop.Def == static_cast<uint8_t>(CropId::Parsnip) && crop.Stage == 0 && crop.DaysWatered == 0 && crop.PlantedDay == 3
				&& f.Stock.Count(SeedItem(CropId::Parsnip)) == parsnips - 1,
				"Growth: planting takes one seed and records the crop in the tile");
			report.Check(PlantSeed(f.Grid(), f.Stock, 0, Season::Spring, k_PX, k_PY, CropId::Radish).Result == FarmResult::HasCrop, "Growth: one crop per tile");

			TillTile(f.Grid(), k_PX + 9, k_PY, Weather::Clear);
			const int tomatoes = f.Stock.Count(SeedItem(CropId::Tomato));
			report.Check(PlantSeed(f.Grid(), f.Stock, 0, Season::Spring, k_PX + 9, k_PY, CropId::Tomato).Result == FarmResult::WrongSeason && f.Stock.Count(SeedItem(CropId::Tomato)) == tomatoes
				&& !f.At(k_PX + 9, k_PY).Crop.Present(),
				"Growth: a summer seed is refused in spring and kept");
			Inventory empty;
			report.Check(PlantSeed(f.Grid(), empty, 0, Season::Spring, k_PX + 9, k_PY, CropId::Parsnip).Result == FarmResult::NoSeed && !f.At(k_PX + 9, k_PY).Crop.Present(),
				"Growth: planting without a seed in the pack is refused");

			f.Grid().At(k_PX, k_PY).Built = 0;
			report.Check(PickSeed(empty, CropId::Parsnip, Season::Spring) == CropId::Count, "Growth: no held seed gives no pick");
			Inventory some;
			some.Add(SeedItem(CropId::Tomato), 2);
			some.Add(SeedItem(CropId::Radish), 2);
			report.Check(PickSeed(some, CropId::Parsnip, Season::Spring) == CropId::Radish && PickSeed(some, CropId::Parsnip, Season::Summer) == CropId::Tomato,
				"Growth: the pick skips seeds that do not grow this season");
		}

		void CheckRipeDay(CheckReport& report)
		{
			for (const CropDef& def : GetCropDefs())
			{
				Farm f(StartOf(def.GrowSeason));
				const bool sowed = f.Sow(k_PX, k_PY, def.Id);
				bool early = false;
				bool stagesMonotone = true;
				int previousStage = 0;
				int rolls = 0;
				while (sowed && !IsRipe(f.At(k_PX, k_PY).Crop) && rolls < def.GrowthDays + 5)
				{
					WaterTile(f.Grid(), k_PX, k_PY);
					f.Roll(Weather::Clear);
					++rolls;
					const CropRecord& crop = f.At(k_PX, k_PY).Crop;
					stagesMonotone = stagesMonotone && crop.Stage >= previousStage;
					previousStage = crop.Stage;
					if (rolls < def.GrowthDays && (crop.Stage == 3 || IsRipe(crop)))
						early = true;
				}
				const CropRecord& crop = f.At(k_PX, k_PY).Crop;
				report.Check(sowed && !early && rolls == def.GrowthDays && crop.Stage == 3 && crop.DaysWatered == def.GrowthDays && stagesMonotone,
					std::format("Growth: {} watered every day is ripe after {} days and not before (took {})", def.Name, def.GrowthDays, rolls));
			}
		}

		void CheckDryDay(CheckReport& report)
		{
			const CropDef& def = GetCropDef(CropId::Parsnip);
			Farm f(StartOf(Season::Spring));
			f.Sow(k_PX, k_PY, CropId::Parsnip);

			WaterTile(f.Grid(), k_PX, k_PY);
			f.Roll(Weather::Clear);
			const int afterFirst = f.At(k_PX, k_PY).Crop.DaysWatered;
			const FarmRollReport firstRoll = f.Sim.GetLastRoll();
			report.Check(afterFirst == 1 && firstRoll.Crops.Grown == 1 && firstRoll.Dried == 1 && f.At(k_PX, k_PY).GroundState == Ground::Tilled,
				"Growth: a watered day grows the crop one day and the soil dries overnight");

			f.Roll(Weather::Clear);
			const FarmRollReport dryRoll = f.Sim.GetLastRoll();
			report.Check(f.At(k_PX, k_PY).Crop.DaysWatered == 1 && dryRoll.Crops.Dry == 1 && dryRoll.Crops.Grown == 0, "Growth: an unwatered day does not grow the crop");

			int rolls = 2;
			while (!IsRipe(f.At(k_PX, k_PY).Crop) && rolls < def.GrowthDays + 6)
			{
				WaterTile(f.Grid(), k_PX, k_PY);
				f.Roll(Weather::Clear);
				++rolls;
			}
			report.Check(rolls == def.GrowthDays + 1, std::format("Growth: one unwatered day delays ripeness by exactly one day ({} rolls, expected {})", rolls, def.GrowthDays + 1));

			Farm g(StartOf(Season::Spring));
			g.Sow(k_PX, k_PY, CropId::Parsnip);
			for (int i = 0; i < 5; ++i)
				g.Roll(Weather::Clear);
			report.Check(g.At(k_PX, k_PY).Crop.Present() && g.At(k_PX, k_PY).Crop.DaysWatered == 0 && g.At(k_PX, k_PY).Crop.Stage == 0, "Growth: a crop that is never watered stays a seed and does not die");
		}

		void CheckRain(CheckReport& report)
		{
			Farm f(StartOf(Season::Spring));
			f.Sow(k_PX, k_PY, CropId::Parsnip);
			f.Sow(k_PX + 1, k_PY, CropId::Radish);
			f.Roll(Weather::Rain);
			report.Check(f.At(k_PX, k_PY).Crop.DaysWatered == 1 && f.At(k_PX + 1, k_PY).Crop.DaysWatered == 1 && f.Sim.GetLastRoll().Crops.Grown == 2,
				"Growth: rain waters every outdoor crop with no can");

			Farm g(StartOf(Season::Spring));
			g.Greenhouse(30, 44);
			g.Sow(31, 45, CropId::Parsnip);
			g.Sow(k_PX, k_PY, CropId::Parsnip);
			g.Roll(Weather::Rain);
			report.Check(IsGreenhouseTile(g.Grid(), 31, 45) && g.At(31, 45).Crop.DaysWatered == 0 && g.At(k_PX, k_PY).Crop.DaysWatered == 1,
				"Growth: rain does not reach a crop under the greenhouse roof");

			uint32_t seed = 1;
			const int today = StartOf(Season::Spring, 5);
			while (seed < 20000 && Calendar::WeatherForDay(seed, today + 1) != Weather::Rain)
				++seed;
			Farm h(today, seed);
			h.Greenhouse(30, 44);
			TillTile(h.Grid(), k_PX, k_PY, Weather::Clear);
			TillTile(h.Grid(), 31, 45, Weather::Clear);
			h.Sow(k_PX + 1, k_PY, CropId::Parsnip);
			h.Dawn.reset();
			h.Roll(Weather::Clear);
			report.Check(h.Cal().GetWeather() == Weather::Rain && h.At(k_PX, k_PY).GroundState == Ground::Watered && h.At(k_PX + 1, k_PY).GroundState == Ground::Watered
				&& h.At(31, 45).GroundState == Ground::Tilled && h.Sim.GetLastRoll().Rained >= 2,
				"Growth: a rainy dawn waters the outdoor soil and leaves the greenhouse floor dry");
			h.Roll(Weather::Rain);
			report.Check(h.At(k_PX + 1, k_PY).Crop.DaysWatered == 1, "Growth: the crop under a rainy dawn grows at the next roll");

			WeatherFx fx;
			report.Check(!fx.SetWeather(Weather::Clear) && fx.SetWeather(Weather::Rain) && fx.IsPrecipitating() && !fx.SetWeather(Weather::Rain) && fx.SetWeather(Weather::Snow) && fx.GetWeather() == Weather::Snow,
				"Growth: the weather hook reports a change once");
		}

		void CheckSnow(CheckReport& report)
		{
			Farm f(StartOf(Season::Winter, 3));
			f.Greenhouse(30, 44);
			f.Sow(k_PX, k_PY, CropId::Kale);
			f.Sow(31, 45, CropId::Kale);
			WaterTile(f.Grid(), k_PX, k_PY);
			WaterTile(f.Grid(), 31, 45);
			f.Roll(Weather::Snow);
			report.Check(f.At(k_PX, k_PY).Crop.DaysWatered == 0 && f.At(31, 45).Crop.DaysWatered == 1 && f.Sim.GetLastRoll().Crops.Snowed == 1,
				"Growth: snow stops outdoor growth even when watered, and the greenhouse grows on");

			WaterTile(f.Grid(), k_PX, k_PY);
			f.Roll(Weather::Clear);
			report.Check(f.At(k_PX, k_PY).Crop.DaysWatered == 1, "Growth: the next clear watered day grows the outdoor crop again");
		}

		void CheckSeasons(CheckReport& report)
		{
			Farm f(StartOf(Season::Spring, 13));
			f.Greenhouse(30, 44);
			f.Sow(k_PX, k_PY, CropId::Parsnip);
			f.Sow(31, 45, CropId::Parsnip);
			TillTile(f.Grid(), 32, 46, Weather::Clear);
			const int tomatoes = f.Stock.Count(SeedItem(CropId::Tomato));
			report.Check(PlantSeed(f.Grid(), f.Stock, 0, Season::Spring, 32, 46, CropId::Tomato).Result == FarmResult::Ok && f.Stock.Count(SeedItem(CropId::Tomato)) == tomatoes - 1,
				"Growth: the greenhouse takes a summer seed in spring");
			WaterTile(f.Grid(), k_PX, k_PY);
			WaterTile(f.Grid(), 31, 45);
			f.Roll(Weather::Clear);
			report.Check(f.At(k_PX, k_PY).Crop.Present() && f.Cal().GetDayOfSeason() == 14, "Growth: a spring crop survives until the last day of spring");

			WaterTile(f.Grid(), k_PX, k_PY);
			WaterTile(f.Grid(), 31, 45);
			f.Roll(Weather::Clear);
			report.Check(f.Cal().GetSeason() == Season::Summer && !f.At(k_PX, k_PY).Crop.Present() && f.Sim.GetLastRoll().Crops.Withered == 1 && f.At(k_PX, k_PY).GroundState == Ground::Tilled,
				"Growth: the outdoor spring crop withers at the season change and leaves tilled soil");
			report.Check(f.At(31, 45).Crop.Present() && f.At(32, 46).Crop.Present() && f.At(31, 45).Crop.DaysWatered == 2,
				"Growth: crops in the greenhouse survive the season change, out of season ones too");

			Farm g(StartOf(Season::Winter, 14));
			g.Sow(k_PX, k_PY, CropId::Turnip);
			WaterTile(g.Grid(), k_PX, k_PY);
			g.Roll(Weather::Clear);
			report.Check(g.Cal().GetSeason() == Season::Spring && g.Cal().GetYear() == 2 && !g.At(k_PX, k_PY).Crop.Present(), "Growth: winter crops wither when spring comes round");

			Farm h(StartOf(Season::Summer, 14));
			h.Sow(k_PX, k_PY, CropId::Melon);
			h.Sow(k_PX + 1, k_PY, CropId::Sunflower);
			h.Roll(Weather::Clear);
			report.Check(h.Cal().GetSeason() == Season::Fall && !h.At(k_PX, k_PY).Crop.Present() && !h.At(k_PX + 1, k_PY).Crop.Present(), "Growth: summer crops wither when fall comes");
		}

		void CheckRegrow(CheckReport& report)
		{
			Farm f(StartOf(Season::Spring));
			const CropDef& straw = GetCropDef(CropId::Strawberry);
			f.Sow(k_PX, k_PY, CropId::Strawberry);
			f.Sow(k_PX + 1, k_PY, CropId::Parsnip);
			report.Check(HarvestTile(f.Grid(), f.Stock, k_PX, k_PY).Result == FarmResult::NotRipe && HarvestTile(f.Grid(), f.Stock, k_PX + 5, k_PY).Result == FarmResult::NoCrop,
				"Growth: an unripe crop or an empty tile gives nothing");

			for (int i = 0; i < straw.GrowthDays; ++i)
			{
				WaterTile(f.Grid(), k_PX, k_PY);
				WaterTile(f.Grid(), k_PX + 1, k_PY);
				f.Roll(Weather::Clear);
			}
			const int before = f.Stock.Count(HarvestItem(CropId::Strawberry));
			const FarmOutcome first = HarvestTile(f.Grid(), f.Stock, k_PX, k_PY);
			report.Check(first.Result == FarmResult::Ok && f.Stock.Count(HarvestItem(CropId::Strawberry)) == before + straw.HarvestYield && first.Yield == HarvestItem(CropId::Strawberry),
				"Growth: harvesting a ripe crop puts the yield in the inventory");
			report.Check(f.At(k_PX, k_PY).Crop.Present() && !IsRipe(f.At(k_PX, k_PY).Crop) && f.At(k_PX, k_PY).Crop.Stage == 2, "Growth: a regrowing crop stays and goes back to young");

			int rolls = 0;
			bool early = false;
			while (!IsRipe(f.At(k_PX, k_PY).Crop) && rolls < straw.RegrowDays + 5)
			{
				WaterTile(f.Grid(), k_PX, k_PY);
				f.Roll(Weather::Clear);
				++rolls;
				if (rolls < straw.RegrowDays && IsRipe(f.At(k_PX, k_PY).Crop))
					early = true;
			}
			report.Check(rolls == straw.RegrowDays && !early, std::format("Growth: the strawberry is ripe again after {} watered days (took {})", straw.RegrowDays, rolls));

			const CropDef& parsnipDef = GetCropDef(CropId::Parsnip);
			report.Check(IsRipe(f.At(k_PX, k_PY).Crop) && f.At(k_PX + 1, k_PY).Crop.DaysWatered == parsnipDef.GrowthDays && IsRipe(f.At(k_PX + 1, k_PY).Crop),
				"Growth: a ripe crop waits and does not overgrow");

			HarvestTile(f.Grid(), f.Stock, k_PX, k_PY);
			f.Roll(Weather::Clear);
			report.Check(f.At(k_PX, k_PY).Crop.DaysWatered == straw.GrowthDays - straw.RegrowDays, "Growth: a dry day delays the regrowth");

			const FarmOutcome parsnip = HarvestTile(f.Grid(), f.Stock, k_PX + 1, k_PY);
			report.Check(parsnip.Result == FarmResult::Ok && !f.At(k_PX + 1, k_PY).Crop.Present() && f.At(k_PX + 1, k_PY).GroundState != Ground::Grass,
				"Growth: a single-harvest crop is gone after harvest and leaves the soil");
		}

		void CheckSprinklers(CheckReport& report)
		{
			Farm f(StartOf(Season::Spring));
			f.Grid().AddOccupant(OccupantKind::Building, static_cast<uint16_t>(BuildId::Sprinkler), 30, 48, 1, 1, 0, 0);
			f.Grid().AddOccupant(OccupantKind::Building, static_cast<uint16_t>(BuildId::QualitySprinkler), 26, 51, 1, 1, 0, 0);
			for (int y = 46; y <= 50; ++y)
			{
				for (int x = 28; x <= 32; ++x)
				{
					if (x != 30 || y != 48)
						TillTile(f.Grid(), x, y, Weather::Clear);
				}
			}
			for (int y = 49; y <= 53; ++y)
			{
				for (int x = 24; x <= 28; ++x)
				{
					if (x != 26 || y != 51)
						TillTile(f.Grid(), x, y, Weather::Clear);
				}
			}
			f.Sow(31, 48, CropId::Parsnip);

			f.Roll(Weather::Clear);
			report.Check(f.At(31, 48).Crop.DaysWatered == 0, "Growth: a sprinkler placed today has not watered yet, so the first roll is dry");

			bool ring = true;
			bool outside = true;
			for (int y = 46; y <= 50; ++y)
			{
				for (int x = 28; x <= 32; ++x)
				{
					if (x == 30 && y == 48)
						continue;
					const bool near = x >= 29 && x <= 31 && y >= 47 && y <= 49;
					const bool qualityReach = x >= 24 && x <= 28 && y >= 49 && y <= 53;
					const Ground ground = f.At(x, y).GroundState;
					if (near && ground != Ground::Watered)
						ring = false;
					if (!near && !qualityReach && ground == Ground::Watered)
						outside = false;
				}
			}
			report.Check(ring && outside, "Growth: a sprinkler waters the 3 x 3 around it at dawn and nothing beyond");

			bool wide = true;
			for (int y = 49; y <= 53; ++y)
			{
				for (int x = 24; x <= 28; ++x)
				{
					if ((x != 26 || y != 51) && f.At(x, y).GroundState != Ground::Watered)
						wide = false;
				}
			}
			report.Check(wide, "Growth: a quality sprinkler waters the 5 x 5 around it");
			int wateredTiles = 0;
			for (int y = k_PY; y < k_PY + 12; ++y)
			{
				for (int x = k_PX; x < k_PX + 12; ++x)
					wateredTiles += f.At(x, y).GroundState == Ground::Watered ? 1 : 0;
			}
			report.Check(wateredTiles > 0 && f.Sim.GetLastRoll().Sprinklered == wateredTiles, "Growth: the roll report counts the tiles the sprinklers watered");

			f.Roll(Weather::Clear);
			report.Check(f.At(31, 48).Crop.DaysWatered == 1, "Growth: a crop next to a sprinkler grows with no can");
			f.Roll(Weather::Clear);
			report.Check(f.At(31, 48).Crop.DaysWatered == 2, "Growth: the sprinkler waters again every dawn");
		}

		void CheckPassOut(CheckReport& report)
		{
			Farm f(StartOf(Season::Spring, 4));
			f.Stock.AddCoins(1234 - f.Stock.GetCoins());
			f.Cal().Reset(f.Cal().GetAbsoluteDay(), DAY_TICKS - 1);
			const int rolls = f.Director.GetDayRollCount();
			f.Director.Step();
			report.Check(f.Director.GetDayRollCount() == rolls + 1 && f.Sim.GetLastRoll().Reason == DayRollReason::PassOut && f.Stock.GetCoins() == 1234 - 123 && f.Sim.GetLastRoll().CoinsLost == 123,
				"Growth: passing out at 02:00 costs 10% of the coins, rounded down");
			report.Check(f.Cal().GetDayOfSeason() == 5 && f.Cal().GetTickOfDay() == 0, "Growth: the pass-out day rolls to 06:00 of the next day");

			f.Roll(Weather::Clear);
			report.Check(f.Sim.GetLastRoll().Reason == DayRollReason::Sleep && f.Stock.GetCoins() == 1111 && f.Sim.GetLastRoll().CoinsLost == 0, "Growth: sleeping in a bed costs nothing");

			Farm g(StartOf(Season::Spring, 4));
			g.Stock.AddCoins(9 - g.Stock.GetCoins());
			g.Cal().Reset(g.Cal().GetAbsoluteDay(), DAY_TICKS - 1);
			g.Director.Step();
			report.Check(g.Stock.GetCoins() == 9, "Growth: ten percent of a handful of coins rounds down to nothing");

			Farm h(StartOf(Season::Spring, 4));
			h.Sow(k_PX, k_PY, CropId::Parsnip);
			WaterTile(h.Grid(), k_PX, k_PY);
			h.Cal().Reset(h.Cal().GetAbsoluteDay(), DAY_TICKS - 1);
			h.Director.Step();
			report.Check(h.At(k_PX, k_PY).Crop.DaysWatered == 1, "Growth: a pass-out still grows the watered crops");
		}

		bool Advance(Farm& f, SleepFlow& flow, int maxSteps, int& steps)
		{
			for (int i = 0; i < maxSteps && flow.GetPhase() != FadePhase::Idle; ++i)
			{
				f.Director.Step();
				++steps;
			}
			return flow.GetPhase() == FadePhase::Idle;
		}

		void CheckSleepFlow(CheckReport& report)
		{
			Farm f(StartOf(Season::Spring, 2));
			SleepFlow flow(f.Director);
			report.Check(flow.GetPhase() == FadePhase::Idle && flow.GetAlpha() == 0.0f && !flow.BlocksInput(), "Growth: no fade runs at the start");

			const int day = f.Cal().GetAbsoluteDay();
			const int rolls = f.Director.GetDayRollCount();
			report.Check(flow.RequestSleep() && !flow.RequestSleep() && flow.BlocksInput() && f.Director.IsTimePaused(), "Growth: sleeping starts the fade, blocks input, stops the clock and refuses a second request");

			bool rolledBeforeBlack = false;
			float lastAlpha = 0.0f;
			bool rising = true;
			int steps = 0;
			while (flow.GetPhase() == FadePhase::Out && steps < 200)
			{
				f.Director.Step();
				++steps;
				if (flow.GetPhase() == FadePhase::Out)
				{
					if (f.Director.GetDayRollCount() != rolls)
						rolledBeforeBlack = true;
					rising = rising && flow.GetAlpha() >= lastAlpha;
					lastAlpha = flow.GetAlpha();
				}
			}
			report.Check(!rolledBeforeBlack && rising && lastAlpha > 0.0f && lastAlpha < 1.0f && flow.GetPhase() == FadePhase::Hold && flow.GetAlpha() == 1.0f,
				std::format("Growth: the screen fades to black over {} steps before anything changes", steps));
			report.Check(f.Director.GetDayRollCount() == rolls + 1 && f.Cal().GetAbsoluteDay() == day + 1, "Growth: the day rolls once, at full black");

			bool dark = true;
			int hold = 0;
			while (flow.GetPhase() == FadePhase::Hold && hold < 200)
			{
				f.Director.Step();
				++hold;
				if (flow.GetPhase() == FadePhase::Hold)
					dark = dark && flow.GetAlpha() == 1.0f;
			}
			report.Check(dark && hold > 0 && f.Cal().GetTickOfDay() == 0, "Growth: black holds while the clock stays at 06:00");

			bool falling = true;
			float previous = 1.0f;
			int fadeIn = 0;
			while (flow.GetPhase() == FadePhase::In && fadeIn < 200)
			{
				f.Director.Step();
				++fadeIn;
				if (flow.GetPhase() == FadePhase::In)
				{
					falling = falling && flow.GetAlpha() <= previous;
					previous = flow.GetAlpha();
				}
			}
			report.Check(falling && flow.GetPhase() == FadePhase::Idle && flow.GetAlpha() == 0.0f && !flow.BlocksInput() && !f.Director.IsTimePaused() && f.Director.GetDayRollCount() == rolls + 1,
				"Growth: the fade clears, input returns, the clock runs again and no second roll happens");

			Farm g(StartOf(Season::Spring, 2), 1, true);
			SleepFlow frozen(g.Director);
			frozen.RequestSleep();
			int count = 0;
			Advance(g, frozen, 400, count);
			report.Check(frozen.GetPhase() == FadePhase::Idle && g.Director.IsTimePaused() && g.Director.GetDayRollCount() == 1, "Growth: a frozen clock stays frozen after the fade");

			Farm h(StartOf(Season::Spring, 2));
			SleepFlow passOut(h.Director);
			h.Cal().Reset(h.Cal().GetAbsoluteDay(), DAY_TICKS - 1);
			h.Director.Step();
			report.Check(passOut.GetPhase() == FadePhase::Hold && passOut.GetAlpha() == 1.0f && passOut.BlocksInput() && h.Director.IsTimePaused() && h.Director.GetDayRollCount() == 1,
				"Growth: passing out cuts to black right after the roll");
			count = 0;
			Advance(h, passOut, 400, count);
			report.Check(passOut.GetPhase() == FadePhase::Idle && !h.Director.IsTimePaused() && h.Director.GetDayRollCount() == 1 && h.Cal().GetTickOfDay() >= 0,
				"Growth: after a pass-out the fade clears and time resumes");

			Farm k(StartOf(Season::Spring, 2));
			const int before = k.Director.GetDayRollCount();
			k.Director.RequestSleep();
			k.Director.Step();
			SleepFlow idle(k.Director);
			report.Check(k.Director.GetDayRollCount() == before + 1 && idle.GetPhase() == FadePhase::Idle, "Growth: a roll nobody asked the flow for starts no fade");
		}

		void CheckController(CheckReport& report)
		{
			Farm f(StartOf(Season::Spring));
			int dirty = 0;
			BuildController controller(f.Grid(), f.Stock, [&dirty](const TileRect&) { ++dirty; });
			controller.SetCalendar(&f.Cal());
			int slept = 0;
			controller.SetSleepHandler([&slept] { ++slept; });

			const auto act = [&controller](int x, int y, bool use, bool interact)
			{
				AimInput aim;
				aim.Feet = glm::vec2(static_cast<float>(x) + 0.5f, static_cast<float>(y) - 0.5f);
				aim.HasMouse = true;
				aim.MouseGround = glm::vec2(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);
				PlayerIntent intent;
				intent.Use = use;
				intent.Interact = interact;
				controller.Step(intent, aim);
			};

			controller.SelectTool(ItemId::Hoe);
			act(k_PX, k_PY, true, false);
			report.Check(f.At(k_PX, k_PY).GroundState == Ground::Tilled && dirty == 1, "Controller: the hoe tills the aimed tile and reports it");

			controller.SelectTool(ItemId::WateringCan);
			act(k_PX, k_PY, true, false);
			report.Check(f.At(k_PX, k_PY).GroundState == Ground::Watered && dirty == 2, "Controller: the can waters it");
			act(k_PX, k_PY, true, false);
			report.Check(dirty == 2, "Controller: watering twice reports nothing the second time");

			controller.SelectTool(ItemId::Axe);
			act(k_PX, k_PY, false, true);
			report.Check(f.At(k_PX, k_PY).Crop.Present() && f.At(k_PX, k_PY).Crop.Def == static_cast<uint8_t>(CropId::Parsnip) && dirty == 3 && f.Stock.Count(SeedItem(CropId::Parsnip)) == 98,
				"Controller: Interact plants the first seed that grows this season");

			controller.SelectSeed(CropId::Radish);
			TillTile(f.Grid(), k_PX + 1, k_PY, Weather::Clear);
			act(k_PX + 1, k_PY, false, true);
			report.Check(f.At(k_PX + 1, k_PY).Crop.Def == static_cast<uint8_t>(CropId::Radish), "Controller: a selected seed wins over the default pick");

			PlayerIntent cycle;
			cycle.CycleSeed = true;
			AimInput idleAim;
			idleAim.Feet = glm::vec2(20.5f, 20.5f);
			const CropId before = controller.GetSeed();
			controller.Step(cycle, idleAim);
			report.Check(controller.GetSeed() != before && f.Stock.Count(SeedItem(controller.GetSeed())) > 0, "Controller: cycling picks the next seed held");

			controller.SelectTool(ItemId::Scythe);
			act(k_PX, k_PY, true, false);
			report.Check(f.At(k_PX, k_PY).Crop.Present(), "Controller: the scythe leaves an unripe crop alone");

			f.Grid().At(k_PX, k_PY).Crop.DaysWatered = static_cast<uint8_t>(GetCropDef(CropId::Parsnip).GrowthDays);
			f.Grid().At(k_PX, k_PY).Crop.Stage = 3;
			const int calls = dirty;
			act(k_PX, k_PY, true, false);
			report.Check(!f.At(k_PX, k_PY).Crop.Present() && f.Stock.Count(HarvestItem(CropId::Parsnip)) == 1 && dirty == calls + 1, "Controller: the scythe harvests a ripe crop");

			f.Grid().At(k_PX + 1, k_PY).Crop.DaysWatered = static_cast<uint8_t>(GetCropDef(CropId::Radish).GrowthDays);
			controller.SelectTool(ItemId::Axe);
			act(k_PX + 1, k_PY, false, true);
			report.Check(!f.At(k_PX + 1, k_PY).Crop.Present() && f.Stock.Count(HarvestItem(CropId::Radish)) == 1, "Controller: Interact harvests a ripe crop");

			const LandmarkSpec& home = GetLandmarkSpec(Landmark::FarmHouse);
			act(home.DoorX, home.DoorY, false, true);
			report.Check(slept == 1, "Controller: Interact at the farmhouse door goes to bed");
			act(home.Footprint.X + 1, home.Footprint.Y + 1, false, true);
			report.Check(slept == 2, "Controller: Interact on the farmhouse goes to bed");

			Farm g(StartOf(Season::Summer));
			BuildController rainy(g.Grid(), g.Stock, [](const TileRect&) {});
			rainy.SetCalendar(&g.Cal());
			g.Cal().ForceWeather(Weather::Rain);
			rainy.SelectTool(ItemId::Hoe);
			AimInput aim;
			aim.Feet = glm::vec2(24.5f, 41.5f);
			aim.HasMouse = true;
			aim.MouseGround = glm::vec2(24.5f, 42.5f);
			PlayerIntent use;
			use.Use = true;
			rainy.Step(use, aim);
			report.Check(g.At(k_PX, k_PY).GroundState == Ground::Watered, "Controller: the hoe in the rain leaves watered soil");
			Inventory none;
			BuildController bare(g.Grid(), none, [](const TileRect&) {});
			bare.SetCalendar(&g.Cal());
			PlayerIntent plant;
			plant.Interact = true;
			bare.Step(plant, aim);
			report.Check(!g.At(k_PX, k_PY).Crop.Present() && bare.GetSeed() == CropId::Count, "Controller: planting with an empty pack does nothing");
		}

		struct Outcome
		{
			uint64_t Hash;
			int Coins;
			int Rolls;
			int Harvest;
		};

		Outcome PlayDays(uint32_t seed, int days)
		{
			Farm f(StartOf(Season::Spring), seed);
			f.Dawn.reset();
			for (int i = 0; i < 6; ++i)
				f.Sow(k_PX + i, k_PY, CropId::Parsnip);
			for (int day = 0; day < days; ++day)
			{
				for (int i = 0; i < 6; ++i)
				{
					WaterTile(f.Grid(), k_PX + i, k_PY);
					HarvestTile(f.Grid(), f.Stock, k_PX + i, k_PY);
				}
				for (int i = 0; i < 6; ++i)
				{
					if (!f.At(k_PX + i, k_PY).Crop.Present())
						PlantSeed(f.Grid(), f.Stock, f.Cal().GetAbsoluteDay(), f.Cal().GetSeason(), k_PX + i, k_PY, CropId::Parsnip);
				}
				f.Director.RequestSleep();
				f.Director.Step();
			}
			return Outcome{ f.Grid().Hash(), f.Stock.GetCoins(), f.Director.GetDayRollCount(), f.Stock.Count(HarvestItem(CropId::Parsnip)) };
		}

		void CheckDeterminism(CheckReport& report)
		{
			const Outcome a = PlayDays(7, 20);
			const Outcome b = PlayDays(7, 20);
			report.Check(a.Hash == b.Hash && a.Coins == b.Coins && a.Rolls == b.Rolls && a.Harvest == b.Harvest && a.Rolls == 20,
				"Growth: the same seed played twice gives the same grid, coins and harvest");
			report.Check(a.Harvest > 0, std::format("Growth: twenty scripted days harvest something ({} parsnips)", a.Harvest));
			const Outcome c = PlayDays(8, 20);
			report.Check(c.Hash != a.Hash, "Growth: another seed gives another valley");

			Farm f(StartOf(Season::Spring));
			std::string order;
			f.Director.OnDayRoll(DayRollStep::Crops, [&order](ValleyDirector&, DayRollReason) { order += 'C'; });
			f.Director.OnDayRoll(DayRollStep::Sprinklers, [&order](ValleyDirector&, DayRollReason) { order += 'S'; });
			f.Director.OnDayRoll(DayRollStep::Weather, [&order](ValleyDirector&, DayRollReason) { order += 'W'; });
			f.Director.RequestSleep();
			f.Director.Step();
			report.Check(order == "CSW", "Growth: the farm handlers sit on the Crops, Sprinklers and Weather steps in the plan's order");
		}
	}

	void RunGrowthChecks(CheckReport& report)
	{
		SuiteScope scope(report, "Growth");
		CheckFarmActions(report);
		CheckRipeDay(report);
		CheckDryDay(report);
		CheckRain(report);
		CheckSnow(report);
		CheckSeasons(report);
		CheckRegrow(report);
		CheckSprinklers(report);
		CheckPassOut(report);
		CheckSleepFlow(report);
		CheckController(report);
		CheckDeterminism(report);
	}

}
