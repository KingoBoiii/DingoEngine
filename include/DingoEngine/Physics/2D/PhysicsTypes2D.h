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

	/// How a body participates in the simulation.
	enum class BodyType2D
	{
		Static = 0, ///< Never moves (ground, walls); zero mass.
		Dynamic,    ///< Fully simulated - gravity, forces, collisions.
		Kinematic   ///< Moves only by the velocity you set; ignores forces/gravity.
	};

	// Opaque handles to a simulated body / collision shape inside a Physics2D world.
	// No backend (Box2D) type ever appears in the public API; this is all the
	// client ever holds. Valid only while that world is alive; 0 means "none".

	/// Opaque handle to a body in a Physics2D world; 0 (k_InvalidBody2D) means none.
	using PhysicsBodyId2D = std::uint64_t;
	/// The "no body" handle; every Physics2D body call ignores it.
	inline constexpr PhysicsBodyId2D k_InvalidBody2D = 0;
	/// Opaque handle to a collision shape on a body; 0 means none.
	using PhysicsShapeId2D = std::uint64_t;

	/// Describes a rigid body to create. Position is in world units and Rotation is
	/// in radians (the engine's TransformComponent stores degrees, so the Scene
	/// converts at the boundary). Collider shapes are attached separately because a
	/// 2D entity carries its colliders as their own ECS components.
	struct RigidBodyParams2D
	{
		/// Static by default, so a body stays put unless asked.
		BodyType2D Type = BodyType2D::Static;
		/// World position of the body origin, in world units.
		glm::vec2 Position{ 0.0f };
		float Rotation = 0.0f;                ///< Initial angle in radians, counter-clockwise.
		bool FixedRotation = false;           ///< Lock rotation about Z.
	};

	/// Describes a box collision shape, in resolved world units (any
	/// fraction-of-extent conventions are applied by the caller before this point).
	/// Center is relative to the body origin.
	struct BoxShapeParams2D
	{
		glm::vec2 HalfExtents{ 0.5f }; ///< Half the box's width and height, in world units.
		glm::vec2 Center{ 0.0f };      ///< Box center relative to the body origin, in world units.

		float Density = 1.0f;     ///< Mass per area; with the area it sets a Dynamic body's mass.
		/// Coulomb friction coefficient, 0 for ice and 1 for rubber-like grip.
		float Friction = 0.5f;
		float Restitution = 0.0f; ///< Bounciness, [0,1].
	};

	/// Describes a circle collision shape, in resolved world units.
	struct CircleShapeParams2D
	{
		float Radius = 0.5f;      ///< Circle radius in world units.
		glm::vec2 Center{ 0.0f }; ///< Circle center relative to the body origin, in world units.

		float Density = 1.0f;     ///< Mass per area; with the area it sets a Dynamic body's mass.
		/// Coulomb friction coefficient, 0 for ice and 1 for rubber-like grip.
		float Friction = 0.5f;
		float Restitution = 0.0f; ///< Bounciness, [0,1].
	};

}
