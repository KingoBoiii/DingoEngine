#include "Checks/LifeWorldChecks.h"
#include "Farm/Animals.h"
#include "GameTuning.h"
#include "LaunchOptions.h"
#include "Life/AnimalWorld.h"
#include "Life/VillagerSim.h"
#include "Life/VillagerWorld.h"
#include "ValleyDirector.h"

#include <cmath>
#include <format>
#include <limits>

namespace Dingo
{

	namespace
	{
		constexpr int k_SettleFrames = 8;
		constexpr int k_DriftFrames = 30;
		constexpr int k_NoonTick = (720 - DAY_START_MINUTES) * TICKS_PER_MINUTE;
	}

	bool LifeWorldCheckRun::Finish(CheckReport& report, ValleyDirector& director)
	{
		Calendar& calendar = director.GetCalendar();
		calendar.Reset(m_Day, m_Tick);
		calendar.ForceWeather(static_cast<Weather>(m_Weather));
		director.SetTimePaused(m_Paused);
		const int passed = report.GetPassed() - m_PassedAtStart;
		const int failed = report.GetFailed() - m_FailedAtStart;
		report.AddSuite("Life world", passed, failed);
		DE_INFO("Life world checks: {} passed, {} failed", passed, failed);
		m_Step = Step::Done;
		return true;
	}

	bool LifeWorldCheckRun::StartAnimals(CheckReport& report, ValleyDirector& director, AnimalWorld* animals, Herd& herd)
	{
		if (!animals)
		{
			report.Check(!ANIMALS_ENABLED, "Life world: with animals off no animal world exists");
			return Finish(report, director);
		}
		// Frozen, the director never runs the Animals stage, so no animal gets an entity.
		if (GetLaunchOptions().Freeze)
			return Finish(report, director);
		report.Check(animals->EntityCount() == herd.Count(), "Life world: the animal entities match the herd");
		herd.Add(AnimalKind::Chicken, k_NoOccupant, ANIMAL_HAPPINESS_START, true);
		herd.Add(AnimalKind::Cow, k_NoOccupant, ANIMAL_HAPPINESS_START, true);
		m_Frames = 0;
		m_Step = Step::Animals;
		return false;
	}

	bool LifeWorldCheckRun::Tick(CheckReport& report, ValleyDirector& director, const VillagerSim& sim, VillagerWorld* villagers, AnimalWorld* animals, Herd& herd)
	{
		switch (m_Step)
		{
			case Step::Start:
			{
				m_PassedAtStart = report.GetPassed();
				m_FailedAtStart = report.GetFailed();
				Calendar& calendar = director.GetCalendar();
				m_Day = calendar.GetAbsoluteDay();
				m_Tick = calendar.GetTickOfDay();
				m_Weather = static_cast<int>(calendar.GetWeather());
				m_Paused = director.IsTimePaused();
				if (!villagers)
				{
					report.Check(false, "Life world: the scene has a villager world");
					return Finish(report, director);
				}
				calendar.Reset(m_Day, k_NoonTick);
				calendar.ForceWeather(Weather::Clear);
				director.SetTimePaused(true);
				m_Frames = 0;
				m_Step = Step::Settle;
				return false;
			}

			case Step::Settle:
			{
				if (++m_Frames < k_SettleFrames)
					return false;
				m_Step = Step::Villagers;
				return false;
			}

			case Step::Villagers:
			{
				const VillagerPlaces& places = sim.GetPlaces();
				int outdoors = 0;
				bool visibleMatches = true;
				bool pickedOutdoors = true;
				bool ignoredIndoors = true;
				for (int i = 0; i < VILLAGER_COUNT; ++i)
				{
					const VillagerId id = static_cast<VillagerId>(i);
					const VillagerPlace& place = places[static_cast<size_t>(i)];
					Entity root = villagers->GetEntity(id);
					visibleMatches = visibleMatches && root.IsValid() && root.IsActive() == !place.Indoors;

					const Ray above(glm::vec3(place.Position.x, GROUND_TOP + 4.0f, place.Position.y), glm::vec3(0.0f, -1.0f, 0.0f));
					const std::optional<VillagerId> hit = villagers->Pick(above, 10.0f);
					if (place.Indoors)
					{
						ignoredIndoors = ignoredIndoors && !hit;
						continue;
					}
					++outdoors;
					pickedOutdoors = pickedOutdoors && hit && *hit == id;
				}
				// Frozen, the director never runs the Villagers stage and the scene never steps, so nobody is
				// placed outdoors and there is no walk or drift to measure.
				const bool frozen = GetLaunchOptions().Freeze;
				if (!frozen)
					report.Check(outdoors >= 1, std::format("Life world: {} villagers are out at noon", outdoors));
				report.Check(visibleMatches, "Life world: a villager is visible exactly when outdoors");
				if (!frozen)
					report.Check(pickedOutdoors, "Life world: a ray from above finds each outdoor villager and maps back to it");
				report.Check(ignoredIndoors, "Life world: a villager indoors cannot be picked");

				const Ray empty(glm::vec3(1.5f, GROUND_TOP + 4.0f, 1.5f), glm::vec3(0.0f, -1.0f, 0.0f));
				report.Check(!villagers->Pick(empty, 10.0f), "Life world: a ray over empty ground finds nobody");

				if (frozen)
					return StartAnimals(report, director, animals, herd);

				int walkTick = -1;
				for (int i = 0; i < VILLAGER_COUNT && walkTick < 0; ++i)
				{
					for (const PlanStop& stop : sim.GetPlan(static_cast<VillagerId>(i)).Stops)
					{
						if (stop.Route.size() >= 2 && stop.Minute >= DAY_START_MINUTES && stop.Minute < DAY_START_MINUTES + DAY_MINUTES - 10)
						{
							walkTick = (stop.Minute - DAY_START_MINUTES) * TICKS_PER_MINUTE;
							break;
						}
					}
				}
				report.Check(walkTick >= 0, "Life world: some villager walks a route today");
				if (walkTick >= 0)
				{
					director.GetCalendar().Reset(m_Day, walkTick);
					director.GetCalendar().ForceWeather(Weather::Clear);
				}
				director.SetTimePaused(false);
				m_Frames = 0;
				m_Measured = 0;
				m_Drift = 0.0f;
				m_Walked = false;
				m_Step = Step::Drift;
				return false;
			}

			case Step::Drift:
			{
				const VillagerPlaces& places = sim.GetPlaces();
				for (int i = 0; i < VILLAGER_COUNT; ++i)
				{
					const VillagerPlace& place = places[static_cast<size_t>(i)];
					if (place.Indoors)
						continue;
					m_Walked = m_Walked || place.Moving;
					const std::optional<glm::vec3> body = villagers->GetBodyPosition(static_cast<VillagerId>(i));
					if (!body)
						continue;
					++m_Measured;
					const float drift = glm::length(glm::vec2(body->x, body->z) - place.Position);
					m_Drift = std::isfinite(drift) ? (std::max)(m_Drift, drift) : std::numeric_limits<float>::infinity();
				}
				if (++m_Frames < k_DriftFrames)
					return false;

				director.SetTimePaused(true);
				report.Check(m_Walked, "Life world: a villager walks while time runs at several steps a frame");
				report.Check(m_Measured > 0 && m_Drift < 0.1f, std::format("Life world: each villager's body stays on its place at {} sim steps a frame ({} bodies measured, off by up to {:.3f} m)", k_DriftSteps, m_Measured, m_Drift));
				return StartAnimals(report, director, animals, herd);
			}

			case Step::Animals:
			{
				if (++m_Frames < 4)
					return false;
				report.Check(animals->EntityCount() == 2, "Life world: each animal in the herd gets an entity");
				herd.Clear();
				herd.Add(AnimalKind::Cow, k_NoOccupant, ANIMAL_HAPPINESS_START, true);
				herd.Add(AnimalKind::Chicken, k_NoOccupant, ANIMAL_HAPPINESS_START, true);
				m_Frames = 0;
				m_Step = Step::Replaced;
				return false;
			}

			case Step::Replaced:
			{
				if (++m_Frames < 4)
					return false;
				report.Check(animals->EntityCount() == 2 && animals->KindOf(1) == AnimalKind::Cow && animals->KindOf(2) == AnimalKind::Chicken,
					"Life world: a replaced herd whose ids repeat gets the new kinds' bodies");
				herd.Clear();
				m_Frames = 0;
				m_Step = Step::Cleared;
				return false;
			}

			case Step::Cleared:
			{
				if (++m_Frames < 4)
					return false;
				report.Check(animals->EntityCount() == 0, "Life world: the entities go when the animals do");
				return Finish(report, director);
			}

			case Step::Done:
				return true;
		}
		return true;
	}

}
