#pragma once
#include <DingoEngine.h>

#include "Life/PlayerIntent.h"
#include "World/TileGrid.h"

namespace Dingo
{

	class FarmerAssets;

	// The farmer: a character controller driven by a PlayerIntent. The model mode draws the KayKit character with
	// idle and walk clips; without one it is a box and a head. GPU side: never constructed headless.
	class Player
	{
	public:
		// `assets` null, or not usable, gives the placeholder body.
		Player(Scene& scene, const glm::vec3& feet, const FarmerAssets* assets);

		// Call once per sim step, after the intent was filled and before the scene steps its physics.
		void Step(const PlayerIntent& intent, const TileGrid& grid, float cameraYaw, float dt);

		void Teleport(const glm::vec3& feet);
		// Turns the farmer on the spot, e.g. toward a mouse target; the next Step keeps it.
		void SetYaw(float yaw);

		glm::vec3 GetFeet() const;
		float GetYaw() const { return m_Yaw; }
		bool IsMoving() const { return m_Moving; }
		bool IsModel() const { return m_Model; }
		Entity GetEntity() const { return m_Entity; }

		// A static slab under the whole valley, top at GROUND_TOP, for the controller to stand on. Walls, water and
		// props are kept out by PlayerMath::ResolveMove, not by colliders.
		static Entity SpawnFloor(Scene& scene, int tilesWide, int tilesDeep);

	private:
		void Animate(bool moving);

	private:
		Scene& m_Scene;
		Entity m_Entity;
		bool m_Model = false;
		const AnimationClip* m_Idle = nullptr;
		const AnimationClip* m_Walk = nullptr;
		float m_Yaw = 0.0f;
		float m_VerticalVelocity = 0.0f;
		bool m_Moving = false;
	};

}
