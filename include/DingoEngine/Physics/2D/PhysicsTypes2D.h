#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace Dingo
{

	// Backend-agnostic 2D physics value types. These describe what to simulate
	// without naming any physics backend (Box2D, etc.), so they are safe to use
	// from public headers and client code. The concrete backend lives behind the
	// Physics2D interface and translates these into its own representation.
	// (Mirrors the 3D side's PhysicsTypes3D.h.)

	// How a body participates in the simulation.
	enum class BodyType2D
	{
		Static = 0, // never moves (ground, walls); zero mass
		Dynamic,    // fully simulated — gravity, forces, collisions
		Kinematic   // moves only by the velocity you set; ignores forces/gravity
	};

	// Opaque handles to a simulated body / collision shape inside a Physics2D world.
	// No backend (Box2D) type ever appears in the public API; this is all the
	// client ever holds. Valid only while that world is alive; 0 means "none".
	using PhysicsBodyId2D = std::uint64_t;
	using PhysicsShapeId2D = std::uint64_t;

	// Describes a rigid body to create. Position is in world units and Rotation is
	// in radians (the engine's TransformComponent stores degrees, so the Scene
	// converts at the boundary). Collider shapes are attached separately because a
	// 2D entity carries its colliders as their own ECS components.
	struct RigidBodyParams2D
	{
		BodyType2D Type = BodyType2D::Static;
		glm::vec2 Position{ 0.0f };
		float Rotation = 0.0f;        // radians
		bool FixedRotation = false;   // lock rotation about Z
	};

	// Describes a box collision shape, in resolved world units (any
	// fraction-of-extent conventions are applied by the caller before this point).
	// Center is relative to the body origin.
	struct BoxShapeParams2D
	{
		glm::vec2 HalfExtents{ 0.5f };
		glm::vec2 Center{ 0.0f };

		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.0f; // bounciness, [0,1]
	};

	// Describes a circle collision shape, in resolved world units.
	struct CircleShapeParams2D
	{
		float Radius = 0.5f;
		glm::vec2 Center{ 0.0f };

		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.0f;
	};

}
