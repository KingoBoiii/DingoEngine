#pragma once

#include "DingoEngine/Physics/2D/PhysicsTypes2D.h"

#include <glm/glm.hpp>

namespace Dingo
{

	// Abstraction over a 2D rigid-body simulation. A Physics2D owns a single
	// physics world plus the bodies and shapes created inside it, and is driven by
	// stepping it each frame. The concrete backend (Box2D) is selected by Create()
	// and never appears in this header — exactly like GraphicsContext hides NVRHI.
	//
	// Bodies and shapes are referred to through opaque handles so no backend type
	// leaks across the interface. The Scene is the usual driver: it builds bodies
	// from its ECS components, steps the world, and writes simulated transforms
	// back. Advanced users can reach the world directly via Scene::GetPhysics2D().
	//
	// (The 3D side mirrors this exactly: a Physics3D interface with its Jolt
	// backend behind it — see Physics/3D/.)
	class Physics2D
	{
	public:
		// Creates the default 2D physics backend (currently Box2D). The world is
		// not live until Initialize() is called.
		static Physics2D* Create();

	public:
		Physics2D() = default;
		virtual ~Physics2D() = default;

		Physics2D(const Physics2D&) = delete;
		Physics2D& operator=(const Physics2D&) = delete;

		// --- World lifecycle --------------------------------------------------

		// Creates the simulation world with the given gravity (world units/s^2).
		virtual void Initialize(const glm::vec2& gravity) = 0;
		// Destroys the world and every body/shape in it. Safe to call when idle.
		virtual void Shutdown() = 0;
		// True between Initialize() and Shutdown().
		virtual bool IsValid() const = 0;

		// Advances the simulation by deltaTime seconds using subStepCount solver
		// sub-steps. No-op when the world isn't live.
		virtual void Step(float deltaTime, int subStepCount) = 0;

		virtual void SetGravity(const glm::vec2& gravity) = 0;
		virtual glm::vec2 GetGravity() const = 0;

		// --- Bodies & shapes --------------------------------------------------

		// Creates a body from params and returns its handle (0 on failure).
		virtual PhysicsBodyId2D CreateBody(const RigidBodyParams2D& params) = 0;
		// Destroys a body and all shapes attached to it. No-op on a 0/stale handle.
		virtual void DestroyBody(PhysicsBodyId2D body) = 0;
		virtual bool IsBodyValid(PhysicsBodyId2D body) const = 0;

		// Attaches a collision shape to an existing body; returns its handle.
		virtual PhysicsShapeId2D AddBoxShape(PhysicsBodyId2D body, const BoxShapeParams2D& params) = 0;
		virtual PhysicsShapeId2D AddCircleShape(PhysicsBodyId2D body, const CircleShapeParams2D& params) = 0;

		// --- Body transform read-back -----------------------------------------

		virtual glm::vec2 GetPosition(PhysicsBodyId2D body) const = 0;
		virtual float GetAngle(PhysicsBodyId2D body) const = 0; // radians

		// --- Body controls (no-ops on a 0/stale handle) -----------------------

		virtual void SetLinearVelocity(PhysicsBodyId2D body, const glm::vec2& velocity) = 0;
		virtual glm::vec2 GetLinearVelocity(PhysicsBodyId2D body) const = 0;
		virtual void ApplyLinearImpulse(PhysicsBodyId2D body, const glm::vec2& impulse, const glm::vec2& worldPoint, bool wake = true) = 0;
		virtual void ApplyLinearImpulseToCenter(PhysicsBodyId2D body, const glm::vec2& impulse, bool wake = true) = 0;
		virtual void ApplyForceToCenter(PhysicsBodyId2D body, const glm::vec2& force, bool wake = true) = 0;

		template<typename T>
		T& As() { return static_cast<T&>(*this); }
	};

}
