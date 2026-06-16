#pragma once
#include <glm/glm.hpp>

// Game-specific ECS components for the Space Invaders example. These live in the
// game (not the engine) and are attached to entities created in the game scene —
// demonstrating that gameplay code can define and query its own component types
// on top of the engine's generic Scene/Entity/Component system.
namespace Dingo
{

	// Marks the player ship.
	struct PlayerTag
	{
	};

	// Marks an invader and records its slot in the formation grid.
	struct InvaderTag
	{
		int Row = 0;
		int Column = 0;
	};

	// Marks a projectile. FromPlayer distinguishes the player's shot (travels up)
	// from invader bombs (travel down).
	struct BulletTag
	{
		bool FromPlayer = true;
	};

	// Marks a destructible shield block.
	struct ShieldTag
	{
	};

	// Linear velocity in world units / second (used by projectiles).
	struct VelocityComponent
	{
		glm::vec2 Velocity{ 0.0f };
	};

}
