#include "Life/VillagerSim.h"
#include "Life/PlayerMath.h"

#include <cmath>

namespace Dingo
{

	float MinuteOfDay(const Calendar& calendar)
	{
		return static_cast<float>(DAY_START_MINUTES) + static_cast<float>(calendar.GetTickOfDay()) / static_cast<float>(TICKS_PER_MINUTE);
	}

	std::optional<VillagerPick> PickNearby(const VillagerPlaces& places, const glm::vec2& feet, float yaw, float radius, float arcDegrees)
	{
		const glm::vec2 facing(std::sin(yaw), std::cos(yaw));
		const float minDot = std::cos(arcDegrees * PlayerMath::PI / 180.0f);
		std::optional<VillagerPick> best;
		for (int i = 0; i < VILLAGER_COUNT; ++i)
		{
			const VillagerPlace& place = places[static_cast<size_t>(i)];
			if (place.Indoors)
				continue;
			const glm::vec2 offset = place.Position - feet;
			const float distance = glm::length(offset);
			if (distance > radius)
				continue;
			if (distance > 0.3f && glm::dot(offset / distance, facing) < minDot)
				continue;
			if (!best || distance < best->Distance)
				best = VillagerPick{ static_cast<VillagerId>(i), distance };
		}
		return best;
	}

	std::optional<VillagerPick> PickNearPoint(const VillagerPlaces& places, const glm::vec2& point, float radius)
	{
		std::optional<VillagerPick> best;
		for (int i = 0; i < VILLAGER_COUNT; ++i)
		{
			const VillagerPlace& place = places[static_cast<size_t>(i)];
			if (place.Indoors)
				continue;
			const float distance = glm::length(place.Position - point);
			if (distance <= radius && (!best || distance < best->Distance))
				best = VillagerPick{ static_cast<VillagerId>(i), distance };
		}
		return best;
	}

	VillagerSim::VillagerSim(ValleyDirector& director)
	{
		director.OnStage(SimStage::Villagers, [this](ValleyDirector& d, const StepInfo&) { Update(d); });
		Update(director);
	}

	void VillagerSim::Update(const ValleyDirector& director)
	{
		const Calendar& calendar = director.GetCalendar();
		const ScheduleVariant variant = VariantFor(calendar.GetWeather());
		if (!m_PlansValid || variant != m_Variant)
		{
			for (int i = 0; i < VILLAGER_COUNT; ++i)
				m_Plans[static_cast<size_t>(i)] = BuildDayPlan(director.GetGrid(), static_cast<VillagerId>(i), variant);
			m_Variant = variant;
			m_PlansValid = true;
		}

		const float minute = MinuteOfDay(calendar);
		for (int i = 0; i < VILLAGER_COUNT; ++i)
			m_Places[static_cast<size_t>(i)] = PlaceOnPlan(m_Plans[static_cast<size_t>(i)], minute);
	}

}
