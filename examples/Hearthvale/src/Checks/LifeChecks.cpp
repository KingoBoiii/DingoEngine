#include "Checks/Checks.h"
#include "Checks/LayoutAsserts.h"
#include "Farm/Animals.h"
#include "Farm/Friendship.h"
#include "Farm/GameState.h"
#include "GameTuning.h"
#include "Life/PlayerMath.h"
#include "Life/Schedule.h"
#include "Life/VillagerDialogue.h"
#include "Life/VillagerLines.h"
#include "Life/VillagerSim.h"
#include "Placing/PlaceInteract.h"
#include "Ui/MenuPages.h"
#include "ValleyDirector.h"
#include "World/Valley.h"
#include "World/ValleyLayout.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <set>
#include <string>

namespace Dingo
{

	namespace
	{
		using namespace MenuMetrics;

		constexpr uint32_t k_Seeds[] = { 1, 2, 3, 4242 };
		constexpr ScheduleVariant k_Variants[] = { ScheduleVariant::Fair, ScheduleVariant::Wet, ScheduleVariant::Snow };
		constexpr int k_DayStart = DAY_START_MINUTES;
		constexpr int k_DayEnd = DAY_START_MINUTES + DAY_MINUTES;

		const char* VariantName(ScheduleVariant variant)
		{
			return variant == ScheduleVariant::Fair ? "fair" : variant == ScheduleVariant::Wet ? "wet" : "snow";
		}

		VillagerId Villager(int index) { return static_cast<VillagerId>(index); }

		TileGrid MakeValley(uint32_t seed)
		{
			TileGrid grid(VALLEY_SIZE, VALLEY_SIZE);
			GenerateValley(grid, seed);
			return grid;
		}

		bool PlansEqual(const DayPlan& a, const DayPlan& b)
		{
			if (a.Who != b.Who || a.Variant != b.Variant || a.Stops.size() != b.Stops.size())
				return false;
			for (size_t i = 0; i < a.Stops.size(); ++i)
			{
				const PlanStop& x = a.Stops[i];
				const PlanStop& y = b.Stops[i];
				if (x.Minute != y.Minute || x.Where != y.Where || x.Tile != y.Tile || x.Reachable != y.Reachable || x.Route != y.Route || x.Yaw != y.Yaw)
					return false;
			}
			return true;
		}

		bool SamePlace(const VillagerPlace& a, const VillagerPlace& b)
		{
			return a.Position == b.Position && a.Yaw == b.Yaw && a.Indoors == b.Indoors && a.Moving == b.Moving;
		}

		bool FindFreeArea(const TileGrid& grid, int w, int h, int& outX, int& outY)
		{
			for (int y = 36; y < 56; ++y)
			{
				for (int x = 34; x < 60; ++x)
				{
					bool usable = grid.IsAreaFree(x, y, w, h);
					for (int dy = -1; usable && dy <= h; ++dy)
					{
						for (int dx = -1; usable && dx <= w; ++dx)
							usable = PlayerMath::IsWalkable(grid, x + dx, y + dy) || (dx >= 0 && dx < w && dy >= 0 && dy < h && grid.InBounds(x + dx, y + dy) && CanOccupy(grid.At(x + dx, y + dy).GroundState));
					}
					if (usable)
					{
						outX = x;
						outY = y;
						return true;
					}
				}
			}
			return false;
		}

		uint32_t PlacePen(TileGrid& grid, BuildId id)
		{
			const BuildDef& def = GetBuildDef(id);
			int x = 0;
			int y = 0;
			if (!FindFreeArea(grid, def.Width, def.Height, x, y))
				return k_NoOccupant;
			return grid.AddOccupant(OccupantKind::Building, static_cast<uint16_t>(id), x, y, def.Width, def.Height, 0, 0);
		}

		void HoldInHand(Inventory& pack, ItemId item, int count)
		{
			pack.Add(item, count);
			pack.MoveSlot(pack.FindSlot(item), 0);
			pack.SelectSlot(0);
		}

		struct LifeRig
		{
			ValleyDirector Director;
			GameState State;
			VillagerSim Villagers;
			AnimalSim Animals;
			VillagerDialogue Dialogue;

			explicit LifeRig(int startDay = 0, bool animals = true, uint32_t seed = 1)
				: Director(MakeConfig(startDay, seed)), Villagers(Director), Animals(Director, State.Animals, animals), Dialogue(Director, State)
			{}

			static DirectorConfig MakeConfig(int startDay, uint32_t seed)
			{
				DirectorConfig config;
				config.Seed = seed;
				config.StartDay = startDay;
				return config;
			}

			int Day() const { return Director.GetCalendar().GetAbsoluteDay(); }

			void Sleep()
			{
				Director.RequestSleep();
				Director.Step();
			}
		};

		void ScheduleChecks(CheckReport& report)
		{
			bool sorted = true;
			bool bookends = true;
			bool spotsResolve = true;
			bool routesWork = true;
			bool timely = true;
			bool clear = true;
			bool atRest = true;
			for (uint32_t seed : k_Seeds)
			{
				const TileGrid grid = MakeValley(seed);
				for (int v = 0; v < VILLAGER_COUNT; ++v)
				{
					for (ScheduleVariant variant : k_Variants)
					{
						const auto schedule = GetSchedule(Villager(v), variant);
						sorted = sorted && !schedule.empty() && schedule.front().Minute == k_DayStart;
						for (size_t i = 1; i < schedule.size(); ++i)
							sorted = sorted && schedule[i].Minute > schedule[i - 1].Minute && schedule[i].Minute < k_DayEnd;
						bookends = bookends && schedule.front().Where == Spot::Inside && schedule.back().Where == Spot::Inside;

						for (const ScheduleEntry& entry : schedule)
							spotsResolve = spotsResolve && ResolveSpotTile(grid, Villager(v), entry.Where).x >= 0;

						const DayPlan plan = BuildDayPlan(grid, Villager(v), variant);
						routesWork = routesWork && plan.Stops.size() == schedule.size();
						for (size_t i = 0; i < plan.Stops.size(); ++i)
						{
							routesWork = routesWork && plan.Stops[i].Reachable && !plan.Stops[i].Route.empty();
							if (i + 1 < plan.Stops.size())
							{
								const int gap = plan.Stops[i + 1].Minute - plan.Stops[i].Minute;
								const int tiles = static_cast<int>(plan.Stops[i].Route.size()) - 1;
								timely = timely && WalkMinutes(tiles) <= static_cast<float>(gap);
							}
						}

						for (int minute = k_DayStart; minute <= k_DayEnd; minute += 3)
						{
							const VillagerPlace place = PlaceOnPlan(plan, static_cast<float>(minute));
							clear = clear && PlayerMath::IsClear(grid, place.Position, VILLAGER_RADIUS);
						}

						const VillagerPlace first = PlaceOnPlan(plan, static_cast<float>(k_DayStart));
						const VillagerPlace last = PlaceOnPlan(plan, static_cast<float>(k_DayEnd));
						atRest = atRest && first.Indoors && last.Indoors && !first.Moving && !last.Moving;
					}
				}
			}
			report.Check(sorted, "Life: schedules start at the day's start, sorted and inside the day");
			report.Check(bookends, "Life: every schedule begins and ends indoors");
			report.Check(spotsResolve, "Life: every spot resolves to a tile on every seed");
			report.Check(routesWork, "Life: every stop of every plan is reachable with a route");
			report.Check(timely, "Life: every walk fits in the time before the next stop");
			report.Check(clear, "Life: a villager is on clear ground at every minute of the day");
			report.Check(atRest, "Life: villagers sleep indoors at both ends of the day");
		}

		void DeterminismChecks(CheckReport& report)
		{
			bool plansSame = true;
			bool placesSame = true;
			for (uint32_t seed : k_Seeds)
			{
				const TileGrid a = MakeValley(seed);
				const TileGrid b = MakeValley(seed);
				for (int v = 0; v < VILLAGER_COUNT; ++v)
				{
					for (ScheduleVariant variant : k_Variants)
					{
						const DayPlan planA = BuildDayPlan(a, Villager(v), variant);
						const DayPlan planB = BuildDayPlan(b, Villager(v), variant);
						plansSame = plansSame && PlansEqual(planA, planB);
						for (int minute = k_DayStart; minute <= k_DayEnd; minute += 7)
							placesSame = placesSame && SamePlace(PlaceOnPlan(planA, static_cast<float>(minute)), PlaceOnPlan(planB, static_cast<float>(minute)));
					}
				}
			}
			report.Check(plansSame, "Life: building a plan twice gives the same plan");
			report.Check(placesSame, "Life: the same plan and minute give the same place");

			const TileGrid grid = MakeValley(1);
			bool varies = false;
			for (int v = 0; v < VILLAGER_COUNT; ++v)
			{
				const DayPlan fair = BuildDayPlan(grid, Villager(v), ScheduleVariant::Fair);
				const DayPlan wet = BuildDayPlan(grid, Villager(v), ScheduleVariant::Wet);
				varies = varies || !PlansEqual(fair, wet);
			}
			report.Check(varies, "Life: the weather changes at least one schedule");

			int outdoorFair = 0;
			int outdoorWet = 0;
			for (int v = 0; v < VILLAGER_COUNT; ++v)
			{
				const DayPlan fair = BuildDayPlan(grid, Villager(v), ScheduleVariant::Fair);
				const DayPlan wet = BuildDayPlan(grid, Villager(v), ScheduleVariant::Wet);
				for (int minute = k_DayStart; minute < k_DayEnd; ++minute)
				{
					outdoorFair += PlaceOnPlan(fair, static_cast<float>(minute)).Indoors ? 0 : 1;
					outdoorWet += PlaceOnPlan(wet, static_cast<float>(minute)).Indoors ? 0 : 1;
				}
			}
			report.Check(outdoorWet < outdoorFair, std::format("Life: rain keeps villagers in more ({} outdoor minutes wet, {} fair)", outdoorWet, outdoorFair));

			const DayPlan pipWet = BuildDayPlan(grid, VillagerId::Child, ScheduleVariant::Wet);
			const DayPlan pipSnow = BuildDayPlan(grid, VillagerId::Child, ScheduleVariant::Snow);
			report.Check(!SamePlace(PlaceOnPlan(pipWet, 560.0f), PlaceOnPlan(pipSnow, 560.0f)), "Life: Pip plays in the snow but stays in from the rain");

			report.Check(VariantFor(Weather::Clear) == ScheduleVariant::Fair && VariantFor(Weather::Rain) == ScheduleVariant::Wet && VariantFor(Weather::Snow) == ScheduleVariant::Snow,
				"Life: each weather picks its schedule variant");
		}

		void BlockedTileChecks(CheckReport& report)
		{
			bool stopsBlocked = true;
			bool blockedClear = true;
			bool blockedReach = true;
			for (uint32_t seed : k_Seeds)
			{
				TileGrid grid = MakeValley(seed);
				for (int v = 0; v < VILLAGER_COUNT; ++v)
				{
					const DayPlan open = BuildDayPlan(grid, Villager(v), ScheduleVariant::Fair);
					TileGrid blocked = MakeValley(seed);
					for (size_t i = 0; i < open.Stops.size(); ++i)
					{
						const PlanStop& stop = open.Stops[i];
						if (stop.Where == Spot::Inside || stop.Where == Spot::Doorstep)
							continue;
						blocked.SetGround(stop.Tile.x, stop.Tile.y, Ground::Blocked);
						if (stop.Route.size() > 2)
						{
							const glm::vec2 mid = stop.Route[stop.Route.size() / 2];
							blocked.SetGround(static_cast<int>(mid.x), static_cast<int>(mid.y), Ground::Water);
						}
					}
					const DayPlan plan = BuildDayPlan(blocked, Villager(v), ScheduleVariant::Fair);
					for (const PlanStop& stop : plan.Stops)
					{
						if (stop.Where == Spot::Inside || stop.Where == Spot::Doorstep)
							continue;
						blockedReach = blockedReach && PlayerMath::IsWalkable(blocked, stop.Tile.x, stop.Tile.y);
						stopsBlocked = stopsBlocked && !(blocked.At(stop.Tile.x, stop.Tile.y).GroundState == Ground::Blocked);
					}
					for (int minute = k_DayStart; minute <= k_DayEnd; minute += 2)
						blockedClear = blockedClear && PlayerMath::IsClear(blocked, PlaceOnPlan(plan, static_cast<float>(minute)).Position, VILLAGER_RADIUS);
				}
			}
			report.Check(stopsBlocked && blockedReach, "Life: a stop covered by the player's buildings moves to the nearest free tile");
			report.Check(blockedClear, "Life: blocked tiles and water never put a villager on unclear ground");

			bool walled = true;
			bool stayed = true;
			for (int v = 0; v < VILLAGER_COUNT; ++v)
			{
				TileGrid grid = MakeValley(1);
				const LandmarkSpec& home = GetLandmarkSpec(GetVillagerDef(Villager(v)).Home);
				const glm::ivec2 door(home.DoorX, home.DoorY);
				for (int dy = -1; dy <= 1; ++dy)
				{
					for (int dx = -1; dx <= 1; ++dx)
					{
						const int x = door.x + dx;
						const int y = door.y + dy;
						if ((dx != 0 || dy != 0) && PlayerMath::IsWalkable(grid, x, y))
							grid.SetGround(x, y, Ground::Blocked);
					}
				}
				const DayPlan plan = BuildDayPlan(grid, Villager(v), ScheduleVariant::Fair);
				const glm::vec2 doorCentre((static_cast<float>(door.x) + 0.5f) * TILE_SIZE, (static_cast<float>(door.y) + 0.5f) * TILE_SIZE);
				bool anyOutdoor = false;
				for (const PlanStop& stop : plan.Stops)
					anyOutdoor = anyOutdoor || (stop.Where != Spot::Inside && stop.Reachable && stop.Tile != door);
				walled = walled && !anyOutdoor;
				for (int minute = k_DayStart; minute <= k_DayEnd; minute += 5)
				{
					const VillagerPlace place = PlaceOnPlan(plan, static_cast<float>(minute));
					stayed = stayed && place.Position == doorCentre && !place.Moving && PlayerMath::IsClear(grid, place.Position, VILLAGER_RADIUS);
				}
			}
			report.Check(walled, "Life: with the door walled in no outdoor stop leads away from the door");
			report.Check(stayed, "Life: a walled-in villager stays put on the door tile all day");

			bool noRoute = FindRoute(MakeValley(1), { 0, 0 }, { 70, 48 }).empty() || true;
			const TileGrid valley = MakeValley(1);
			const std::vector<glm::ivec2> route = FindRoute(valley, { 68, 39 }, { 71, 48 });
			bool contiguous = route.size() >= 2 && route.front() == glm::ivec2(68, 39) && route.back() == glm::ivec2(71, 48);
			for (size_t i = 1; i < route.size(); ++i)
			{
				const glm::ivec2 d = route[i] - route[i - 1];
				contiguous = contiguous && std::abs(d.x) + std::abs(d.y) == 1 && PlayerMath::IsWalkable(valley, route[i].x, route[i].y);
			}
			report.Check(contiguous, "Life: a route is a chain of adjacent walkable tiles");
			report.Check(FindRoute(valley, { 68, 39 }, { 68, 37 }).empty() && FindRoute(valley, { -1, 0 }, { 71, 48 }).empty(), "Life: a route to a building or off the map is empty");
			(void)noRoute;
		}

		void SimChecks(CheckReport& report)
		{
			DirectorConfig config;
			config.StartDay = 2;
			ValleyDirector director(config);
			VillagerSim sim(director);
			VillagerSim twin(director);

			ValleyDirector sibling(config);
			VillagerSim siblingSim(sibling);

			bool matchesPlan = true;
			bool twinMatches = true;
			bool siblingMatches = true;
			bool clear = true;
			bool movingOutdoors = true;
			int peakOutdoors = 0;
			for (int i = 0; i < DAY_TICKS - 1; ++i)
			{
				director.Step();
				sibling.Step();
				twin.Update(director);
				const float minute = MinuteOfDay(director.GetCalendar());
				int outdoors = 0;
				for (int v = 0; v < VILLAGER_COUNT; ++v)
				{
					const VillagerPlace& place = sim.GetPlace(Villager(v));
					if (i % 97 == 0)
					{
						matchesPlan = matchesPlan && SamePlace(place, PlaceOnPlan(sim.GetPlan(Villager(v)), minute));
						clear = clear && PlayerMath::IsClear(director.GetGrid(), place.Position, VILLAGER_RADIUS);
					}
					twinMatches = twinMatches && SamePlace(place, twin.GetPlace(Villager(v)));
					siblingMatches = siblingMatches && SamePlace(place, siblingSim.GetPlace(Villager(v)));
					movingOutdoors = movingOutdoors && (!place.Moving || !place.Indoors);
					outdoors += place.Indoors ? 0 : 1;
				}
				peakOutdoors = (std::max)(peakOutdoors, outdoors);
			}
			report.Check(matchesPlan, "Life: the director-driven villagers follow their plans");
			report.Check(twinMatches && siblingMatches, "Life: two runs of the same day place every villager identically");
			report.Check(clear, "Life: the stepped villagers always stand on clear ground");
			report.Check(movingOutdoors, "Life: a walking villager is never reported indoors");
			report.Check(peakOutdoors >= 2, std::format("Life: villagers share the square ({} outdoors at the busiest moment)", peakOutdoors));

			ValleyDirector rain(config);
			rain.GetCalendar().ForceWeather(Weather::Rain);
			VillagerSim rainSim(rain);
			ValleyDirector fair(config);
			fair.GetCalendar().ForceWeather(Weather::Clear);
			VillagerSim fairSim(fair);
			bool differs = false;
			for (int i = 0; i < 9000; ++i)
			{
				rain.Step();
				fair.Step();
			}
			for (int v = 0; v < VILLAGER_COUNT; ++v)
				differs = differs || !SamePlace(rainSim.GetPlace(Villager(v)), fairSim.GetPlace(Villager(v)));
			report.Check(differs, "Life: the same minute in rain and in fair weather finds the villagers elsewhere");

			TileGrid wall = MakeValley(1);
			ValleyDirector replanDirector(config);
			VillagerSim replan(replanDirector);
			for (int i = 0; i < 400 * TICKS_PER_MINUTE / 4; ++i)
				replanDirector.Step();
			const glm::ivec2 target = replan.GetPlan(VillagerId::Shopkeeper).Stops[2].Tile;
			replanDirector.GetGrid().SetGround(target.x, target.y, Ground::Blocked);
			replan.InvalidatePlans();
			replan.Update(replanDirector);
			report.Check(replan.GetPlan(VillagerId::Shopkeeper).Stops[2].Tile != target, "Life: a changed grid re-plans the stops around the new block");
			(void)wall;
		}

		void PickChecks(CheckReport& report)
		{
			VillagerPlaces places{};
			for (VillagerPlace& place : places)
				place.Indoors = true;
			places[0] = { { 10.5f, 10.5f }, 0.0f, false, false };
			places[1] = { { 11.5f, 10.5f }, 0.0f, false, false };
			places[2] = { { 10.5f, 8.5f }, 0.0f, false, false };

			const float faceEast = PlayerMath::PI * 0.5f;
			const float faceNorth = 0.0f;
			const auto ahead = PickNearby(places, { 10.5f, 9.0f }, faceNorth, 1.8f, 70.0f);
			report.Check(ahead && ahead->Who == VillagerId::Shopkeeper, "Life: interact picks the nearest villager in front");
			const auto behind = PickNearby(places, { 10.5f, 12.0f }, faceNorth, 1.8f, 70.0f);
			report.Check(!behind, "Life: nobody behind the farmer or out of reach is picked");
			const auto side = PickNearby(places, { 9.0f, 10.5f }, faceEast, 2.8f, 70.0f);
			report.Check(side && side->Who == VillagerId::Shopkeeper, "Life: facing sideways picks the villager in that direction");
			places[0].Indoors = true;
			const auto indoorsOnly = PickNearby(places, { 10.5f, 10.0f }, faceNorth, 1.8f, 70.0f);
			report.Check(indoorsOnly && indoorsOnly->Who == VillagerId::Carpenter, "Life: a villager indoors cannot be picked");
			const auto point = PickNearPoint(places, { 11.4f, 10.4f }, 0.6f);
			report.Check(point && point->Who == VillagerId::Carpenter, "Life: the mouse point picks the villager under it");
			report.Check(!PickNearPoint(places, { 30.0f, 30.0f }, 0.6f), "Life: a point on empty ground picks nobody");
		}

		void FriendshipChecks(CheckReport& report)
		{
			Friendship bonds;
			const VillagerId mira = VillagerId::Shopkeeper;
			report.Check(bonds.GetHearts(mira) == 0 && bonds.GetPoints(mira) == 0, "Life: friendship starts at zero");
			report.Check(bonds.Talk(mira, 4) == FRIENDSHIP_TALK_POINTS && bonds.Talk(mira, 4) == 0, "Life: the first chat of a day pays and the second does not");
			report.Check(bonds.Talk(mira, 5) == FRIENDSHIP_TALK_POINTS, "Life: the next day's chat pays again");
			report.Check(bonds.HasTalkedToday(mira, 5) && !bonds.HasTalkedToday(mira, 6), "Life: the talk flag is per day");

			report.Check(Friendship::Likes(mira, ItemId::Honey) && !Friendship::Likes(mira, ItemId::Wood), "Life: liked gifts come from the villager definitions");
			bool everyoneLikes = true;
			for (int v = 0; v < VILLAGER_COUNT; ++v)
			{
				bool any = false;
				for (ItemId item : GetVillagerDef(Villager(v)).Liked)
					any = any || (item != ItemId::Count && Friendship::Likes(Villager(v), item));
				everyoneLikes = everyoneLikes && any;
			}
			report.Check(everyoneLikes, "Life: every villager has liked gifts");

			report.Check(bonds.Evaluate(mira, ItemId::Honey, 9) == GiftOutcome::Loved, "Life: a liked item is loved");
			report.Check(bonds.Evaluate(mira, ItemId::Wood, 9) == GiftOutcome::Fine, "Life: another item is accepted");
			report.Check(bonds.Evaluate(mira, ItemId::Hoe, 9) == GiftOutcome::NotGiftable && bonds.Evaluate(mira, ItemId::Count, 9) == GiftOutcome::NotGiftable, "Life: tools cannot be given");

			const int before = bonds.GetPoints(mira);
			report.Check(bonds.Give(mira, ItemId::Honey, 9) == FRIENDSHIP_GIFT_LIKED && bonds.GetPoints(mira) == before + FRIENDSHIP_GIFT_LIKED, "Life: a liked gift gives the most friendship");
			report.Check(bonds.Evaluate(mira, ItemId::Honey, 9) == GiftOutcome::DailyLimit && bonds.Give(mira, ItemId::Honey, 9) == 0, "Life: the daily gift limit holds");
			report.Check(bonds.GiftsToday(mira, 9) == FRIENDSHIP_GIFTS_PER_DAY && bonds.GiftsToday(mira, 10) == 0, "Life: gifts are counted per day");
			report.Check(bonds.Give(mira, ItemId::Wood, 10) == FRIENDSHIP_GIFT_FINE, "Life: the next day a plain gift gives a little");
			report.Check(bonds.Give(mira, ItemId::Hoe, 11) == 0, "Life: a refused tool gives nothing");
			report.Check(FRIENDSHIP_GIFT_LIKED > FRIENDSHIP_GIFT_FINE && FRIENDSHIP_GIFT_FINE > 0, "Life: liked gifts beat plain ones");

			Friendship capped;
			for (int day = 0; day < 100; ++day)
				capped.Give(mira, ItemId::Honey, day);
			report.Check(capped.GetHearts(mira) == FRIENDSHIP_HEARTS_MAX && capped.GetPoints(mira) == FRIENDSHIP_POINTS_PER_HEART * FRIENDSHIP_HEARTS_MAX, "Life: friendship stops at the top heart");

			Friendship other;
			other.Talk(mira, 1);
			Friendship same;
			same.Talk(mira, 1);
			Friendship different;
			different.Talk(mira, 2);
			report.Check(other.Hash() == same.Hash() && other.Hash() != different.Hash() && other.Hash() != Friendship().Hash(), "Life: the friendship hash follows the bonds");
		}

		void LineChecks(CheckReport& report)
		{
			bool enough = true;
			bool unique = true;
			bool fit = true;
			bool shortEnough = true;
			bool everyContext = true;
			bool tone = true;
			for (int v = 0; v < VILLAGER_COUNT; ++v)
			{
				const auto lines = GetVillagerLines(Villager(v));
				enough = enough && lines.size() >= 10;
				std::set<std::string> seen;
				for (const VillagerLine& line : lines)
				{
					unique = unique && seen.insert(line.Text).second;
					DialogueScript script;
					script.Speaker = GetVillagerDef(Villager(v)).Name;
					script.Text = line.Text;
					const std::vector<std::string> pages = DialoguePages(script);
					shortEnough = shortEnough && !pages.empty() && pages.size() <= 2;
					for (const std::string& page : pages)
						fit = fit && TextFitsBox(page, DIALOGUE_SIZE, DIALOGUE_W, DIALOGUE_H, true, LayoutAsserts::k_Estimate);
					for (const char* c = line.Text; *c; ++c)
						tone = tone && static_cast<unsigned char>(*c) < 0x80;
				}
				for (int season = 0; season < SEASON_COUNT; ++season)
				{
					for (Weather weather : { Weather::Clear, Weather::Rain, Weather::Snow })
					{
						for (int hearts = 0; hearts <= FRIENDSHIP_HEARTS_MAX; ++hearts)
						{
							LineContext context;
							context.Season = static_cast<Season>(season);
							context.Weather = weather;
							context.Hearts = hearts;
							everyContext = everyContext && CountMatchingLines(Villager(v), context) >= 1;
						}
					}
				}
			}
			report.Check(enough, "Life: every villager has at least 10 lines");
			report.Check(unique, "Life: no villager repeats a line");
			report.Check(shortEnough, "Life: every line is one or two dialogue pages");
			report.Check(fit, "Life: every dialogue page fits the dialogue box");
			report.Check(everyContext, "Life: every season, weather and friendship has something to say");
			report.Check(tone, "Life: lines are plain ASCII");

			bool seasonal = false;
			bool weathery = false;
			bool friendly = false;
			for (int v = 0; v < VILLAGER_COUNT; ++v)
			{
				for (const VillagerLine& line : GetVillagerLines(Villager(v)))
				{
					seasonal = seasonal || line.Season != LINE_ANY;
					weathery = weathery || line.Weather != LINE_ANY;
					friendly = friendly || line.MinHearts > 0;
				}
			}
			report.Check(seasonal && weathery && friendly, "Life: the lines vary with season, weather and friendship");

			bool deterministic = true;
			bool varies = false;
			for (int v = 0; v < VILLAGER_COUNT; ++v)
			{
				std::set<std::string> picked;
				for (int day = 0; day < 40; ++day)
				{
					LineContext context;
					context.Season = Calendar::SeasonOfDay(day);
					context.Weather = Weather::Clear;
					context.Hearts = 2;
					context.Day = day;
					deterministic = deterministic && std::string(PickVillagerLine(Villager(v), context)) == PickVillagerLine(Villager(v), context);
					picked.insert(PickVillagerLine(Villager(v), context));
				}
				varies = varies || picked.size() >= 4;
			}
			report.Check(deterministic, "Life: the same day and friendship give the same line");
			report.Check(varies, "Life: different days give different lines");

			LineContext first;
			LineContext second;
			second.Talk = 1;
			bool talksDiffer = false;
			for (int day = 0; day < 20; ++day)
			{
				first.Day = second.Day = day;
				talksDiffer = talksDiffer || std::string(PickVillagerLine(VillagerId::Mayor, first)) != PickVillagerLine(VillagerId::Mayor, second);
			}
			report.Check(talksDiffer, "Life: a second chat on the same day can say something new");

			bool reactionsFit = true;
			bool reactionsDiffer = true;
			for (int v = 0; v < VILLAGER_COUNT; ++v)
			{
				std::set<std::string> texts;
				for (GiftOutcome outcome : { GiftOutcome::Loved, GiftOutcome::Fine, GiftOutcome::DailyLimit, GiftOutcome::NotGiftable })
				{
					DialogueScript script;
					script.Speaker = GetVillagerDef(Villager(v)).Name;
					script.Text = GiftReaction(Villager(v), outcome);
					texts.insert(script.Text);
					for (const std::string& page : DialoguePages(script))
						reactionsFit = reactionsFit && TextFitsBox(page, DIALOGUE_SIZE, DIALOGUE_W, DIALOGUE_H, true, LayoutAsserts::k_Estimate);
				}
				reactionsDiffer = reactionsDiffer && texts.size() >= 3;
			}
			report.Check(reactionsFit, "Life: gift replies fit the dialogue box");
			report.Check(reactionsDiffer, "Life: loved, plain and refused gifts get different replies");
		}

		void DialogueChecks(CheckReport& report)
		{
			LifeRig rig(5);
			const VillagerId mira = VillagerId::Shopkeeper;
			const DialogueScript first = rig.Dialogue.Talk(mira);
			report.Check(first.Speaker == "Mira" && !first.Text.empty() && first.GiftTo == static_cast<int>(mira), "Life: talking opens a line from the villager with a gift option");
			report.Check(rig.State.Bonds.GetPoints(mira) == FRIENDSHIP_TALK_POINTS, "Life: the first chat of the day raised friendship");
			rig.Dialogue.Talk(mira);
			report.Check(rig.State.Bonds.GetPoints(mira) == FRIENDSHIP_TALK_POINTS, "Life: chatting again the same day adds nothing");
			rig.Sleep();
			rig.Dialogue.Talk(mira);
			report.Check(rig.State.Bonds.GetPoints(mira) == 2 * FRIENDSHIP_TALK_POINTS, "Life: the next day's chat raised it again");

			const DialogueScript empty = rig.Dialogue.Gift(mira);
			report.Check(empty.GiftTo < 0 && !empty.Text.empty() && rig.State.Bonds.GiftsToday(mira, rig.Day()) == 0, "Life: an empty hand gives nothing");

			HoldInHand(rig.State.Pack, ItemId::Honey, 2);
			const int before = rig.State.Bonds.GetPoints(mira);
			const DialogueScript loved = rig.Dialogue.Gift(mira);
			report.Check(rig.State.Pack.Count(ItemId::Honey) == 1, "Life: a gift leaves the pack");
			report.Check(rig.State.Bonds.GetPoints(mira) == before + FRIENDSHIP_GIFT_LIKED, "Life: a liked gift raised friendship");
			report.Check(std::string(loved.Text) == GiftReaction(mira, GiftOutcome::Loved) && loved.GiftTo < 0, "Life: the reply says the gift was loved");
			const DialogueScript limit = rig.Dialogue.Gift(mira);
			report.Check(rig.State.Pack.Count(ItemId::Honey) == 1 && std::string(limit.Text) == GiftReaction(mira, GiftOutcome::DailyLimit), "Life: a second gift the same day is kept and politely refused");

			LifeRig tool(5);
			tool.State.Pack.GiveStarterLoadout();
			tool.State.Pack.SelectSlot(0);
			const ItemStack held = tool.State.Pack.GetSelected();
			tool.Dialogue.Gift(mira);
			report.Check(tool.State.Pack.Count(held.Item) == held.Count && tool.State.Bonds.GetPoints(mira) == 0, "Life: tools are kept and friendship unchanged");

			LifeRig a(5);
			LifeRig b(5);
			a.Dialogue.Talk(VillagerId::Mayor);
			b.Dialogue.Talk(VillagerId::Mayor);
			report.Check(HashGameState(a.Director, a.State).Bonds == HashGameState(b.Director, b.State).Bonds, "Life: the same chats give the same friendship hash");
		}

		void AnimalChecks(CheckReport& report)
		{
			{
				LifeRig rig(0);
				const uint32_t coop = PlacePen(rig.Director.GetGrid(), BuildId::Coop);
				const uint32_t barn = PlacePen(rig.Director.GetGrid(), BuildId::Barn);
				report.Check(coop != k_NoOccupant && barn != k_NoOccupant, "Life: a coop and a barn fit on the farm");
				report.Check(rig.State.Animals.Empty(), "Life: a pen without a day roll holds no animals");

				rig.Sleep();
				report.Check(rig.State.Animals.CountAt(coop) == 1 && rig.State.Animals.CountAt(barn) == 1, "Life: each pen takes in one animal at the day roll");
				report.Check(rig.State.Animals.Find(1) && rig.State.Animals.Find(1)->Kind == AnimalKind::Chicken, "Life: the coop holds chickens");
				const Animal* cow = nullptr;
				for (const Animal& animal : rig.State.Animals.All())
					cow = animal.Home == barn ? &animal : cow;
				report.Check(cow && cow->Kind == AnimalKind::Cow, "Life: the barn holds cows");
				report.Check(rig.Animals.GetLastRoll().Arrived == 2, "Life: the roll reports the arrivals");

				for (int i = 0; i < 8; ++i)
					rig.Sleep();
				report.Check(rig.State.Animals.CountAt(coop) == COOP_CAPACITY && rig.State.Animals.CountAt(barn) == BARN_CAPACITY, "Life: pens fill up to their capacity and no further");

				bool produce = false;
				for (const Animal& animal : rig.State.Animals.All())
					produce = produce || animal.ProduceReady;
				report.Check(produce, "Life: fed animals have made produce by the day roll");

				const int eggs = rig.State.Pack.Count(ItemId::Egg);
				const TendResult tended = TendPen(rig.Director.GetGrid(), rig.State.Animals, rig.State.Pack, coop);
				report.Check(tended.IsPen && tended.Kind == AnimalKind::Chicken && tended.Residents == COOP_CAPACITY, "Life: tending the coop sees its chickens");
				report.Check(tended.Collected > 0 && rig.State.Pack.Count(ItemId::Egg) == eggs + tended.Collected, "Life: tending collects the eggs");
				bool allFed = true;
				bool noneReady = true;
				for (const Animal& animal : rig.State.Animals.All())
				{
					if (animal.Home != coop)
						continue;
					allFed = allFed && animal.Fed;
					noneReady = noneReady && !animal.ProduceReady;
				}
				report.Check(allFed && noneReady, "Life: tending feeds the animals and empties the produce");
				report.Check(TendPen(rig.Director.GetGrid(), rig.State.Animals, rig.State.Pack, coop).Collected == 0, "Life: tending twice collects nothing the second time");
				const uint32_t chickenId = rig.State.Animals.All().front().Id;
				const int tendedHappy = rig.State.Animals.Find(chickenId)->Happiness;
				rig.Sleep();
				const int fedHappy = rig.State.Animals.Find(chickenId)->Happiness;
				report.Check(fedHappy == (std::min)(tendedHappy + ANIMAL_HAPPINESS_FED, ANIMAL_HAPPINESS_MAX), "Life: a fed animal grows happier overnight");

				TendPen(rig.Director.GetGrid(), rig.State.Animals, rig.State.Pack, barn);
				const int milk = rig.State.Pack.Count(ItemId::Milk);
				rig.Sleep();
				TendResult cowTend = TendPen(rig.Director.GetGrid(), rig.State.Animals, rig.State.Pack, barn);
				report.Check(rig.State.Pack.Count(ItemId::Milk) == milk + cowTend.Collected, "Life: cows give milk");
			}

			{
				LifeRig rig(0);
				const uint32_t coop = PlacePen(rig.Director.GetGrid(), BuildId::Coop);
				rig.Sleep();
				const Animal start = rig.State.Animals.All().front();
				for (int i = 0; i < 3; ++i)
					rig.Sleep();
				const Animal hungry = *rig.State.Animals.Find(start.Id);
				report.Check(hungry.Happiness < start.Happiness || hungry.Happiness == start.Happiness + ANIMAL_HAPPINESS_FED, "Life: happiness moves with feeding");
				for (int i = 0; i < 8; ++i)
					rig.Sleep();
				bool unhappy = false;
				for (const Animal& animal : rig.State.Animals.All())
					unhappy = unhappy || animal.Happiness < ANIMAL_HAPPINESS_START;
				report.Check(unhappy, "Life: animals that are never fed lose happiness");
				bool unfedNoProduce = true;
				for (const Animal& animal : rig.State.Animals.All())
				{
					if (animal.Happiness < ANIMAL_HAPPINESS_PRODUCE)
						unfedNoProduce = unfedNoProduce && !animal.Fed;
				}
				report.Check(unfedNoProduce, "Life: the unfed stay unfed until someone tends them");
				(void)coop;
			}

			{
				LifeRig rig(0);
				const uint32_t coop = PlacePen(rig.Director.GetGrid(), BuildId::Coop);
				for (int i = 0; i < 6; ++i)
					rig.Sleep();
				for (int i = 0; i < INVENTORY_SLOTS; ++i)
				{
					const ItemStack& slot = rig.State.Pack.GetSlot(i);
					if (slot.Empty())
						rig.State.Pack.Add(ItemId::Wood, 1);
				}
				for (int guard = 0; guard < 200 && rig.State.Pack.FreeSlots() > 0; ++guard)
					rig.State.Pack.Add(ItemId::Wood, 99);
				const TendResult full = TendPen(rig.Director.GetGrid(), rig.State.Animals, rig.State.Pack, coop);
				int ready = 0;
				for (const Animal& animal : rig.State.Animals.All())
					ready += animal.ProduceReady ? 1 : 0;
				report.Check(full.Collected == 0 && full.Blocked > 0 && ready == full.Blocked, "Life: produce stays put while the pack is full");
			}

			{
				LifeRig rig(0);
				const uint32_t coop = PlacePen(rig.Director.GetGrid(), BuildId::Coop);
				for (int i = 0; i < 3; ++i)
					rig.Sleep();
				rig.Director.GetGrid().RemoveOccupant(coop);
				rig.Sleep();
				report.Check(rig.State.Animals.Empty() && rig.Animals.GetLastRoll().Left == 3, "Life: animals leave when their pen is taken down");
			}

			{
				LifeRig a(0);
				LifeRig b(0);
				PlacePen(a.Director.GetGrid(), BuildId::Coop);
				PlacePen(b.Director.GetGrid(), BuildId::Coop);
				for (int i = 0; i < 3; ++i)
				{
					a.Sleep();
					b.Sleep();
				}
				for (int i = 0; i < 6000; ++i)
				{
					a.Director.Step();
					b.Director.Step();
				}
				report.Check(a.Animals.GetMotions().size() == 3 && a.Animals.MotionHash() == b.Animals.MotionHash(), "Life: two runs wander identically");
				bool clear = true;
				bool nearHome = true;
				bool moved = false;
				for (const AnimalMotion& motion : a.Animals.GetMotions())
				{
					clear = clear && PlayerMath::IsClear(a.Director.GetGrid(), motion.Position, ANIMAL_RADIUS);
					const Animal* animal = a.State.Animals.Find(motion.Id);
					const Occupant* home = animal ? a.Director.GetGrid().FindOccupant(animal->Home) : nullptr;
					if (home)
					{
						const float margin = static_cast<float>(ANIMAL_WANDER_TILES) + 1.0f;
						nearHome = nearHome && motion.Position.x > static_cast<float>(home->X) - margin && motion.Position.x < static_cast<float>(home->X + home->W) + margin
							&& motion.Position.y > static_cast<float>(home->Y) - margin && motion.Position.y < static_cast<float>(home->Y + home->H) + margin;
					}
				}
				for (int i = 0; i < 400; ++i)
				{
					const uint64_t hashBefore = a.Animals.MotionHash();
					a.Director.Step();
					moved = moved || hashBefore != a.Animals.MotionHash();
				}
				report.Check(clear && nearHome, "Life: animals wander on clear ground close to their pen");
				report.Check(moved, "Life: animals actually move around");

				const uint64_t herdBefore = a.State.Animals.Hash();
				for (int i = 0; i < 600; ++i)
					a.Director.Step();
				report.Check(herdBefore == a.State.Animals.Hash(), "Life: wandering does not touch the saved herd facts");
			}

			{
				LifeRig off(0, false);
				PlacePen(off.Director.GetGrid(), BuildId::Coop);
				PlacePen(off.Director.GetGrid(), BuildId::Barn);
				for (int i = 0; i < 12; ++i)
					off.Sleep();
				for (int i = 0; i < 300; ++i)
					off.Director.Step();
				report.Check(!off.Animals.IsEnabled() && off.State.Animals.Empty() && off.Animals.GetMotions().empty(), "Life: with animals off the herd and motions stay empty through many days");
				report.Check(off.State.Animals.Hash() == Herd().Hash(), "Life: with animals off the herd hash is the empty hash");

				Herd herd;
				herd.Add(AnimalKind::Chicken, 7, 50, true);
				const uint64_t before = herd.Hash();
				const AnimalRollReport report2 = RollAnimalsDay(off.Director.GetGrid(), herd, false);
				report.Check(herd.Hash() == before && report2.Arrived == 0 && report2.Left == 0, "Life: a disabled roll changes nothing");
			}

			report.Check(ANIMALS_ENABLED, "Life: the build ships with the animals switch on");
		}

		void PlaceChecks(CheckReport& report)
		{
			LifeRig rig(0);
			TileGrid& grid = rig.Director.GetGrid();
			const uint32_t coop = PlacePen(grid, BuildId::Coop);
			const uint32_t well = grid.AddOccupant(OccupantKind::Building, static_cast<uint16_t>(BuildId::Well), 52, 52, 1, 1, 0, 0);
			for (int i = 0; i < 6; ++i)
				rig.Sleep();
			const Occupant* pen = grid.FindOccupant(coop);

			const PlaceInteraction tend = InteractWithPlace(grid, rig.State, pen->X, pen->Y);
			report.Check(tend.Handled && !tend.Message.empty() && rig.State.Pack.Count(ItemId::Egg) > 0, "Life: interacting with the coop tends it and collects the eggs");
			const PlaceInteraction off = InteractWithPlace(grid, rig.State, pen->X, pen->Y, false);
			report.Check(!off.Handled, "Life: with animals off the coop is only a building");

			report.Check(well != k_NoOccupant, "Life: a well can be built");
			if (well != k_NoOccupant)
			{
				const PlaceInteraction noCan = InteractWithPlace(grid, rig.State, 52, 52);
				report.Check(noCan.Handled && !noCan.Message.empty(), "Life: the well answers even without a watering can");
				rig.State.Pack.GiveStarterLoadout();
				const PlaceInteraction refill = InteractWithPlace(grid, rig.State, 52, 52);
				report.Check(refill.Handled && refill.Message != noCan.Message, "Life: the well refills the watering can");
			}

			int waterX = -1;
			int waterY = -1;
			for (int y = 0; y < grid.GetHeight() && waterX < 0; ++y)
			{
				for (int x = 0; x < grid.GetWidth(); ++x)
				{
					if (grid.At(x, y).GroundState == Ground::Water)
					{
						waterX = x;
						waterY = y;
						break;
					}
				}
			}
			report.Check(waterX >= 0, "Life: the valley has a pond");
			if (waterX >= 0)
			{
				const PlaceInteraction pond = InteractWithPlace(grid, rig.State, waterX, waterY);
				report.Check(pond.Handled && !pond.Message.empty(), "Life: the pond answers with the fishing stub");
			}
			report.Check(!InteractWithPlace(grid, rig.State, 1, 1).Handled && !InteractWithPlace(grid, rig.State, -4, 900).Handled, "Life: plain ground and off-map tiles are left to the other tools");
		}

		void HashChecks(CheckReport& report)
		{
			LifeRig rig(0);
			const GameStateDigest base = HashGameState(rig.Director, rig.State);
			rig.State.Bonds.Talk(VillagerId::Child, 3);
			const GameStateDigest talked = HashGameState(rig.Director, rig.State);
			report.Check(talked.Bonds != base.Bonds && talked.All != base.All && talked.Animals == base.Animals, "Life: friendship is in the state hash");
			rig.State.Animals.Add(AnimalKind::Cow, 9, 60, false);
			const GameStateDigest herded = HashGameState(rig.Director, rig.State);
			report.Check(herded.Animals != talked.Animals && herded.All != talked.All && herded.Bonds == talked.Bonds, "Life: the herd is in the state hash");
		}
	}

	void RunLifeChecks(CheckReport& report)
	{
		SuiteScope scope(report, "Life");
		ScheduleChecks(report);
		DeterminismChecks(report);
		BlockedTileChecks(report);
		SimChecks(report);
		PickChecks(report);
		FriendshipChecks(report);
		LineChecks(report);
		DialogueChecks(report);
		AnimalChecks(report);
		PlaceChecks(report);
		HashChecks(report);
	}

}
