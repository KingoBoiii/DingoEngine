#pragma once

#include "DingoEngine/Physics/3D/PhysicsTypes3D.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>

namespace Dingo
{

	// Configuration for a 3D physics world. MaxBodies is fixed at Initialize() time
	// because the backend (Jolt) pre-allocates its body storage up front.
	struct Physics3DParams
	{
		glm::vec3 Gravity{ 0.0f, -9.81f, 0.0f };
		std::uint32_t MaxBodies = 65536;
	};

	// Abstraction over a 3D rigid-body simulation. A Physics3D owns a single physics
	// world plus the bodies created inside it, and is driven by stepping it each
	// frame. The concrete backend (Jolt) is selected by Create() and never appears in
	// this header — exactly like Physics2D hides Box2D and GraphicsContext hides NVRHI.
	//
	// Bodies are referred to through opaque PhysicsBodyId3D handles so no backend type
	// leaks across the interface.
	//
	// Unlike the 2D Scene physics, this is NOT wired into the ECS: you drive it
	// directly — create bodies, Step() each frame, then read each body's transform
	// (GetTransform / GetPosition / GetRotation) to render it however you like.
	class Physics3D
	{
	public:
		// Creates the default 3D physics backend (currently Jolt). The world is not
		// live until Initialize() is called.
		static Physics3D* Create();

	public:
		Physics3D() = default;
		virtual ~Physics3D() = default;

		Physics3D(const Physics3D&) = delete;
		Physics3D& operator=(const Physics3D&) = delete;

		// --- World lifecycle --------------------------------------------------

		// Creates the simulation world from params. No-op if already live.
		virtual void Initialize(const Physics3DParams& params) = 0;
		// Destroys the world and every body in it. Safe to call when idle.
		virtual void Shutdown() = 0;
		// True between Initialize() and Shutdown().
		virtual bool IsValid() const = 0;

		// --- Bodies -----------------------------------------------------------

		// Creates a body from params and returns its handle (k_InvalidBody3D on failure).
		virtual PhysicsBodyId3D CreateBody(const RigidBodyParams3D& params) = 0;
		// Destroys a body. No-op on an invalid/stale handle.
		virtual void DestroyBody(PhysicsBodyId3D body) = 0;
		virtual bool IsBodyValid(PhysicsBodyId3D body) const = 0;

		// --- Simulation -------------------------------------------------------

		// Advances the world by deltaTime. Use more collision steps for large steps
		// (Jolt recommends one step per 1/60s, rounded up).
		virtual void Step(float deltaTime, int collisionSteps = 1) = 0;

		virtual void SetGravity(const glm::vec3& gravity) = 0;
		virtual glm::vec3 GetGravity() const = 0;

		// --- Per-body state (no-ops / identity if the body isn't valid) -------

		virtual glm::vec3 GetPosition(PhysicsBodyId3D body) const = 0;
		virtual glm::quat GetRotation(PhysicsBodyId3D body) const = 0;
		virtual glm::mat4 GetTransform(PhysicsBodyId3D body) const = 0; // translation * rotation (no scale)

		virtual void SetLinearVelocity(PhysicsBodyId3D body, const glm::vec3& velocity) = 0;
		virtual glm::vec3 GetLinearVelocity(PhysicsBodyId3D body) const = 0;
		virtual void ApplyImpulse(PhysicsBodyId3D body, const glm::vec3& impulse) = 0; // at center of mass
		virtual void ApplyForce(PhysicsBodyId3D body, const glm::vec3& force) = 0;     // at center of mass

		template<typename T>
		T& As() { return static_cast<T&>(*this); }
	};

}
