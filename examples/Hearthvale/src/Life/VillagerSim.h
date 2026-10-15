#pragma once
#include "Life/Schedule.h"
#include "ValleyDirector.h"

#include <array>
#include <optional>

namespace Dingo
{

	using VillagerPlaces = std::array<VillagerPlace, VILLAGER_COUNT>;

	struct VillagerPick
	{
		VillagerId Who = VillagerId::Shopkeeper;
		float Distance = 0.0f;
	};

	// The nearest villager standing outdoors within `radius` of `feet` and inside the `arcDegrees` cone the farmer faces.
	std::optional<VillagerPick> PickNearby(const VillagerPlaces& places, const glm::vec2& feet, float yaw, float radius, float arcDegrees);

	// The nearest villager standing outdoors within `radius` of a world (x, z) point, for picking with the mouse ray.
	std::optional<VillagerPick> PickNearPoint(const VillagerPlaces& places, const glm::vec2& point, float radius);

	// Moves the four villagers along their schedules at SimStage::Villagers. Plain C++: no renderer, runs headless.
	// The plans are resolved against the grid, so the layer calls InvalidatePlans when the grid changes.
	class VillagerSim
	{
	public:
		explicit VillagerSim(ValleyDirector& director);
		VillagerSim(const VillagerSim&) = delete;
		VillagerSim& operator=(const VillagerSim&) = delete;

		void InvalidatePlans() { m_PlansValid = false; }
		void Update(const ValleyDirector& director);

		const VillagerPlaces& GetPlaces() const { return m_Places; }
		const VillagerPlace& GetPlace(VillagerId villager) const { return m_Places[static_cast<size_t>(villager)]; }
		const DayPlan& GetPlan(VillagerId villager) const { return m_Plans[static_cast<size_t>(villager)]; }

	private:
		std::array<DayPlan, VILLAGER_COUNT> m_Plans;
		VillagerPlaces m_Places{};
		bool m_PlansValid = false;
		ScheduleVariant m_Variant = ScheduleVariant::Fair;
	};

	float MinuteOfDay(const Calendar& calendar);

}
