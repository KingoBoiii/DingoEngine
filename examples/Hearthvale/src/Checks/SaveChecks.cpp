#include "Checks/Checks.h"
#include "Defs/BuildDefs.h"
#include "Defs/ItemDefs.h"
#include "Farm/Animals.h"
#include "Farm/Crops.h"
#include "Farm/Economy.h"
#include "Farm/EconomySim.h"
#include "Farm/FarmSim.h"
#include "Farm/GameState.h"
#include "Life/VillagerSim.h"
#include "Placing/Placement.h"
#include "Save/GameSaves.h"
#include "Save/SaveGame.h"
#include "Save/SaveStore.h"
#include "ValleyDirector.h"
#include "World/ValleyLayout.h"

#include <DingoEngine.h>
#include <DingoEngine/Core/Platform.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Dingo
{

	namespace
	{
		DirectorConfig SaveRigConfig(uint32_t seed)
		{
			DirectorConfig config;
			config.Seed = seed;
			return config;
		}

		const Occupant* FindBuilt(const TileGrid& grid, BuildId id)
		{
			for (const Occupant& occupant : grid.GetOccupants())
			{
				if (occupant.Kind == OccupantKind::Building && occupant.DefId == static_cast<uint16_t>(id))
					return &occupant;
			}
			return nullptr;
		}

		bool Overlaps(const TileRect& a, const TileRect& b)
		{
			return a.X < b.Right() && b.X < a.Right() && a.Y < b.Bottom() && b.Y < a.Bottom();
		}

		bool PlaceInClearing(TileGrid& grid, Inventory& pack, BuildId id)
		{
			for (int y = FARM_CLEARING.Y; y < FARM_CLEARING.Bottom(); ++y)
			{
				for (int x = FARM_CLEARING.X; x < FARM_CLEARING.Right(); ++x)
				{
					const PlaceSpec spec{ id, x, y, 0 };
					if (Overlaps(FootprintOf(spec), FARM_PLOT) || CheckPlacement(grid, pack, spec) != PlaceResult::Ok)
						continue;
					return PlaceBuilding(grid, pack, spec).Result == PlaceResult::Ok;
				}
			}
			return false;
		}

		// A farmer whose every choice follows from the day and the saved state alone, so a loaded game plays on
		// exactly as the one that was saved.
		void PlayScriptedFarmer(ValleyDirector& director, GameState& state)
		{
			Calendar& calendar = director.GetCalendar();
			TileGrid& grid = director.GetGrid();
			Inventory& pack = state.Pack;
			const int day = calendar.GetAbsoluteDay();
			const int tick = calendar.GetTickOfDay();

			if (tick == 6)
			{
				if (day == 3 || day == 10)
					calendar.Reset(day, DAY_TICKS - 1);
				else
					director.RequestSleep();
				return;
			}
			if (tick != 2)
				return;

			const Season season = calendar.GetSeason();
			for (int i = 0; i < 6; ++i)
			{
				const int x = FARM_PLOT.X + 1 + i;
				const int y = FARM_PLOT.Y + 1;
				HarvestTile(grid, pack, x, y);
				TillTile(grid, x, y, calendar.GetWeather());
				const CropId seed = PickSeed(pack, CropId::Parsnip, season);
				if (seed != CropId::Count)
					PlantSeed(grid, pack, day, season, x, y, seed);
				WaterTile(grid, x, y);
			}

			const ItemId parsnip = HarvestItem(CropId::Parsnip);
			if (const int held = pack.Count(parsnip); held > 1)
				state.Bin.Ship(pack, parsnip, held - 1);
			if (day % 2 == 0)
				ShopBuyItem(pack, SeedItem(CropId::Radish), 2);

			const VillagerId villager = static_cast<VillagerId>(day % VILLAGER_COUNT);
			state.Bonds.Talk(villager, day);
			const ItemId gift = pack.Count(ItemId::Egg) > 0 ? ItemId::Egg : parsnip;
			const GiftOutcome outcome = state.Bonds.Evaluate(villager, gift, day);
			if ((outcome == GiftOutcome::Loved || outcome == GiftOutcome::Fine) && pack.Remove(gift, 1))
				state.Bonds.Give(villager, gift, day);

			if (!FindBuilt(grid, BuildId::Coop))
				PlaceInClearing(grid, pack, BuildId::Coop);
			if (const Occupant* coop = FindBuilt(grid, BuildId::Coop))
				TendPen(grid, state.Animals, pack, coop->Id);

			if (day == 2)
				PlaceInClearing(grid, pack, BuildId::WoodFence);
			if (day == 9)
			{
				if (const Occupant* fence = FindBuilt(grid, BuildId::WoodFence))
					RemoveBuilding(grid, pack, fence->X, fence->Y);
			}
		}

		struct SaveRig
		{
			ValleyDirector Director;
			GameState State;
			FarmSim Farm;
			EconomySim Economy;
			VillagerSim Villagers;
			AnimalSim Animals;

			explicit SaveRig(uint32_t seed)
				: Director(SaveRigConfig(seed)), Farm(Director, State.Pack), Economy(Director, State), Villagers(Director), Animals(Director, State.Animals)
			{
				Director.OnStage(SimStage::ToolsAndPlacement, [this](ValleyDirector& director, const StepInfo&) { PlayScriptedFarmer(director, State); });
			}

			void Stock()
			{
				State.Pack.GiveStarterLoadout();
				State.Pack.Add(ItemId::Wood, 99);
				State.Pack.Add(ItemId::Stone, 40);
				State.Pack.AddCoins(2000);
				State.Pack.LearnPlan(BuildId::Coop);
			}

			bool PlayRolls(int rolls)
			{
				const int target = Director.GetDayRollCount() + rolls;
				for (int guard = 0; guard < rolls * 64 && Director.GetDayRollCount() < target; ++guard)
					Director.Step();
				return Director.GetDayRollCount() == target;
			}

			void Rebuilt()
			{
				Animals.Reset(Director.GetSeed());
				Villagers.InvalidatePlans();
			}

			GameStateDigest Digest() const { return HashGameState(Director, State); }
		};

		bool SameParts(const GameStateDigest& a, const GameStateDigest& b)
		{
			return a.Grid == b.Grid && a.Pack == b.Pack && a.Bin == b.Bin && a.Board == b.Board && a.Bonds == b.Bonds && a.Animals == b.Animals && a.Time == b.Time
				&& a.All == b.All;
		}

		void WriteText(const std::filesystem::path& path, const std::string& text)
		{
			std::ofstream file(path, std::ios::binary | std::ios::trunc);
			file << text;
		}

		std::string ReadText(const std::filesystem::path& path)
		{
			std::ifstream file(path, std::ios::binary);
			return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
		}

		bool FileExists(const std::filesystem::path& path)
		{
			std::error_code ec;
			return std::filesystem::exists(path, ec);
		}

		void ResumeChecks(CheckReport& report, const std::filesystem::path& root)
		{
			constexpr uint32_t seed = 5;
			SaveRig straight(seed);
			straight.Stock();
			const bool straightPlayed = straight.PlayRolls(14);
			const GameStateDigest fourteen = straight.Digest();

			SaveRig first(seed);
			first.Stock();
			GameSaves firstSaves(first.Director, first.State, SaveStore(root / "week"));
			std::string message;
			const bool activated = firstSaves.Save(0, message);
			const bool firstPlayed = first.PlayRolls(7);
			const GameStateDigest seven = first.Digest();
			report.Check(activated && firstPlayed && firstSaves.GetAutosaveCount() == 7 && firstSaves.GetActiveSlot() == 0, "Save: each of seven day rolls autosaves into the active slot");

			SaveRig second(seed + 100);
			GameSaves secondSaves(second.Director, second.State, SaveStore(root / "week"));
			secondSaves.SetOnGameReplaced([&second] { second.Rebuilt(); });
			const bool loaded = secondSaves.Load(0, message);
			report.Check(loaded && second.Director.GetSeed() == seed && SameParts(second.Digest(), seven) && secondSaves.GetActiveSlot() == 0,
				"Save: loading the autosave restores the seed and every part of the state hash");

			const bool secondPlayed = second.PlayRolls(7);
			const GameStateDigest resumed = second.Digest();
			report.Check(straightPlayed && secondPlayed && SameParts(resumed, fourteen), "Save: seven days, a save and a load, then seven more equal fourteen straight days");
			report.Check(FindBuilt(straight.Director.GetGrid(), BuildId::Coop) && straight.State.Animals.Count() > 0 && straight.State.Bonds.GetPoints(VillagerId::Shopkeeper) > 0
				&& FindBuilt(straight.Director.GetGrid(), BuildId::WoodFence) == nullptr, "Save: the scripted fortnight builds, keeps animals, makes friends and takes its fence down");
			if (!SameParts(resumed, fourteen))
			{
				DE_WARN("Save: resumed grid {} pack {} bin {} board {} bonds {} animals {} time {}", resumed.Grid == fourteen.Grid, resumed.Pack == fourteen.Pack,
					resumed.Bin == fourteen.Bin, resumed.Board == fourteen.Board, resumed.Bonds == fourteen.Bonds, resumed.Animals == fourteen.Animals, resumed.Time == fourteen.Time);
			}
		}

		void RoundTripChecks(CheckReport& report)
		{
			SaveRig rig(9);
			rig.Stock();
			rig.PlayRolls(5);
			const SaveData save = WriteSave(rig.Director, rig.State, SaveMeta{ 4321.5 });

			LoadedGame game;
			std::string error;
			const bool read = ReadSave(save, game, error);
			report.Check(read && error.empty(), "Save: a written save reads back");

			SaveRig other(77);
			GameState& state = other.State;
			const uint32_t next = rig.Director.GetGrid().GetNextOccupantId();
			const double played = game.Meta.PlaySeconds;
			ApplyLoadedGame(std::move(game), other.Director, state);
			const GameStateDigest a = rig.Digest();
			const GameStateDigest b = other.Digest();
			report.Check(read && a.Grid == b.Grid && a.Pack == b.Pack && a.Bin == b.Bin && a.Board == b.Board, "Save: grid, pack, bin and board hash the same after a round trip");
			report.Check(read && a.Bonds == b.Bonds && a.Animals == b.Animals && a.Time == b.Time && a.All == b.All, "Save: bonds, animals and the clock hash the same after a round trip");
			report.Check(other.Director.GetGrid().GetNextOccupantId() == next && played == 4321.5 && other.Director.GetSeed() == 9, "Save: the next occupant id, the play time and the seed survive");
			report.Check(WriteSave(other.Director, other.State, SaveMeta{ 4321.5 }) == save, "Save: the loaded game writes the same tree");

			const std::optional<SaveData> reparsed = SaveData::FromJson(save.ToJson());
			report.Check(reparsed && *reparsed == save, "Save: the tree survives JSON text");

			const auto refuses = [&save](auto&& edit)
			{
				SaveData broken = save;
				edit(broken);
				LoadedGame target;
				target.Day = 1234;
				std::string why;
				return !ReadSave(broken, target, why) && !why.empty() && target.Day == 1234;
			};
			report.Check(refuses([](SaveData& s) { s["calendar"]["tick"] = DAY_TICKS; }), "Save: a tick past the end of the day is refused and leaves the target alone");
			report.Check(refuses([](SaveData& s) { s["pack"]["slots"][0]["count"] = 100000; }), "Save: a stack over its maximum is refused");
			report.Check(refuses([](SaveData& s)
			{
				SaveData& occupants = s["grid"]["occupants"];
				if (occupants.Size() >= 2)
					std::swap(occupants[0], occupants[1]);
				else
					occupants = 3;
			}), "Save: occupants out of order are refused");
			report.Check(refuses([](SaveData& s) { s["pack"]["slots"][0]["item"] = "Moon Rock"; }), "Save: an unknown item name is refused");
			report.Check(refuses([](SaveData& s) { s["grid"]["ground"][3] = "gg"; }), "Save: a short ground row is refused");
			report.Check(refuses([](SaveData& s) { s.Remove("animals"); }), "Save: a missing section is refused");

			const auto reasonFor = [&save](auto&& edit)
			{
				SaveData broken = save;
				edit(broken);
				LoadedGame target;
				std::string why;
				return ReadSave(broken, target, why) ? std::string() : why;
			};
			report.Check(reasonFor([](SaveData& s) { s["grid"]["crops"].Push(std::string("0 0 0 0 0 0")); }).find("not on soil") != std::string::npos,
				"Save: a crop on ground that is not soil is refused");
			report.Check(reasonFor([](SaveData& s) { s["grid"]["built"].Push(std::string("0 0 1")); }).find("not on a path") != std::string::npos,
				"Save: a built tile on ground that is not a path is refused");
			report.Check(reasonFor([](SaveData& s) { s["grid"]["occupants"].Push(std::string("4000000000 1 0 0 0 5 5 0 0")); }).find("footprint") != std::string::npos,
				"Save: a building whose size is not its footprint is refused");
			report.Check(reasonFor([](SaveData& s)
			{
				SaveData request = SaveData::MakeArray();
				request.Push(0);
				request.Push(0);
				request.Push(true);
				s["board"]["boards"][0][0] = std::move(request);
			}).find("done before its target") != std::string::npos, "Save: a board request done before its target is refused");

			SaveData newer = save;
			newer["schema"] = SAVE_SCHEMA + 1;
			LoadedGame ignored;
			report.Check(!ReadSave(newer, ignored, error) && error.find("newer") != std::string::npos, "Save: a save from a newer game is refused by name");
			SaveData odd = save;
			odd["schema"] = "one";
			report.Check(SaveSchemaOf(odd) == -1 && !ReadSave(odd, ignored, error), "Save: a schema that is not a number is refused");
		}

		void StoreChecks(CheckReport& report, const std::filesystem::path& root)
		{
			SaveStore store(root / "store");
			const LoadedGame a = MakeNewGame(31);
			LoadedGame bGame = MakeNewGame(32);
			bGame.State.Pack.AddCoins(7);

			const auto encode = [](const LoadedGame& game)
			{
				ValleyDirector director(SaveRigConfig(game.Seed));
				GameState state;
				LoadedGame copy = game;
				ApplyLoadedGame(std::move(copy), director, state);
				return WriteSave(director, state, game.Meta);
			};
			const SaveData saveA = encode(a);
			const SaveData saveB = encode(bGame);

			LoadedGame out;
			report.Check(store.Read("slot1", out).State == SlotState::Empty && !store.Exists("slot1"), "Save store: a slot never written reads as empty");

			const bool written = store.Write("slot1", saveA) && store.Write("slot1", saveB);
			SlotRead read = store.Read("slot1", out);
			report.Check(written && read.State == SlotState::Ok && !read.FromBackup && out.Seed == 32, "Save store: the second write is what reads back");

			const std::filesystem::path path = store.PathOf("slot1");
			const std::filesystem::path bak = root / "store" / "slot1.json.bak";
			const std::filesystem::path tmp = root / "store" / "slot1.json.tmp";
			const std::string full = ReadText(path);
			WriteText(path, full.substr(0, full.size() / 2));
			read = store.Read("slot1", out);
			report.Check(FileExists(bak) && read.State == SlotState::Ok && read.FromBackup && out.Seed == 31, "Save store: a truncated save falls back to its .bak with a warning");

			SaveData invalid = saveB;
			invalid["calendar"]["weather"] = "hail";
			WriteText(path, invalid.ToJson());
			read = store.Read("slot1", out);
			report.Check(read.State == SlotState::Ok && read.FromBackup && out.Seed == 31, "Save store: a save that parses but fails the checks also falls back to its .bak");

			std::error_code ec;
			std::filesystem::remove(bak, ec);
			WriteText(path, full.substr(0, full.size() / 3));
			out.Seed = 4242;
			read = store.Read("slot1", out);
			report.Check(read.State == SlotState::Damaged && !read.Error.empty() && out.Seed == 4242 && store.Exists("slot1"), "Save store: a truncated save with no backup is damaged and changes nothing");

			const bool rewritten = store.Write("slot1", saveA);
			WriteText(tmp, full.substr(0, 40));
			read = store.Read("slot1", out);
			const bool ignored = read.State == SlotState::Ok && !read.FromBackup && out.Seed == 31;
			report.Check(rewritten && ignored && store.Write("slot1", saveB) && store.Read("slot1", out).State == SlotState::Ok && out.Seed == 32,
				"Save store: a write killed half way leaves a partial .tmp that is ignored, and the next write succeeds");

			std::filesystem::remove(path, ec);
			std::filesystem::remove(bak, ec);
			WriteText(tmp, saveA.ToJson());
			read = store.Read("slot1", out);
			report.Check(read.State == SlotState::Ok && read.FromBackup && out.Seed == 31, "Save store: a write killed before its rename is recovered from the complete .tmp");

			const bool restored = store.Write("slot1", saveA) && store.Write("slot1", saveB);
			std::filesystem::remove(path, ec);
			WriteText(tmp, invalid.ToJson());
			read = store.Read("slot1", out);
			report.Check(restored && read.State == SlotState::Ok && read.FromBackup && out.Seed == 31,
				"Save store: a missing save with an unusable .tmp still falls back to its .bak");

			report.Check(store.Delete("slot1") && !store.Exists("slot1") && store.Read("slot1", out).State == SlotState::Empty && !FileExists(tmp),
				"Save store: delete leaves no file, backup or temp behind");
			report.Check(!store.Write("../escape", saveA) && !store.Exists("../escape"), "Save store: a name that is not a plain slot name is refused");
		}

		constexpr const char* k_FixtureV0 = R"json(
{
  "seed": 7,
  "day": 4,
  "tick": 0,
  "weather": 1,
  "coins": 812,
  "grid": {
    "width": 96,
    "height": 96,
    "tiles": [[25, 43, 1], [26, 43, 2], [27, 43, 1], [40, 47, 3]],
    "built": ["40 47 4"],
    "crops": ["26 43 0 1 2 2"],
    "occupants": ["3 1 0 30 44 1 1 0 0", "9 0 0 12 30 1 1 0 2"],
    "nextOccupant": 12
  },
  "pack": {
    "selected": 1,
    "slots": [
      { "slot": 0, "item": "Hoe", "count": 1 },
      { "slot": 1, "item": "Watering Can", "count": 1 },
      { "slot": 2, "item": "Parsnip Seeds", "count": 5 },
      { "slot": 9, "item": "Wood", "count": 14 }
    ],
    "plans": [14]
  },
  "bin": { "Parsnip": 3 },
  "board": {
    "festival": 0,
    "tier": 0,
    "boards": [
      [[2, 0, false], [0, 0, false], [0, 0, false], [0, 0, false]],
      [[0, 0, false], [0, 0, false], [0, 0, false], [0, 0, false]],
      [[0, 0, false], [0, 0, false], [0, 0, false], [0, 0, false]],
      [[0, 0, false], [0, 0, false], [0, 0, false], [0, 0, false]]
    ]
  }
}
)json";

		void MigrationChecks(CheckReport& report)
		{
			std::string error;
			const std::optional<SaveData> fixture = SaveData::FromJson(k_FixtureV0, &error);
			report.Check(fixture && SaveSchemaOf(*fixture) == 0, "Save: the fixture parses and has no schema");
			if (!fixture)
				return;

			SaveData upgraded = *fixture;
			const bool migrated = MigrateSave(upgraded, error);
			report.Check(migrated && SaveSchemaOf(upgraded) == SAVE_SCHEMA && !upgraded.Contains("day") && !upgraded.Contains("coins") && !upgraded["grid"].Contains("tiles")
				&& upgraded["grid"]["ground"].Size() == VALLEY_SIZE && upgraded["calendar"]["weather"].AsString() == "rain", "Save: schema 0 migrates step by step to the current layout");

			LoadedGame game;
			const bool read = ReadSave(*fixture, game, error);
			report.Check(read, "Save: the old save loads through the migration");
			if (!read)
			{
				DE_WARN("Save: the fixture failed: {}", error);
				return;
			}
			const TileGrid& grid = game.Grid;
			report.Check(game.Seed == 7 && game.Day == 4 && game.Tick == 0 && game.Today == Weather::Rain && game.Meta.PlaySeconds == 0.0, "Save: the old clock and seed carry over");
			report.Check(grid.At(25, 43).GroundState == Ground::Tilled && grid.At(26, 43).GroundState == Ground::Watered && grid.At(40, 47).GroundState == Ground::Path
				&& grid.At(0, 0).GroundState == Ground::Grass && grid.At(40, 47).Built == 4, "Save: the sparse old ground becomes the full grid");
			const CropRecord& crop = grid.At(26, 43).Crop;
			report.Check(crop.Def == static_cast<uint8_t>(CropId::Parsnip) && crop.Stage == 1 && crop.DaysWatered == 2 && crop.PlantedDay == 2, "Save: the old crop carries over");
			const Occupant* fence = grid.FindOccupant(3);
			report.Check(grid.GetOccupants().size() == 2 && fence && fence->X == 30 && fence->Kind == OccupantKind::Building && grid.At(12, 30).Occupant == 9
				&& grid.GetNextOccupantId() == 12, "Save: the old occupants and the next id carry over");

			const Inventory& pack = game.State.Pack;
			report.Check(pack.GetCoins() == 812 && pack.GetSelectedSlot() == 1 && pack.GetSlot(2).Item == SeedItem(CropId::Parsnip) && pack.GetSlot(2).Count == 5
				&& pack.GetSlot(9).Item == ItemId::Wood && pack.GetSlot(9).Count == 14 && pack.GetSlot(3).Empty() && pack.KnowsPlan(BuildId::Coop) && !pack.KnowsPlan(BuildId::Barn),
				"Save: the old pack, its coins and plans carry over");
			report.Check(game.State.Bin.Held(HarvestItem(CropId::Parsnip)) == 3 && game.State.Animals.Empty() && game.State.Bonds.GetPoints(VillagerId::Mayor) == 0
				&& std::as_const(game.State.Bonds).Bond(VillagerId::Mayor).LastTalkDay == -1, "Save: the bin carries over and the new parts start empty");

			ValleyDirector director(SaveRigConfig(1));
			GameState state;
			LoadedGame copy = game;
			ApplyLoadedGame(std::move(copy), director, state);
			const SaveData rewritten = WriteSave(director, state, game.Meta);
			LoadedGame again;
			const bool same = rewritten["grid"]["ground"] == upgraded["grid"]["ground"] && rewritten["grid"]["occupants"] == upgraded["grid"]["occupants"]
				&& rewritten["grid"]["crops"] == upgraded["grid"]["crops"] && rewritten["pack"]["slots"] == upgraded["pack"]["slots"] && rewritten["bin"] == upgraded["bin"]
				&& rewritten["board"] == upgraded["board"] && rewritten["bonds"] == upgraded["bonds"] && rewritten["animals"] == upgraded["animals"] && rewritten["calendar"]["weather"] == upgraded["calendar"]["weather"];
			report.Check(same && ReadSave(rewritten, again, error) && again.Grid.Hash() == game.Grid.Hash(), "Save: the migrated save writes back as the current layout");

			SaveData broken = *fixture;
			broken["grid"]["tiles"][0] = "x";
			report.Check(!ReadSave(broken, again, error) && error.find("schema 0") != std::string::npos, "Save: a malformed old save is refused while upgrading");
		}

		void ProviderChecks(CheckReport& report, const std::filesystem::path& root)
		{
			const std::filesystem::path dir = root / "slots";
			std::error_code ec;
			std::filesystem::create_directories(dir, ec);
			WriteText(dir / "slot3.json", "{ \"schema\": 1, \"seed\": ");

			SaveRig rig(3);
			GameSaves saves(rig.Director, rig.State, SaveStore(dir));
			int replaced = 0;
			saves.SetOnGameReplaced([&rig, &replaced] { rig.Rebuilt(); ++replaced; });
			saves.SetNewGameSeed(21u);

			const SlotInfo empty = saves.Describe(0);
			const SlotInfo damaged = saves.Describe(2);
			report.Check(!empty.Occupied && !empty.Damaged && empty.Summary == "Empty" && !damaged.Occupied && damaged.Damaged && !damaged.Summary.empty(),
				"Save slots: an empty and a damaged slot describe themselves");

			std::string message;
			const bool started = saves.StartNew(0, message);
			const SlotInfo fresh = saves.Describe(0);
			report.Check(started && replaced == 1 && rig.Director.GetSeed() == 21 && rig.Director.GetCalendar().GetAbsoluteDay() == 0 && rig.State.Pack.GetCoins() == STARTER_COINS
				&& fresh.Occupied && fresh.Summary.starts_with("Spring 1, year 1 - 500 coins"), "Save slots: a new game starts on day 1 with the starter pack and is written at once");

			saves.AddPlayTime(3725.0f);
			rig.PlayRolls(1);
			const GameStateDigest morning = rig.Digest();
			report.Check(saves.GetAutosaveCount() == 1 && saves.Describe(0).Summary.find("Spring 2") != std::string::npos && saves.Describe(0).Summary.find("1h 2m played") != std::string::npos,
				"Save slots: sleeping autosaves the new morning with its play time");

			report.Check(saves.Save(1, message) && saves.GetActiveSlot() == 1 && saves.Describe(1).Occupied, "Save slots: saving to another slot makes it the active one");
			rig.PlayRolls(2);
			report.Check(saves.Describe(1).Summary.find("Spring 4") != std::string::npos && saves.Describe(0).Summary.find("Spring 2") != std::string::npos,
				"Save slots: autosave writes the active slot only");

			const bool loaded = saves.Load(0, message);
			report.Check(loaded && SameParts(rig.Digest(), morning) && saves.GetActiveSlot() == 0 && replaced == 2 && saves.GetPlaySeconds() == 3725.0,
				"Save slots: loading a slot brings back its morning and its play time");

			const int before = replaced;
			report.Check(!saves.Load(1 + 1, message) && !message.empty() && replaced == before && saves.GetActiveSlot() == 0, "Save slots: a damaged slot refuses to load and keeps the game");
			report.Check(saves.Delete(2, message) && !saves.Describe(2).Damaged && !saves.Describe(2).Occupied && !saves.GetStore().Exists("slot3"), "Save slots: a damaged slot can be deleted");
			report.Check(!saves.Load(2, message) && !message.empty(), "Save slots: an empty slot refuses to load");

			report.Check(saves.Delete(0, message) && saves.GetActiveSlot() < 0, "Save slots: deleting the active slot leaves no active slot");
			const int autosaves = saves.GetAutosaveCount();
			rig.PlayRolls(1);
			report.Check(saves.GetAutosaveCount() == autosaves && !saves.Describe(0).Occupied, "Save slots: with no active slot the day roll writes nothing");

			report.Check(GameSaves::ParseSlot("1") == 0 && GameSaves::ParseSlot("3") == 2 && GameSaves::ParseSlot("slot2") == 1 && GameSaves::ParseSlot("4") == -1
				&& GameSaves::ParseSlot("0") == -1 && GameSaves::ParseSlot("1x") == -1 && GameSaves::ParseSlot("") == -1 && GameSaves::SlotName(0) == "slot1",
				"Save slots: --load takes 1 to 3 or a slot's name");
		}
	}

	void RunSaveChecks(CheckReport& report)
	{
		SuiteScope scope(report, "Save");
		const std::filesystem::path root = Platform::GetExecutableDirectory() / "check-saves";
		std::error_code ec;
		std::filesystem::remove_all(root, ec);

		ResumeChecks(report, root);
		RoundTripChecks(report);
		StoreChecks(report, root);
		MigrationChecks(report);
		ProviderChecks(report, root);

		std::filesystem::remove_all(root, ec);
		report.Check(!std::filesystem::exists(root, ec), "Save: the checks leave no files behind");
	}

}
