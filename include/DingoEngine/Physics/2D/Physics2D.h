#pragma once

#include "DingoEngine/Physics/2D/PhysicsTypes2D.h"

#include <glm/glm.hpp>

namespace Dingo
{

	/// Abstraction over a 2D rigid-body simulation. A Physics2D owns a single
	/// physics world plus the bodies and shapes created inside it, and is driven by
	/// stepping it each frame. The concrete backend (Box2D) is selected by Create()
	/// and never appears in this header - exactly like GraphicsContext hides NVRHI.
	///
	/// Bodies and shapes are referred to through opaque handles so no backend type
	/// leaks across the interface. The Scene is the usual driver: it builds bodies
	/// from its ECS components, steps the world, and writes simulated transforms
	/// back. Advanced users can reach the world directly via Scene::GetPhysics2D().
	///
	/// (The 3D side mirrors this exactly: a Physics3D interface with its Jolt
	/// backend behind it - see Physics/3D/.)
	///
	/// Main thread only. Every call that takes a body is a no-op, or returns zero, on a 0 or
	/// stale handle. Positions are world units (metres by convention), angles are radians.
	class Physics2D
	{
	public:
		/// Creates the default 2D physics backend (currently Box2D). The world is
		/// not live until Initialize() is called. The caller owns the result and deletes it.
		static Physics2D* Create();

	public:
		/// An empty object with no world; call Initialize() to bring the simulation up.
		Physics2D() = default;
		/// Destroys the world and everything in it, like Shutdown().
		virtual ~Physics2D() = default;

		Physics2D(const Physics2D&) = delete;
		Physics2D& operator=(const Physics2D&) = delete;

		// --- World lifecycle --------------------------------------------------

		/// Creates the simulation world with the given gravity (world units/s^2).
		/// No-op if the world is already live.
		virtual void Initialize(const glm::vec2& gravity) = 0;
		/// Destroys the world and every body/shape in it. Safe to call when idle.
		virtual void Shutdown() = 0;
		/// True between Initialize() and Shutdown().
		virtual bool IsValid() const = 0;

		/// Advances the simulation by deltaTime seconds using subStepCount solver
		/// sub-steps. No-op when the world isn't live.
		virtual void Step(float deltaTime, int subStepCount) = 0;

		/// Sets the gravity in world units/s^2; no-op when the world isn't live.
		virtual void SetGravity(const glm::vec2& gravity) = 0;
		/// The current gravity in world units/s^2; zero when the world isn't live.
		virtual glm::vec2 GetGravity() const = 0;

		// --- Bodies & shapes --------------------------------------------------

		/// Creates a body from params and returns its handle (0 on failure).
		/// A body has no collision until a shape is attached with AddBoxShape or AddCircleShape.
		virtual PhysicsBodyId2D CreateBody(const RigidBodyParams2D& params) = 0;
		/// Destroys a body and all shapes attached to it. No-op on a 0/stale handle.
		virtual void DestroyBody(PhysicsBodyId2D body) = 0;
		/// True while the handle names a live body in this world; false for 0 or a destroyed body.
		virtual bool IsBodyValid(PhysicsBodyId2D body) const = 0;

		/// Attaches a collision shape to an existing body; returns its handle (0 on a 0/stale
		/// body). A body can carry several shapes; their densities add up to its mass.
		///@{
		virtual PhysicsShapeId2D AddBoxShape(PhysicsBodyId2D body, const BoxShapeParams2D& params) = 0;
		virtual PhysicsShapeId2D AddCircleShape(PhysicsBodyId2D body, const CircleShapeParams2D& params) = 0;
		///@}

		// --- Body transform read-back -----------------------------------------

		/// World position of the body's origin; zero for a 0/stale handle.
		virtual glm::vec2 GetPosition(PhysicsBodyId2D body) const = 0;
		/// World rotation in radians, in (-pi, pi]; 0 for a 0/stale handle.
		virtual float GetAngle(PhysicsBodyId2D body) const = 0; // radians

		// --- Body controls (no-ops on a 0/stale handle) -----------------------

		/// Sets the body's linear velocity in world units per second.
		virtual void SetLinearVelocity(PhysicsBodyId2D body, const glm::vec2& velocity) = 0;
		/// The body's linear velocity in world units per second; zero for a 0/stale handle.
		virtual glm::vec2 GetLinearVelocity(PhysicsBodyId2D body) const = 0;
		/// The body's angular velocity in radians per second; 0 for a 0/stale handle.
		virtual float GetAngularVelocity(PhysicsBodyId2D body) const = 0; // radians per second

		/// Gives a Kinematic body the velocity that brings it to the target position and angle
		/// (radians) over deltaTime, which must match the next Step's. Unlike a teleport it pushes
		/// what it meets. No-op on a body that isn't Kinematic, or when deltaTime is not positive.
		virtual void MoveKinematic(PhysicsBodyId2D body, const glm::vec2& targetPosition, float targetAngle, float deltaTime) = 0;
		/// Applies an instantaneous impulse (mass * velocity change) at a world-space point, which
		/// also spins the body; wake wakes a sleeping body.
		virtual void ApplyLinearImpulse(PhysicsBodyId2D body, const glm::vec2& impulse, const glm::vec2& worldPoint, bool wake = true) = 0;
		/// Applies an instantaneous impulse at the center of mass, so it adds no spin; wake wakes a
		/// sleeping body.
		virtual void ApplyLinearImpulseToCenter(PhysicsBodyId2D body, const glm::vec2& impulse, bool wake = true) = 0;
		/// Applies a force at the center of mass for the next Step only; call it every frame to
		/// keep pushing. wake wakes a sleeping body.
		virtual void ApplyForceToCenter(PhysicsBodyId2D body, const glm::vec2& force, bool wake = true) = 0;
	};

}
