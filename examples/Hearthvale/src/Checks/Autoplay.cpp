#include "Checks/Autoplay.h"
#include "Defs/BoardDefs.h"
#include "GameTuning.h"
#include "Life/PlayerMath.h"
#include "Life/Schedule.h"
#include "Placing/Targeting.h"
#include "World/ValleyLayout.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace Dingo
{

	namespace
	{
		constexpr int FIELD_FIRST_X = FARM_PLOT.X + 1;
		constexpr int FIELD_COLUMNS = FARM_PLOT.W - 2;
		constexpr int LANE_NORTH = FARM_PLOT.Y;
		constexpr int LANE_MIDDLE = FARM_PLOT.Y + 3;
		constexpr int LANE_SOUTH = FARM_PLOT.Y + 6;

		constexpr int FENCE_RUN_LENGTH = 10;
		constexpr int FENCE_ROWS[] = { FARM_PLOT.Bottom() + 1, FARM_PLOT.Bottom() + 2, FARM_PLOT.Bottom() + 3, FARM_PLOT.Y - 2, FARM_PLOT.Y - 3 };
		constexpr int FENCE_WINDOW = 14;
		constexpr glm::ivec2 LAMP_SPOTS[] = { { 22, 46 }, { 22, 44 }, { 22, 48 }, { 21, 46 }, { 22, 50 } };

		constexpr int ACTION_GAP_STEPS = 8;
		constexpr int FIELD_ATTEMPTS = 6;
		constexpr int PLACE_ATTEMPTS = 3;
		constexpr int STOP_STEP_LIMIT = 12000;
		constexpr int BED_RETRY_STEPS = 600;
		constexpr int SHOP_MIN_SEEDS = 2;
		constexpr int SEED_RESERVE = 20;
		constexpr int MATERIAL_RESERVE = 60;
		constexpr int GIFT_RETRIES = 3;
		constexpr float GIFT_REACH = 2.5f;
		constexpr int GIFT_POLL_STEPS = 120;
		constexpr int GIFT_GIVE_UP_TICK = DAY_TICKS * 4 / 5;

		glm::vec2 CentreOf(const glm::ivec2& tile)
		{
			return glm::vec2((static_cast<float>(tile.x) + 0.5f) * TILE_SIZE, (static_cast<float>(tile.y) + 0.5f) * TILE_SIZE);
		}

		glm::ivec2 TileAt(const glm::vec2& position)
		{
			const TileCoord tile = Targeting::TileOf(position);
			return glm::ivec2(tile.X, tile.Y);
		}

		// Profit per tile-day over what is left of the season; zero when the crop cannot ripen in time.
		float CropScore(const CropDef& def, int daysLeft)
		{
			if (def.GrowthDays > daysLeft)
				return 0.0f;
			const int harvests = def.RegrowDays > 0 ? 1 + (daysLeft - def.GrowthDays) / def.RegrowDays : 1;
			const int span = def.GrowthDays + (harvests - 1) * def.RegrowDays;
			const float profit = static_cast<float>(def.SellPrice * def.HarvestYield * harvests - def.SeedPrice);
			return profit / static_cast<float>(span);
		}

		int HotbarSlotOf(const Inventory& pack, ItemId item)
		{
			const int slot = pack.FindSlot(item);
			return slot >= 0 && slot < HOTBAR_SLOTS ? slot : -1;
		}
	}

	std::string FormatDayLine(const ValleyDirector& director, const GameState& state)
	{
		const Calendar& calendar = director.GetCalendar();
		const int finished = calendar.GetAbsoluteDay();
		const int last = (std::max)(0, finished - 1);
		const TileGrid& grid = director.GetGrid();
		int crops = 0;
		for (int y = 0; y < grid.GetHeight(); ++y)
		{
			for (int x = 0; x < grid.GetWidth(); ++x)
			{
				if (grid.At(x, y).Crop.Present())
					++crops;
			}
		}
		return std::format("[Day] {} {} {} coins {} crops {} bin {} board {}/{} hash {:016x}", finished, ToString(Calendar::SeasonOfDay(last)),
			Calendar::DayOfSeasonOf(last), state.Pack.GetCoins(), crops, state.Bin.PendingValue(), state.Board.DoneCount(),
			CommunityBoard::BoardCount * BOARD_REQUESTS, HashGameState(director, state).All);
	}

	std::string FormatAutoplayStats(const AutoplayStats& stats)
	{
		return std::format("tilled {} planted {} watered {} harvested {} shipped {} seeds bought {} fences {} lamps {} gifts {} nights {}", stats.Tilled,
			stats.Planted, stats.Watered, stats.Harvested, stats.Shipped, stats.SeedsBought, stats.Fences, stats.Lamps, stats.Gifts, stats.Nights);
	}

	AutoplayFarmer::AutoplayFarmer(ValleyDirector& director, GameState& state, BuildController& build, SleepFlow& sleep, const VillagerSim* villagers)
		: m_Director(director), m_State(state), m_Build(build), m_Sleep(sleep), m_Villagers(villagers)
	{
		Reset();
	}

	void AutoplayFarmer::Reset()
	{
		const LandmarkSpec& home = GetLandmarkSpec(Landmark::FarmHouse);
		m_Feet = CentreOf(glm::ivec2(home.DoorX, home.DoorY + 1));
		m_Yaw = 0.0f;
		m_Heading = glm::vec2(0.0f);
		m_PlanDay = -1;
		m_Stops.clear();
		m_HasStop = false;
		m_Walking = false;
		m_Route.clear();
		m_RouteIndex = 0;
		m_Pending = FieldNeed::None;
		m_FenceStart = glm::ivec2(-1);
		m_Idle = 0;
	}

	void AutoplayFarmer::Step(bool inputBlocked)
	{
		PlayerIntent intent;
		m_Heading = glm::vec2(0.0f);
		if (!inputBlocked && !m_Sleep.BlocksInput())
		{
			const int day = m_Director.GetCalendar().GetAbsoluteDay();
			if (day != m_PlanDay)
				PlanDay(day);
			Act(intent);
		}
		AimInput aim;
		aim.Feet = m_Feet;
		aim.Yaw = m_Yaw;
		m_Build.Step(intent, aim);
	}

	void AutoplayFarmer::PlanDay(int day)
	{
		if (m_PlanDay >= 0)
			++m_Stats.Nights;
		m_PlanDay = day;
		m_Stops.clear();
		m_HasStop = false;
		m_Walking = false;
		m_Route.clear();
		m_RouteIndex = 0;
		m_BedSteps = 0;
		m_Idle = 0;
		m_Pending = FieldNeed::None;

		const LandmarkSpec& shop = GetLandmarkSpec(Landmark::Shop);
		const LandmarkSpec& bin = GetLandmarkSpec(Landmark::ShippingBin);
		const LandmarkSpec& home = GetLandmarkSpec(Landmark::FarmHouse);

		m_Stops.push_back(Stop{ StopKind::Shop, glm::ivec2(shop.DoorX, shop.DoorY), glm::ivec2(shop.DoorX, shop.DoorY - 1) });
		for (int column = 0; column < FIELD_COLUMNS; ++column)
		{
			const int x = FIELD_FIRST_X + column;
			const Stop column_stops[] = {
				Stop{ StopKind::Field, glm::ivec2(x, LANE_NORTH), glm::ivec2(x, LANE_NORTH + 1) },
				Stop{ StopKind::Field, glm::ivec2(x, LANE_MIDDLE), glm::ivec2(x, LANE_MIDDLE - 1) },
				Stop{ StopKind::Field, glm::ivec2(x, LANE_MIDDLE), glm::ivec2(x, LANE_MIDDLE + 1) },
				Stop{ StopKind::Field, glm::ivec2(x, LANE_SOUTH), glm::ivec2(x, LANE_SOUTH - 1) },
			};
			if (column % 2 == 0)
				m_Stops.insert(m_Stops.end(), std::begin(column_stops), std::end(column_stops));
			else
				m_Stops.insert(m_Stops.end(), std::rbegin(column_stops), std::rend(column_stops));
		}
		m_Stops.push_back(Stop{ StopKind::BuildRun });
		m_Stops.push_back(Stop{ StopKind::Bin, glm::ivec2(bin.DoorX, bin.DoorY), glm::ivec2(bin.DoorX, bin.DoorY - 1) });
		m_Stops.push_back(Stop{ StopKind::Gift });
		m_Stops.push_back(Stop{ StopKind::Bed, glm::ivec2(home.DoorX, home.DoorY + 1), glm::ivec2(home.DoorX, home.DoorY) });
	}

	void AutoplayFarmer::Act(PlayerIntent& intent)
	{
		if (m_Idle > 0)
		{
			--m_Idle;
			return;
		}
		if (!m_HasStop && !StartStop())
			return;

		++m_StopSteps;
		if (m_Current.Kind != StopKind::Bed && m_StopSteps > STOP_STEP_LIMIT)
		{
			FinishStop();
			return;
		}

		if (m_Walking)
		{
			if (Walk())
				Arrive();
			return;
		}

		const bool placing = m_Current.Kind == StopKind::Fence || m_Current.Kind == StopKind::Lamp;
		if ((m_Build.GetMode() == BuildMode::Build) != placing)
		{
			intent.ToggleBuild = true;
			if (placing)
				intent.HotbarSlot = static_cast<int>(m_Current.Kind == StopKind::Fence ? BuildId::WoodFence : BuildId::LampPost);
			return;
		}

		if (Work(intent))
			FinishStop();
	}

	bool AutoplayFarmer::StartStop()
	{
		const TileGrid& grid = m_Director.GetGrid();
		while (!m_Stops.empty())
		{
			Stop stop = m_Stops.front();
			m_Stops.pop_front();

			switch (stop.Kind)
			{
				case StopKind::BuildRun:
					ExpandBuildRun();
					continue;
				case StopKind::Field:
					if (NeedOf(stop.Face.x, stop.Face.y) == FieldNeed::None)
						continue;
					break;
				case StopKind::Shop:
					if (!WantsShop())
						continue;
					break;
				case StopKind::Bin:
					if (ShippableCount() <= 0)
						continue;
					break;
				case StopKind::Gift:
				{
					VillagerId who = VillagerId::Count;
					ItemId item = ItemId::Count;
					if (!m_Villagers || !FindGiftRequest(who, item) || m_State.Pack.Count(item) <= 0)
						continue;
					const VillagerPlace& place = m_Villagers->GetPlace(who);
					const glm::ivec2 tile = TileAt(place.Position);
					if (place.Indoors || !PlayerMath::IsWalkable(grid, tile.x, tile.y))
					{
						if (m_Director.GetCalendar().GetTickOfDay() < GIFT_GIVE_UP_TICK)
						{
							m_Stops.push_front(stop);
							m_Idle = GIFT_POLL_STEPS;
							return false;
						}
						continue;
					}
					stop.Who = who;
					stop.Stand = tile;
					stop.Face = tile;
					break;
				}
				default:
					break;
			}

			std::vector<glm::ivec2> route = FindRoute(grid, TileAt(m_Feet), stop.Stand);
			if (route.empty())
				continue;

			m_Current = stop;
			m_HasStop = true;
			m_Route = std::move(route);
			m_RouteIndex = 0;
			m_Walking = true;
			m_StopSteps = 0;
			m_Attempts = 0;
			m_ActionGap = 0;
			m_Pending = FieldNeed::None;
			return true;
		}
		return false;
	}

	void AutoplayFarmer::FinishStop()
	{
		m_HasStop = false;
		m_Walking = false;
		m_Route.clear();
		m_RouteIndex = 0;
	}

	bool AutoplayFarmer::Walk()
	{
		const TileGrid& grid = m_Director.GetGrid();
		if (m_RouteIndex >= m_Route.size())
			return true;

		const glm::ivec2 next = m_Route[m_RouteIndex];
		if (!PlayerMath::IsWalkable(grid, next.x, next.y))
		{
			FinishStop();
			return false;
		}

		const glm::vec2 delta = CentreOf(next) - m_Feet;
		const float distance = glm::length(delta);
		const float reach = PLAYER_SPEED * SIM_STEP_SECONDS;
		if (distance <= reach)
		{
			m_Feet = CentreOf(next);
			++m_RouteIndex;
			if (distance > 0.0f)
			{
				m_Heading = delta / distance;
				m_Yaw = PlayerMath::YawOf(m_Heading);
			}
			return m_RouteIndex >= m_Route.size();
		}

		m_Heading = delta / distance;
		m_Yaw = PlayerMath::YawOf(m_Heading);
		m_Feet += m_Heading * reach;
		return false;
	}

	void AutoplayFarmer::Arrive()
	{
		m_Walking = false;
		if (m_Current.Face != m_Current.Stand)
			Face(m_Current.Stand, m_Current.Face);
	}

	void AutoplayFarmer::Face(const glm::ivec2& from, const glm::ivec2& to)
	{
		m_Yaw = PlayerMath::YawOf(glm::vec2(to - from));
	}

	bool AutoplayFarmer::Work(PlayerIntent& intent)
	{
		switch (m_Current.Kind)
		{
			case StopKind::Field:
				return WorkField(intent);
			case StopKind::Shop:
				Shop();
				return true;
			case StopKind::Bin:
				ShipHarvest();
				return true;
			case StopKind::Fence:
			case StopKind::Lamp:
				return WorkPlace(intent);
			case StopKind::Gift:
				return GiveGiftNow();
			case StopKind::Bed:
				return WorkBed(intent);
			default:
				return true;
		}
	}

	bool AutoplayFarmer::WorkField(PlayerIntent& intent)
	{
		const FieldNeed need = NeedOf(m_Current.Face.x, m_Current.Face.y);
		if (m_Pending != FieldNeed::None && need != m_Pending)
		{
			switch (m_Pending)
			{
				case FieldNeed::Harvest: ++m_Stats.Harvested; break;
				case FieldNeed::Till:    ++m_Stats.Tilled; break;
				case FieldNeed::Plant:   ++m_Stats.Planted; break;
				case FieldNeed::Water:   ++m_Stats.Watered; break;
				default: break;
			}
		}
		m_Pending = FieldNeed::None;

		if (need == FieldNeed::None || m_Attempts >= FIELD_ATTEMPTS)
			return true;
		if (m_ActionGap > 0)
		{
			--m_ActionGap;
			return false;
		}

		Inventory& pack = m_State.Pack;
		switch (need)
		{
			case FieldNeed::Harvest:
				intent.Interact = true;
				break;
			case FieldNeed::Till:
			case FieldNeed::Water:
			{
				const int slot = HotbarSlotOf(pack, need == FieldNeed::Till ? ItemId::Hoe : ItemId::WateringCan);
				if (slot < 0)
					return true;
				intent.HotbarSlot = slot;
				intent.Use = true;
				break;
			}
			case FieldNeed::Plant:
				m_Build.SelectSeed(ChooseSeed());
				intent.Interact = true;
				break;
			default:
				return true;
		}
		m_Pending = need;
		++m_Attempts;
		m_ActionGap = ACTION_GAP_STEPS;
		return false;
	}

	bool AutoplayFarmer::WorkPlace(PlayerIntent& intent)
	{
		const BuildId id = m_Current.Kind == StopKind::Fence ? BuildId::WoodFence : BuildId::LampPost;
		const PlaceSpec spec{ id, m_Current.Face.x, m_Current.Face.y, 0 };
		if (m_Attempts > 0)
		{
			if (CheckPlacement(m_Director.GetGrid(), m_State.Pack, spec) == PlaceResult::Occupied)
			{
				++(id == BuildId::WoodFence ? m_Stats.Fences : m_Stats.Lamps);
				return true;
			}
			if (m_Attempts >= PLACE_ATTEMPTS)
				return true;
		}
		if (m_ActionGap > 0)
		{
			--m_ActionGap;
			return false;
		}
		intent.HotbarSlot = static_cast<int>(id);
		intent.Use = true;
		++m_Attempts;
		m_ActionGap = ACTION_GAP_STEPS;
		return false;
	}

	bool AutoplayFarmer::WorkBed(PlayerIntent& intent)
	{
		if (m_BedSteps % BED_RETRY_STEPS == 0)
			intent.Interact = true;
		++m_BedSteps;
		return false;
	}

	void AutoplayFarmer::Shop()
	{
		Inventory& pack = m_State.Pack;
		const Materials missing = MissingMaterials();
		if (missing.Wood > 0)
			(void)ShopBuyItem(pack, ItemId::Wood, missing.Wood);
		if (missing.Stone > 0)
			(void)ShopBuyItem(pack, ItemId::Stone, missing.Stone);

		const CropId gift = GiftCrop();
		if (gift != CropId::Count && pack.Count(SeedItem(gift)) == 0)
			(void)ShopBuyItem(pack, SeedItem(gift), 1);

		const SeedBuy seeds = PlanSeeds();
		if (seeds.Crop != CropId::Count && seeds.Count > 0 && ShopBuyItem(pack, SeedItem(seeds.Crop), seeds.Count) == TradeResult::Ok)
			m_Stats.SeedsBought += seeds.Count;
	}

	void AutoplayFarmer::ShipHarvest()
	{
		Inventory& pack = m_State.Pack;
		const ItemId kept = KeptForGift();
		for (int i = 0; i < CROP_COUNT; ++i)
		{
			const ItemId item = HarvestItem(static_cast<CropId>(i));
			const int count = pack.Count(item) - (item == kept ? 1 : 0);
			if (count > 0 && m_State.Bin.Ship(pack, item, count) == TradeResult::Ok)
				m_Stats.Shipped += count;
		}
	}

	int AutoplayFarmer::ShippableCount() const
	{
		const Inventory& pack = m_State.Pack;
		const ItemId kept = KeptForGift();
		int total = 0;
		for (int i = 0; i < CROP_COUNT; ++i)
		{
			const ItemId item = HarvestItem(static_cast<CropId>(i));
			total += (std::max)(0, pack.Count(item) - (item == kept ? 1 : 0));
		}
		return total;
	}

	bool AutoplayFarmer::GiveGiftNow()
	{
		VillagerId who = VillagerId::Count;
		ItemId item = ItemId::Count;
		if (!FindGiftRequest(who, item) || who != m_Current.Who || m_State.Pack.Count(item) <= 0)
			return true;

		const VillagerPlace& place = m_Villagers->GetPlace(who);
		if (place.Indoors || glm::length(place.Position - m_Feet) > GIFT_REACH)
		{
			if (m_Current.Retries < GIFT_RETRIES)
			{
				Stop again = m_Current;
				++again.Retries;
				m_Stops.push_front(again);
			}
			return true;
		}

		const int day = m_Director.GetCalendar().GetAbsoluteDay();
		const GiftOutcome outcome = m_State.Bonds.Evaluate(who, item, day);
		if (outcome == GiftOutcome::Loved || outcome == GiftOutcome::Fine)
		{
			GiveGift(m_State.Pack, m_State.Board, who, item, day);
			m_State.Bonds.Give(who, item, day);
			++m_Stats.Gifts;
		}
		return true;
	}

	void AutoplayFarmer::ExpandBuildRun()
	{
		const TileGrid& grid = m_Director.GetGrid();
		const Inventory& pack = m_State.Pack;
		std::vector<Stop> placed;

		const BuildCost fence = GetBuildDef(BuildId::WoodFence).Cost;
		int remaining = FENCE_RUN_LENGTH - FencesStanding();
		int wood = pack.Count(ItemId::Wood);
		if (remaining > 0 && m_FenceStart.x < 0)
			m_FenceStart = FindFenceRun();
		for (int i = 0; m_FenceStart.x >= 0 && i < FENCE_WINDOW && remaining > 0 && wood >= fence.Wood; ++i)
		{
			const int x = m_FenceStart.x + i;
			const PlaceSpec spec{ BuildId::WoodFence, x, m_FenceStart.y, 0 };
			const glm::ivec2 stand(x, m_FenceStart.y + 1);
			if (CheckPlacement(grid, pack, spec) != PlaceResult::Ok || !PlayerMath::IsWalkable(grid, stand.x, stand.y))
				continue;
			placed.push_back(Stop{ StopKind::Fence, stand, glm::ivec2(x, m_FenceStart.y) });
			--remaining;
			wood -= fence.Wood;
		}

		const BuildCost lamp = GetBuildDef(BuildId::LampPost).Cost;
		if (!HasBuilding(BuildId::LampPost) && wood >= lamp.Wood && pack.Count(ItemId::Stone) >= lamp.Stone && pack.GetCoins() >= lamp.Coins)
		{
			for (const glm::ivec2& spot : LAMP_SPOTS)
			{
				const glm::ivec2 stand(spot.x + 1, spot.y);
				if (CheckPlacement(grid, pack, PlaceSpec{ BuildId::LampPost, spot.x, spot.y, 0 }) != PlaceResult::Ok
					|| !PlayerMath::IsWalkable(grid, stand.x, stand.y))
					continue;
				placed.push_back(Stop{ StopKind::Lamp, stand, spot });
				break;
			}
		}

		for (auto it = placed.rbegin(); it != placed.rend(); ++it)
			m_Stops.push_front(*it);
	}

	glm::ivec2 AutoplayFarmer::FindFenceRun() const
	{
		const TileGrid& grid = m_Director.GetGrid();
		const Inventory& pack = m_State.Pack;
		glm::ivec2 best(-1);
		int bestCount = 0;
		for (const int row : FENCE_ROWS)
		{
			for (int first = FARM_CLEARING.X; first + FENCE_WINDOW <= FARM_CLEARING.Right(); ++first)
			{
				int count = 0;
				for (int i = 0; i < FENCE_WINDOW && count < FENCE_RUN_LENGTH; ++i)
				{
					const PlaceResult result = CheckPlacement(grid, pack, PlaceSpec{ BuildId::WoodFence, first + i, row, 0 });
					if ((result == PlaceResult::Ok || result == PlaceResult::CannotAfford) && PlayerMath::IsWalkable(grid, first + i, row + 1))
						++count;
				}
				if (count > bestCount)
				{
					bestCount = count;
					best = glm::ivec2(first, row);
				}
			}
		}
		return best;
	}

	AutoplayFarmer::FieldNeed AutoplayFarmer::NeedOf(int x, int y) const
	{
		const TileGrid& grid = m_Director.GetGrid();
		if (!grid.InBounds(x, y))
			return FieldNeed::None;

		const Tile& tile = grid.At(x, y);
		if (tile.Crop.Present())
		{
			if (IsRipe(tile.Crop))
				return FieldNeed::Harvest;
			return tile.GroundState == Ground::Tilled ? FieldNeed::Water : FieldNeed::None;
		}

		const bool seeds = ChooseSeed() != CropId::Count;
		if (tile.GroundState == Ground::Grass)
			return seeds ? FieldNeed::Till : FieldNeed::None;
		if (tile.GroundState == Ground::Tilled || tile.GroundState == Ground::Watered)
			return seeds ? FieldNeed::Plant : FieldNeed::None;
		return FieldNeed::None;
	}

	int AutoplayFarmer::DaysLeftInSeason() const
	{
		return DAYS_PER_SEASON - m_Director.GetCalendar().GetDayOfSeason();
	}

	CropId AutoplayFarmer::GiftCrop() const
	{
		VillagerId who = VillagerId::Count;
		ItemId item = ItemId::Count;
		if (!FindGiftRequest(who, item) || m_State.Pack.Count(item) > 0)
			return CropId::Count;

		const Season season = m_Director.GetCalendar().GetSeason();
		const TileGrid& grid = m_Director.GetGrid();
		for (int i = 0; i < CROP_COUNT; ++i)
		{
			const CropId crop = static_cast<CropId>(i);
			const CropDef& def = GetCropDef(crop);
			if (HarvestItem(crop) != item)
				continue;
			if (def.GrowSeason != season || def.GrowthDays > DaysLeftInSeason())
				return CropId::Count;
			for (int y = FARM_PLOT.Y; y < FARM_PLOT.Bottom(); ++y)
			{
				for (int x = FARM_PLOT.X; x < FARM_PLOT.Right(); ++x)
				{
					if (grid.At(x, y).Crop.Def == static_cast<uint8_t>(crop))
						return CropId::Count;
				}
			}
			return crop;
		}
		return CropId::Count;
	}

	CropId AutoplayFarmer::ChooseSeed() const
	{
		const CropId gift = GiftCrop();
		if (gift != CropId::Count && m_State.Pack.Count(SeedItem(gift)) > 0)
			return gift;
		const Season season = m_Director.GetCalendar().GetSeason();
		const int daysLeft = DaysLeftInSeason();
		CropId best = CropId::Count;
		float bestScore = 0.0f;
		for (int i = 0; i < CROP_COUNT; ++i)
		{
			const CropId crop = static_cast<CropId>(i);
			const CropDef& def = GetCropDef(crop);
			if (def.GrowSeason != season || m_State.Pack.Count(SeedItem(crop)) <= 0)
				continue;
			const float score = CropScore(def, daysLeft);
			if (score > bestScore)
			{
				best = crop;
				bestScore = score;
			}
		}
		return best;
	}

	CropId AutoplayFarmer::BestCrop() const
	{
		const Season season = m_Director.GetCalendar().GetSeason();
		const int daysLeft = DaysLeftInSeason();
		CropId best = CropId::Count;
		float bestScore = 0.0f;
		for (int i = 0; i < CROP_COUNT; ++i)
		{
			const CropDef& def = GetCropDef(static_cast<CropId>(i));
			if (def.GrowSeason != season)
				continue;
			const float score = CropScore(def, daysLeft);
			if (score > bestScore)
			{
				best = def.Id;
				bestScore = score;
			}
		}
		return best;
	}

	int AutoplayFarmer::FreeFieldTiles() const
	{
		const TileGrid& grid = m_Director.GetGrid();
		const int rows[] = { LANE_NORTH + 1, LANE_MIDDLE - 1, LANE_MIDDLE + 1, LANE_SOUTH - 1 };
		int free = 0;
		for (int column = 0; column < FIELD_COLUMNS; ++column)
		{
			for (const int y : rows)
			{
				const Tile& tile = grid.At(FIELD_FIRST_X + column, y);
				if (!tile.Crop.Present() || IsRipe(tile.Crop))
					++free;
			}
		}
		return free;
	}

	int AutoplayFarmer::SeedsForSeason() const
	{
		const Season season = m_Director.GetCalendar().GetSeason();
		const int daysLeft = DaysLeftInSeason();
		int seeds = 0;
		for (int i = 0; i < CROP_COUNT; ++i)
		{
			const CropDef& def = GetCropDef(static_cast<CropId>(i));
			if (def.GrowSeason == season && CropScore(def, daysLeft) > 0.0f)
				seeds += m_State.Pack.Count(SeedItem(def.Id));
		}
		return seeds;
	}

	int AutoplayFarmer::FencesStanding() const
	{
		int count = 0;
		for (const Occupant& occupant : m_Director.GetGrid().GetOccupants())
		{
			if (occupant.Kind == OccupantKind::Building && occupant.DefId == static_cast<uint16_t>(BuildId::WoodFence))
				++count;
		}
		return count;
	}

	bool AutoplayFarmer::HasBuilding(BuildId id) const
	{
		for (const Occupant& occupant : m_Director.GetGrid().GetOccupants())
		{
			if (occupant.Kind == OccupantKind::Building && occupant.DefId == static_cast<uint16_t>(id))
				return true;
		}
		return false;
	}

	AutoplayFarmer::Materials AutoplayFarmer::MissingMaterials() const
	{
		const Inventory& pack = m_State.Pack;
		const BuildCost fence = GetBuildDef(BuildId::WoodFence).Cost;
		const BuildCost lamp = GetBuildDef(BuildId::LampPost).Cost;
		const bool lampMissing = !HasBuilding(BuildId::LampPost);

		Materials missing;
		missing.Wood = (std::max)(0, (std::max)(0, FENCE_RUN_LENGTH - FencesStanding()) * fence.Wood + (lampMissing ? lamp.Wood : 0) - pack.Count(ItemId::Wood));
		missing.Stone = (std::max)(0, (lampMissing ? lamp.Stone : 0) - pack.Count(ItemId::Stone));

		const int cost = missing.Wood * GetItemDef(ItemId::Wood).BuyPrice + missing.Stone * GetItemDef(ItemId::Stone).BuyPrice;
		if (pack.GetCoins() < cost + (lampMissing ? lamp.Coins : 0) + MATERIAL_RESERVE)
			return Materials{};
		return missing;
	}

	AutoplayFarmer::SeedBuy AutoplayFarmer::PlanSeeds() const
	{
		SeedBuy plan;
		plan.Crop = BestCrop();
		if (plan.Crop == CropId::Count)
			return plan;

		const Materials owed = MissingMaterials();
		const bool lampMissing = !HasBuilding(BuildId::LampPost);
		const int spoken = owed.Wood * GetItemDef(ItemId::Wood).BuyPrice + owed.Stone * GetItemDef(ItemId::Stone).BuyPrice
			+ (lampMissing ? GetBuildDef(BuildId::LampPost).Cost.Coins : 0);
		const int budget = m_State.Pack.GetCoins() - spoken - SEED_RESERVE;
		const int price = GetCropDef(plan.Crop).SeedPrice;
		const int wanted = FreeFieldTiles() - SeedsForSeason();
		plan.Count = (std::max)(0, (std::min)({ wanted, budget / price, m_State.Pack.Room(SeedItem(plan.Crop)) }));
		return plan;
	}

	bool AutoplayFarmer::WantsShop() const
	{
		const Materials missing = MissingMaterials();
		if (missing.Wood > 0 || missing.Stone > 0)
			return true;
		const CropId gift = GiftCrop();
		if (gift != CropId::Count && m_State.Pack.Count(SeedItem(gift)) == 0)
			return true;
		return PlanSeeds().Count >= SHOP_MIN_SEEDS;
	}

	bool AutoplayFarmer::FindGiftRequest(VillagerId& who, ItemId& item) const
	{
		const int day = m_Director.GetCalendar().GetAbsoluteDay();
		for (int board = 0; board < CommunityBoard::BoardCount; ++board)
		{
			if (!m_State.Board.IsOpen(board, day))
				continue;
			const BoardDef& def = GetBoardDef(static_cast<Season>(board));
			for (int i = 0; i < BOARD_REQUESTS; ++i)
			{
				const BoardRequest& request = def.Requests[static_cast<size_t>(i)];
				if (request.Kind == RequestKind::Gift && !m_State.Board.GetRequest(board, i).Done)
				{
					who = request.Villager;
					item = request.Item;
					return true;
				}
			}
		}
		return false;
	}

	ItemId AutoplayFarmer::KeptForGift() const
	{
		VillagerId who = VillagerId::Count;
		ItemId item = ItemId::Count;
		return FindGiftRequest(who, item) ? item : ItemId::Count;
	}

}
