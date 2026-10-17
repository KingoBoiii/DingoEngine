#pragma once
#include <DingoEngine.h>

#include "Life/VillagerSim.h"

#include <array>
#include <optional>

namespace Dingo
{

	// The four villagers in the scene: a kinematic sensor box each (so the mouse ray finds them and the farmer never
	// bumps into them) carrying a tinted KayKit character, or the registry's placeholder body when it did not load.
	// GPU side: never constructed headless.
	class FarmerAssets;

	class VillagerWorld
	{
	public:
		VillagerWorld(Scene& scene, const FarmerAssets* assets);

		// Moves each villager to its place and hides the ones indoors.
		void Sync(const VillagerPlaces& places, float dt);

		// The villager under a ray from the camera, within `reach` metres of the ray origin.
		std::optional<VillagerId> Pick(const Ray& ray, float reach) const;
		std::optional<VillagerId> VillagerOf(Entity entity) const;
		Entity GetEntity(VillagerId villager) const { return m_Roots[static_cast<size_t>(villager)]; }
		// Where the villager's physics body is, or nothing while it has none (indoors, or no physics).
		std::optional<glm::vec3> GetBodyPosition(VillagerId villager) const;

	private:
		Scene& m_Scene;
		std::array<Entity, VILLAGER_COUNT> m_Roots{};
		std::array<float, VILLAGER_COUNT> m_HalfHeight{};
		std::array<Entity, VILLAGER_COUNT> m_Visuals{};
		const AnimationClip* m_Idle = nullptr;
		const AnimationClip* m_Walk = nullptr;
	};

}
