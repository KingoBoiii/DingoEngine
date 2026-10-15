#include "Save/SaveGame.h"
#include "Defs/BuildDefs.h"
#include "Defs/DecorDefs.h"
#include "Defs/ItemDefs.h"
#include "World/Valley.h"
#include "World/ValleyLayout.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace Dingo
{

	namespace
	{
		constexpr int64_t DAY_LIMIT = 1'000'000;
		constexpr int64_t COUNT_LIMIT = 1'000'000'000;
		constexpr int64_t INT_LIMIT = (std::numeric_limits<int32_t>::max)();
		constexpr int64_t U32_LIMIT = (std::numeric_limits<uint32_t>::max)();
		constexpr std::array<char, 6> k_GroundChars = { 'g', 't', 'w', 'p', '~', '#' };
		static_assert(k_GroundChars.size() == static_cast<size_t>(Ground::Blocked) + 1);

		bool Fail(std::string& error, std::string message)
		{
			error = std::move(message);
			return false;
		}

		bool GroundFromChar(char c, Ground& out)
		{
			for (size_t i = 0; i < k_GroundChars.size(); ++i)
			{
				if (k_GroundChars[i] == c)
				{
					out = static_cast<Ground>(i);
					return true;
				}
			}
			return false;
		}

		bool WeatherFromName(std::string_view name, Weather& out)
		{
			for (Weather weather : { Weather::Clear, Weather::Rain, Weather::Snow })
			{
				if (name == ToString(weather))
				{
					out = weather;
					return true;
				}
			}
			return false;
		}

		bool ItemFromName(std::string_view name, ItemId& out)
		{
			for (int i = 0; i < ITEM_COUNT; ++i)
			{
				if (name == GetItemDef(static_cast<ItemId>(i)).Name)
				{
					out = static_cast<ItemId>(i);
					return true;
				}
			}
			return false;
		}

		bool ReadInt(const SaveData& value, int64_t min, int64_t max, int64_t& out)
		{
			if (!value.IsInt())
				return false;
			const int64_t number = value.AsInt();
			if (number < min || number > max)
				return false;
			out = number;
			return true;
		}

		template<typename T>
		bool ReadField(const SaveData& object, std::string_view key, int64_t min, int64_t max, T& out)
		{
			int64_t number = 0;
			if (!ReadInt(object[key], min, max, number))
				return false;
			out = static_cast<T>(number);
			return true;
		}

		bool ReadBool(const SaveData& object, std::string_view key, bool& out)
		{
			const SaveData& value = object[key];
			if (!value.IsBool())
				return false;
			out = value.AsBool();
			return true;
		}

		// Space-separated whole numbers, exactly N of them; the compact form of the grid's per-tile records.
		template<size_t N>
		bool ParseInts(const SaveData& value, std::array<int64_t, N>& out)
		{
			if (!value.IsString())
				return false;
			const std::string text = value.AsString();
			const char* p = text.data();
			const char* const end = p + text.size();
			for (size_t i = 0; i < N; ++i)
			{
				if (i > 0)
				{
					if (p == end || *p != ' ')
						return false;
					++p;
				}
				const auto [next, ec] = std::from_chars(p, end, out[i]);
				if (ec != std::errc() || next == p)
					return false;
				p = next;
			}
			return p == end;
		}

		std::string JoinInts(std::initializer_list<int64_t> values)
		{
			std::string text;
			for (const int64_t value : values)
			{
				if (!text.empty())
					text += ' ';
				text += std::to_string(value);
			}
			return text;
		}

		bool InRange(int64_t value, int64_t min, int64_t max) { return value >= min && value <= max; }

		int DefLimit(OccupantKind kind)
		{
			switch (kind)
			{
				case OccupantKind::Decoration: return DECOR_KIND_COUNT;
				case OccupantKind::Building:   return BUILD_COUNT;
				case OccupantKind::Landmark:   return LANDMARK_COUNT;
			}
			return 0;
		}

		SaveData EncodeGrid(const TileGrid& grid)
		{
			SaveData out = SaveData::MakeObject();
			out["width"] = grid.GetWidth();
			out["height"] = grid.GetHeight();
			SaveData& ground = out["ground"] = SaveData::MakeArray();
			SaveData& built = out["built"] = SaveData::MakeArray();
			SaveData& crops = out["crops"] = SaveData::MakeArray();
			for (int y = 0; y < grid.GetHeight(); ++y)
			{
				std::string row(static_cast<size_t>(grid.GetWidth()), 'g');
				for (int x = 0; x < grid.GetWidth(); ++x)
				{
					const Tile& tile = grid.At(x, y);
					row[static_cast<size_t>(x)] = k_GroundChars[static_cast<size_t>(tile.GroundState)];
					if (tile.Built != 0)
						built.Push(JoinInts({ x, y, tile.Built }));
					if (tile.Crop.Present())
						crops.Push(JoinInts({ x, y, tile.Crop.Def, tile.Crop.Stage, tile.Crop.DaysWatered, tile.Crop.PlantedDay }));
				}
				ground.Push(std::move(row));
			}
			SaveData& occupants = out["occupants"] = SaveData::MakeArray();
			for (const Occupant& o : grid.GetOccupants())
				occupants.Push(JoinInts({ o.Id, static_cast<int64_t>(o.Kind), o.DefId, o.X, o.Y, o.W, o.H, o.Rotation, o.Variant }));
			out["nextOccupant"] = grid.GetNextOccupantId();
			return out;
		}

		bool DecodeGrid(const SaveData& in, TileGrid& grid, std::string& error)
		{
			if (in["width"].AsInt(-1) != grid.GetWidth() || in["height"].AsInt(-1) != grid.GetHeight() || !in["width"].IsInt() || !in["height"].IsInt())
				return Fail(error, std::format("grid: the valley must be {} x {}", grid.GetWidth(), grid.GetHeight()));

			const SaveData& ground = in["ground"];
			if (!ground.IsArray() || ground.Size() != static_cast<size_t>(grid.GetHeight()))
				return Fail(error, "grid: ground needs one row per tile row");
			for (int y = 0; y < grid.GetHeight(); ++y)
			{
				const SaveData& rowValue = ground[static_cast<size_t>(y)];
				const std::string row = rowValue.AsString();
				if (!rowValue.IsString() || row.size() != static_cast<size_t>(grid.GetWidth()))
					return Fail(error, std::format("grid: ground row {} is not {} tiles", y, grid.GetWidth()));
				for (int x = 0; x < grid.GetWidth(); ++x)
				{
					Ground value = Ground::Grass;
					if (!GroundFromChar(row[static_cast<size_t>(x)], value))
						return Fail(error, std::format("grid: unknown ground '{}' at {}, {}", row[static_cast<size_t>(x)], x, y));
					grid.SetGround(x, y, value);
				}
			}

			const SaveData& built = in["built"];
			if (!built.IsArray())
				return Fail(error, "grid: built is not a list");
			for (size_t i = 0; i < built.Size(); ++i)
			{
				std::array<int64_t, 3> v{};
				if (!ParseInts(built[i], v) || !InRange(v[0], 0, grid.GetWidth() - 1) || !InRange(v[1], 0, grid.GetHeight() - 1) || !InRange(v[2], 1, BUILD_COUNT))
					return Fail(error, std::format("grid: built entry {} is malformed", i));
				Tile& tile = grid.At(static_cast<int>(v[0]), static_cast<int>(v[1]));
				if (tile.Built != 0)
					return Fail(error, std::format("grid: built entry {} repeats a tile", i));
				tile.Built = static_cast<uint8_t>(v[2]);
			}

			const SaveData& crops = in["crops"];
			if (!crops.IsArray())
				return Fail(error, "grid: crops is not a list");
			for (size_t i = 0; i < crops.Size(); ++i)
			{
				std::array<int64_t, 6> v{};
				if (!ParseInts(crops[i], v) || !InRange(v[0], 0, grid.GetWidth() - 1) || !InRange(v[1], 0, grid.GetHeight() - 1)
					|| !InRange(v[2], 0, CROP_COUNT - 1) || !InRange(v[3], 0, CROP_STAGES - 1) || !InRange(v[4], 0, 255) || !InRange(v[5], -DAY_LIMIT, DAY_LIMIT))
					return Fail(error, std::format("grid: crop entry {} is malformed", i));
				CropRecord& crop = grid.At(static_cast<int>(v[0]), static_cast<int>(v[1])).Crop;
				if (crop.Present())
					return Fail(error, std::format("grid: crop entry {} repeats a tile", i));
				crop.Def = static_cast<uint8_t>(v[2]);
				crop.Stage = static_cast<uint8_t>(v[3]);
				crop.DaysWatered = static_cast<uint8_t>(v[4]);
				crop.PlantedDay = static_cast<int32_t>(v[5]);
			}

			const SaveData& occupants = in["occupants"];
			if (!occupants.IsArray())
				return Fail(error, "grid: occupants is not a list");
			for (size_t i = 0; i < occupants.Size(); ++i)
			{
				std::array<int64_t, 9> v{};
				if (!ParseInts(occupants[i], v) || !InRange(v[0], 1, U32_LIMIT - 1) || !InRange(v[1], 0, static_cast<int64_t>(OccupantKind::Landmark))
					|| !InRange(v[3], 0, grid.GetWidth() - 1) || !InRange(v[4], 0, grid.GetHeight() - 1) || !InRange(v[5], 1, 255) || !InRange(v[6], 1, 255)
					|| !InRange(v[7], 0, 3) || !InRange(v[8], 0, 255))
					return Fail(error, std::format("grid: occupant entry {} is malformed", i));
				const OccupantKind kind = static_cast<OccupantKind>(v[1]);
				if (!InRange(v[2], 0, DefLimit(kind) - 1))
					return Fail(error, std::format("grid: occupant entry {} names an unknown def", i));

				Occupant occupant;
				occupant.Id = static_cast<uint32_t>(v[0]);
				occupant.Kind = kind;
				occupant.DefId = static_cast<uint16_t>(v[2]);
				occupant.X = static_cast<int16_t>(v[3]);
				occupant.Y = static_cast<int16_t>(v[4]);
				occupant.W = static_cast<uint8_t>(v[5]);
				occupant.H = static_cast<uint8_t>(v[6]);
				occupant.Rotation = static_cast<uint8_t>(v[7]);
				occupant.Variant = static_cast<uint8_t>(v[8]);
				if (!grid.RestoreOccupant(occupant))
					return Fail(error, std::format("grid: occupant {} overlaps another, leaves the valley or is out of order", occupant.Id));
			}

			int64_t next = 0;
			if (!ReadInt(in["nextOccupant"], 1, U32_LIMIT, next) || !grid.SetNextOccupantId(static_cast<uint32_t>(next)))
				return Fail(error, "grid: nextOccupant is missing or not past every occupant");
			return true;
		}

		SaveData EncodeCalendar(const Calendar& calendar)
		{
			SaveData out = SaveData::MakeObject();
			out["day"] = calendar.GetAbsoluteDay();
			out["tick"] = calendar.GetTickOfDay();
			out["weather"] = ToString(calendar.GetWeather());
			return out;
		}

		SaveData DefaultBonds()
		{
			SaveData bonds = SaveData::MakeArray();
			for (int i = 0; i < VILLAGER_COUNT; ++i)
			{
				SaveData& bond = bonds.Push(SaveData::MakeObject());
				bond["points"] = 0;
				bond["lastTalk"] = -1;
				bond["lastGift"] = -1;
				bond["giftsOnLastGift"] = 0;
			}
			return bonds;
		}

		// v0: the hand-written first layout, kept only to prove the migration path. Its clock and coins sat at the top
		// level, the ground was a sparse list of [x, y, ground] for every tile that is not grass, and it predates
		// friendship, animals and play time.
		bool MigrateFromV0(SaveData& save, std::string& error)
		{
			const SaveData& old = save;
			int64_t day = 0;
			int64_t tick = 0;
			int64_t weather = 0;
			int64_t coins = 0;
			if (!ReadInt(old["day"], 0, DAY_LIMIT, day) || !ReadInt(old["tick"], 0, DAY_TICKS - 1, tick) || !ReadInt(old["weather"], 0, static_cast<int64_t>(Weather::Snow), weather)
				|| !ReadInt(old["coins"], 0, INT_LIMIT, coins))
				return Fail(error, "day, tick, weather or coins is missing or out of range");
			if (!old["grid"].IsObject() || !old["pack"].IsObject())
				return Fail(error, "grid or pack is missing");

			const SaveData& gridIn = old["grid"];
			if (gridIn["width"].AsInt(-1) != VALLEY_SIZE || gridIn["height"].AsInt(-1) != VALLEY_SIZE)
				return Fail(error, std::format("grid: the valley must be {} x {}", VALLEY_SIZE, VALLEY_SIZE));
			const SaveData& tiles = gridIn["tiles"];
			if (!tiles.IsArray())
				return Fail(error, "grid: tiles is not a list");

			std::vector<std::string> rows(VALLEY_SIZE, std::string(VALLEY_SIZE, k_GroundChars[static_cast<size_t>(Ground::Grass)]));
			for (size_t i = 0; i < tiles.Size(); ++i)
			{
				const SaveData& tile = tiles[i];
				int64_t x = 0;
				int64_t y = 0;
				int64_t ground = 0;
				if (!tile.IsArray() || tile.Size() != 3 || !ReadInt(tile[0], 0, VALLEY_SIZE - 1, x) || !ReadInt(tile[1], 0, VALLEY_SIZE - 1, y)
					|| !ReadInt(tile[2], 0, static_cast<int64_t>(Ground::Blocked), ground))
					return Fail(error, std::format("grid: tile entry {} is malformed", i));
				rows[static_cast<size_t>(y)][static_cast<size_t>(x)] = k_GroundChars[static_cast<size_t>(ground)];
			}

			SaveData& grid = save["grid"];
			grid.Remove("tiles");
			SaveData& ground = grid["ground"] = SaveData::MakeArray();
			for (std::string& row : rows)
				ground.Push(std::move(row));

			SaveData& calendar = save["calendar"] = SaveData::MakeObject();
			calendar["day"] = day;
			calendar["tick"] = tick;
			calendar["weather"] = ToString(static_cast<Weather>(weather));
			save["pack"]["coins"] = coins;
			for (const char* key : { "day", "tick", "weather", "coins" })
				save.Remove(key);

			if (!save.Contains("bonds"))
				save["bonds"] = DefaultBonds();
			if (!save.Contains("animals"))
			{
				save["animals"]["nextId"] = 1;
				save["animals"]["list"] = SaveData::MakeArray();
			}
			if (!save.Contains("meta"))
				save["meta"]["playSeconds"] = 0.0;
			return true;
		}

		using MigrationStep = bool (*)(SaveData&, std::string&);
		// Entry N turns schema N into N + 1.
		constexpr std::array<MigrationStep, SAVE_SCHEMA> k_Migrations = { &MigrateFromV0 };
	}

	struct SaveCodec
	{
		static SaveData EncodePack(const Inventory& pack)
		{
			SaveData out = SaveData::MakeObject();
			out["coins"] = pack.m_Coins;
			out["selected"] = pack.m_Selected;
			SaveData& slots = out["slots"] = SaveData::MakeArray();
			for (size_t i = 0; i < pack.m_Slots.size(); ++i)
			{
				const ItemStack& stack = pack.m_Slots[i];
				if (stack.Empty())
					continue;
				SaveData& slot = slots.Push(SaveData::MakeObject());
				slot["slot"] = i;
				slot["item"] = GetItemDef(stack.Item).Name;
				slot["count"] = stack.Count;
			}
			SaveData& plans = out["plans"] = SaveData::MakeArray();
			for (size_t i = 0; i < pack.m_Plans.size(); ++i)
			{
				if (pack.m_Plans[i])
					plans.Push(i);
			}
			return out;
		}

		static bool DecodePack(const SaveData& in, Inventory& pack, std::string& error)
		{
			pack = Inventory();
			if (!ReadField(in, "coins", 0, INT_LIMIT, pack.m_Coins) || !ReadField(in, "selected", 0, HOTBAR_SLOTS - 1, pack.m_Selected))
				return Fail(error, "pack: coins or selected is missing or out of range");

			const SaveData& slots = in["slots"];
			if (!slots.IsArray())
				return Fail(error, "pack: slots is not a list");
			for (size_t i = 0; i < slots.Size(); ++i)
			{
				const SaveData& slot = slots[i];
				size_t index = 0;
				ItemId item = ItemId::Count;
				int count = 0;
				if (!ReadField(slot, "slot", 0, INVENTORY_SLOTS - 1, index) || !ItemFromName(slot["item"].AsString(), item) || !slot["item"].IsString()
					|| !ReadField(slot, "count", 1, GetItemDef(item).StackMax, count))
					return Fail(error, std::format("pack: slot entry {} is malformed or names an unknown item", i));
				if (!pack.m_Slots[index].Empty())
					return Fail(error, std::format("pack: slot {} is listed twice", index));
				pack.m_Slots[index] = ItemStack{ item, count };
			}

			const SaveData& plans = in["plans"];
			if (!plans.IsArray())
				return Fail(error, "pack: plans is not a list");
			for (size_t i = 0; i < plans.Size(); ++i)
			{
				int64_t id = 0;
				if (!ReadInt(plans[i], 0, BUILD_COUNT - 1, id))
					return Fail(error, std::format("pack: plan entry {} is not a buildable", i));
				pack.m_Plans[static_cast<size_t>(id)] = true;
			}
			return true;
		}

		static SaveData EncodeBin(const ShippingBin& bin)
		{
			SaveData out = SaveData::MakeObject();
			for (size_t i = 0; i < bin.m_Held.size(); ++i)
			{
				if (bin.m_Held[i] > 0)
					out[GetItemDef(static_cast<ItemId>(i)).Name] = bin.m_Held[i];
			}
			return out;
		}

		static bool DecodeBin(const SaveData& in, ShippingBin& bin, std::string& error)
		{
			bin = ShippingBin();
			if (!in.IsObject())
				return Fail(error, "bin is not an object");
			for (size_t i = 0; i < in.Size(); ++i)
			{
				ItemId item = ItemId::Count;
				int64_t count = 0;
				if (!ItemFromName(in.GetKey(i), item) || !ReadInt(in.At(i), 1, COUNT_LIMIT, count))
					return Fail(error, std::format("bin: '{}' is an unknown item or a bad count", in.GetKey(i)));
				bin.m_Held[static_cast<size_t>(item)] = static_cast<int>(count);
			}
			return true;
		}

		static SaveData EncodeBoard(const CommunityBoard& board)
		{
			SaveData out = SaveData::MakeObject();
			out["festival"] = board.m_Festival;
			out["tier"] = board.m_Tier;
			SaveData& boards = out["boards"] = SaveData::MakeArray();
			for (const BoardState& state : board.m_Boards)
			{
				SaveData& requests = boards.Push(SaveData::MakeArray());
				for (const RequestState& request : state.Requests)
				{
					SaveData& entry = requests.Push(SaveData::MakeArray());
					entry.Push(request.Progress);
					entry.Push(request.Baseline);
					entry.Push(request.Done);
				}
			}
			return out;
		}

		static bool DecodeBoard(const SaveData& in, CommunityBoard& board, std::string& error)
		{
			board = CommunityBoard();
			if (!ReadField(in, "festival", 0, static_cast<int64_t>(FestivalPhase::Held), board.m_Festival) || !ReadField(in, "tier", 0, 1000, board.m_Tier))
				return Fail(error, "board: festival or tier is missing or out of range");
			const SaveData& boards = in["boards"];
			if (!boards.IsArray() || boards.Size() != board.m_Boards.size())
				return Fail(error, std::format("board: boards needs {} entries", board.m_Boards.size()));
			for (size_t b = 0; b < board.m_Boards.size(); ++b)
			{
				const SaveData& requests = boards[b];
				if (!requests.IsArray() || requests.Size() != BOARD_REQUESTS)
					return Fail(error, std::format("board {}: needs {} requests", b, BOARD_REQUESTS));
				for (size_t r = 0; r < BOARD_REQUESTS; ++r)
				{
					const SaveData& entry = requests[r];
					int64_t progress = 0;
					int64_t baseline = 0;
					if (!entry.IsArray() || entry.Size() != 3 || !ReadInt(entry[0], 0, COUNT_LIMIT, progress) || !ReadInt(entry[1], 0, COUNT_LIMIT, baseline) || !entry[2].IsBool())
						return Fail(error, std::format("board {}: request {} is malformed", b, r));
					RequestState& request = board.m_Boards[b].Requests[r];
					request.Progress = static_cast<int>(progress);
					request.Baseline = static_cast<int>(baseline);
					request.Done = entry[2].AsBool();
				}
			}
			return true;
		}

		static SaveData EncodeBonds(const Friendship& bonds)
		{
			SaveData out = SaveData::MakeArray();
			for (const VillagerBond& bond : bonds.m_Bonds)
			{
				SaveData& entry = out.Push(SaveData::MakeObject());
				entry["points"] = bond.Points;
				entry["lastTalk"] = bond.LastTalkDay;
				entry["lastGift"] = bond.LastGiftDay;
				entry["giftsOnLastGift"] = bond.GiftsOnLastGiftDay;
			}
			return out;
		}

		static bool DecodeBonds(const SaveData& in, Friendship& bonds, std::string& error)
		{
			bonds = Friendship();
			if (!in.IsArray() || in.Size() != bonds.m_Bonds.size())
				return Fail(error, std::format("bonds: needs {} villagers", bonds.m_Bonds.size()));
			for (size_t i = 0; i < bonds.m_Bonds.size(); ++i)
			{
				VillagerBond& bond = bonds.m_Bonds[i];
				if (!ReadField(in[i], "points", 0, INT_LIMIT, bond.Points) || !ReadField(in[i], "lastTalk", -1, DAY_LIMIT, bond.LastTalkDay)
					|| !ReadField(in[i], "lastGift", -1, DAY_LIMIT, bond.LastGiftDay) || !ReadField(in[i], "giftsOnLastGift", 0, 1000, bond.GiftsOnLastGiftDay))
					return Fail(error, std::format("bonds: villager {} is malformed", i));
			}
			return true;
		}

		static SaveData EncodeHerd(const Herd& herd)
		{
			SaveData out = SaveData::MakeObject();
			out["nextId"] = herd.m_NextId;
			SaveData& list = out["list"] = SaveData::MakeArray();
			for (const Animal& animal : herd.m_Animals)
			{
				SaveData& entry = list.Push(SaveData::MakeObject());
				entry["id"] = animal.Id;
				entry["kind"] = animal.Kind;
				entry["home"] = animal.Home;
				entry["happiness"] = animal.Happiness;
				entry["fed"] = animal.Fed;
				entry["produce"] = animal.ProduceReady;
			}
			return out;
		}

		static bool DecodeHerd(const SaveData& in, Herd& herd, std::string& error)
		{
			herd = Herd();
			if (!ReadField(in, "nextId", 1, U32_LIMIT, herd.m_NextId))
				return Fail(error, "animals: nextId is missing");
			const SaveData& list = in["list"];
			if (!list.IsArray())
				return Fail(error, "animals: list is not a list");
			uint32_t last = 0;
			for (size_t i = 0; i < list.Size(); ++i)
			{
				const SaveData& entry = list[i];
				Animal animal;
				if (!ReadField(entry, "id", 1, U32_LIMIT, animal.Id) || !ReadField(entry, "kind", 0, static_cast<int64_t>(AnimalKind::Cow), animal.Kind)
					|| !ReadField(entry, "home", 0, U32_LIMIT, animal.Home) || !ReadField(entry, "happiness", 0, ANIMAL_HAPPINESS_MAX, animal.Happiness)
					|| !ReadBool(entry, "fed", animal.Fed) || !ReadBool(entry, "produce", animal.ProduceReady))
					return Fail(error, std::format("animals: entry {} is malformed", i));
				if (animal.Id <= last || animal.Id >= herd.m_NextId)
					return Fail(error, std::format("animals: entry {} is out of order or not below nextId", i));
				last = animal.Id;
				herd.m_Animals.push_back(animal);
			}
			return true;
		}
	};

	SaveData WriteSave(const ValleyDirector& director, const GameState& state, const SaveMeta& meta)
	{
		SaveData save = SaveData::MakeObject();
		save["schema"] = SAVE_SCHEMA;
		save["meta"]["playSeconds"] = meta.PlaySeconds;
		save["seed"] = director.GetSeed();
		save["calendar"] = EncodeCalendar(director.GetCalendar());
		save["grid"] = EncodeGrid(director.GetGrid());
		save["pack"] = SaveCodec::EncodePack(state.Pack);
		save["bin"] = SaveCodec::EncodeBin(state.Bin);
		save["board"] = SaveCodec::EncodeBoard(state.Board);
		save["bonds"] = SaveCodec::EncodeBonds(state.Bonds);
		save["animals"] = SaveCodec::EncodeHerd(state.Animals);
		return save;
	}

	int SaveSchemaOf(const SaveData& save)
	{
		const SaveData* schema = save.Find("schema");
		if (!schema)
			return 0;
		int64_t value = 0;
		return ReadInt(*schema, 0, INT_LIMIT, value) ? static_cast<int>(value) : -1;
	}

	bool MigrateSave(SaveData& save, std::string& error)
	{
		if (!save.IsObject())
			return Fail(error, "the file is not a save");
		int schema = SaveSchemaOf(save);
		if (schema < 0)
			return Fail(error, "the schema is not a whole number");
		if (schema > SAVE_SCHEMA)
			return Fail(error, std::format("schema {} is newer than this game's {}", schema, SAVE_SCHEMA));
		while (schema < SAVE_SCHEMA)
		{
			std::string stepError;
			if (!k_Migrations[static_cast<size_t>(schema)](save, stepError))
				return Fail(error, std::format("upgrading schema {}: {}", schema, stepError));
			++schema;
			save["schema"] = schema;
		}
		return true;
	}

	bool ReadSave(const SaveData& save, LoadedGame& out, std::string& error)
	{
		SaveData migrated = save;
		if (!MigrateSave(migrated, error))
			return false;
		const SaveData& in = migrated;

		LoadedGame game;
		const SaveData& meta = in["meta"];
		const double played = meta["playSeconds"].AsDouble(-1.0);
		if (!meta["playSeconds"].IsNumber() || !std::isfinite(played) || played < 0.0)
			return Fail(error, "meta: playSeconds is missing or negative");
		game.Meta.PlaySeconds = played;

		const SaveData& calendar = in["calendar"];
		if (!ReadField(in, "seed", 0, U32_LIMIT, game.Seed) || !ReadField(calendar, "day", 0, DAY_LIMIT, game.Day) || !ReadField(calendar, "tick", 0, DAY_TICKS - 1, game.Tick)
			|| !calendar["weather"].IsString() || !WeatherFromName(calendar["weather"].AsString(), game.Today))
			return Fail(error, "seed or calendar is missing or out of range");

		if (!DecodeGrid(in["grid"], game.Grid, error) || !SaveCodec::DecodePack(in["pack"], game.State.Pack, error) || !SaveCodec::DecodeBin(in["bin"], game.State.Bin, error)
			|| !SaveCodec::DecodeBoard(in["board"], game.State.Board, error) || !SaveCodec::DecodeBonds(in["bonds"], game.State.Bonds, error)
			|| !SaveCodec::DecodeHerd(in["animals"], game.State.Animals, error))
			return false;

		out = std::move(game);
		return true;
	}

	void ApplyLoadedGame(LoadedGame&& game, ValleyDirector& director, GameState& state)
	{
		director.Restore(game.Seed, std::move(game.Grid), game.Day, game.Tick, game.Today);
		state = std::move(game.State);
	}

	LoadedGame MakeNewGame(uint32_t seed)
	{
		LoadedGame game;
		game.Seed = seed;
		GenerateValley(game.Grid, seed);
		Calendar calendar(seed);
		calendar.Reset(0, 0);
		game.Today = calendar.GetWeather();
		game.State.Pack.GiveStarterLoadout();
		return game;
	}

	std::string DescribeGame(const LoadedGame& game)
	{
		std::string season = ToString(Calendar::SeasonOfDay(game.Day));
		if (!season.empty() && season[0] >= 'a' && season[0] <= 'z')
			season[0] = static_cast<char>(season[0] - 'a' + 'A');
		const int64_t minutes = static_cast<int64_t>(game.Meta.PlaySeconds / 60.0);
		const std::string played = minutes >= 60 ? std::format("{}h {}m", minutes / 60, minutes % 60) : std::format("{}m", minutes);
		return std::format("{} {}, year {} - {} coins - {} played", season, Calendar::DayOfSeasonOf(game.Day), game.Day / DAYS_PER_YEAR + 1, game.State.Pack.GetCoins(), played);
	}

}
