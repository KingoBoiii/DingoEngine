#pragma once
#include <DingoEngine.h>

#include "Life/VillagerSim.h"

#include <array>
#include <optional>

namespace Dingo
{

	// The four villagers in the scene: a kinematic sensor box each (so the mouse ray finds them and the farmer never
	// bumps into them) carrying the placeholder body and head from the art registry. GPU side: never constructed headless.
	class VillagerWorld
	{
	public:
		explicit VillagerWorld(Scene& scene);

		// Moves each villager to its place and hides the ones indoors.
		void Sync(const VillagerPlaces& places, float dt);

		// The villager under a ray from the camera, within `reach` metres of the ray origin.
		std::optional<VillagerId> Pick(const Ray& ray, float reach) const;
		std::optional<VillagerId> VillagerOf(Entity entity) const;
		Entity GetEntity(VillagerId villager) const { return m_Roots[static_cast<size_t>(villager)]; }

	private:
		Scene& m_Scene;
		std::array<Entity, VILLAGER_COUNT> m_Roots{};
		std::array<float, VILLAGER_COUNT> m_HalfHeight{};
	};

}
