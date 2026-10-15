#include "Checks/LifeWorldChecks.h"
#include "Farm/Animals.h"
#include "GameTuning.h"
#include "Life/AnimalWorld.h"
#include "Life/VillagerSim.h"
#include "Life/VillagerWorld.h"
#include "ValleyDirector.h"

#include <format>

namespace Dingo
{

	namespace
	{
		constexpr int k_SettleFrames = 8;
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
				calendar.ForceWeather(Weather::Clear);
				calendar.Reset(m_Day, k_NoonTick);
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
				report.Check(outdoors >= 1, std::format("Life world: {} villagers are out at noon", outdoors));
				report.Check(visibleMatches, "Life world: a villager is visible exactly when outdoors");
				report.Check(pickedOutdoors, "Life world: a ray from above finds each outdoor villager and maps back to it");
				report.Check(ignoredIndoors, "Life world: a villager indoors cannot be picked");

				const Ray empty(glm::vec3(1.5f, GROUND_TOP + 4.0f, 1.5f), glm::vec3(0.0f, -1.0f, 0.0f));
				report.Check(!villagers->Pick(empty, 10.0f), "Life world: a ray over empty ground finds nobody");

				if (!animals)
				{
					report.Check(!ANIMALS_ENABLED, "Life world: with animals off no animal world exists");
					return Finish(report, director);
				}
				report.Check(animals->EntityCount() == herd.Count(), "Life world: the animal entities match the herd");
				herd.Add(AnimalKind::Chicken, k_NoOccupant, ANIMAL_HAPPINESS_START, true);
				herd.Add(AnimalKind::Cow, k_NoOccupant, ANIMAL_HAPPINESS_START, true);
				m_Frames = 0;
				m_Step = Step::Animals;
				return false;
			}

			case Step::Animals:
			{
				if (++m_Frames < 4)
					return false;
				report.Check(animals->EntityCount() == 2, "Life world: each animal in the herd gets an entity");
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
