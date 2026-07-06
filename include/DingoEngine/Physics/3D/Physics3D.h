#pragma once

#include "DingoEngine/Physics/3D/PhysicsTypes3D.h"
#include "DingoEngine/Physics/3D/CharacterController3D.h"
#include "DingoEngine/Core/Ray.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <memory>
#include <vector>

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
	// This can be driven directly — create bodies, Step() each frame, then read each
	// body's transform (GetTransform / GetPosition / GetRotation) to render it however
	// you like. As of v0.4.1 the Scene also wires it into the ECS via RigidBody3DComponent
	// (see Scene::OnUpdate), reading each simulated body's transform back automatically.
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

		// Teleports: set the body's transform directly, bypassing the simulation, and
		// re-activate it. Use for spawning / respawning / editor manipulation, not for
		// per-frame movement of dynamic bodies (that fights the solver). For smoothly
		// driven kinematic platforms use MoveKinematic instead.
		virtual void SetPosition(PhysicsBodyId3D body, const glm::vec3& position) = 0;
		virtual void SetRotation(PhysicsBodyId3D body, const glm::quat& rotation) = 0;
		virtual void SetPositionAndRotation(PhysicsBodyId3D body, const glm::vec3& position, const glm::quat& rotation) = 0;

		// Drives a Kinematic body toward the target transform over deltaTime by giving it
		// the velocity that reaches the target in one step. Unlike SetPosition (a teleport)
		// this carries/pushes resting dynamic bodies, so it is the right call for moving
		// platforms and elevators. deltaTime must match the next Step()'s deltaTime.
		virtual void MoveKinematic(PhysicsBodyId3D body, const glm::vec3& targetPosition, const glm::quat& targetRotation, float deltaTime) = 0;

		virtual void SetLinearVelocity(PhysicsBodyId3D body, const glm::vec3& velocity) = 0;
		virtual glm::vec3 GetLinearVelocity(PhysicsBodyId3D body) const = 0;
		virtual void SetAngularVelocity(PhysicsBodyId3D body, const glm::vec3& angularVelocity) = 0;
		virtual glm::vec3 GetAngularVelocity(PhysicsBodyId3D body) const = 0;
		virtual void ApplyImpulse(PhysicsBodyId3D body, const glm::vec3& impulse) = 0; // at center of mass
		virtual void ApplyForce(PhysicsBodyId3D body, const glm::vec3& force) = 0;     // at center of mass

		// --- Scene queries ----------------------------------------------------

		// Casts a ray and reports the closest body it hits within maxDistance. Returns
		// true and fills outHit on a hit; returns false (outHit unchanged) on a miss or
		// if the world isn't live. ray.Direction is expected to be normalized. Used for
		// picking, line-of-sight and hitscan weapons.
		virtual bool RayCast(const Ray& ray, float maxDistance, RayCastHit3D& outHit) const = 0;

		// Sweeps a sphere of `radius` from `center` along `direction` (expected normalized)
		// up to maxDistance and reports the closest body it hits. Returns true and fills
		// outHit on a hit; false (outHit unchanged) on a miss. A "thick" ray cast — use it
		// for melee reach or a projectile with volume.
		virtual bool ShapeCastSphere(const glm::vec3& center, const glm::vec3& direction, float radius, float maxDistance, RayCastHit3D& outHit) const = 0;

		// Collects every body whose shape overlaps a sphere at `center` with `radius` into
		// `out` (cleared first). Returns true if any were found. Cheap area query for melee
		// AoE, triggers and proximity checks; order is unspecified.
		virtual bool OverlapSphere(const glm::vec3& center, float radius, std::vector<PhysicsBodyId3D>& out) const = 0;

		// --- Character controller ---------------------------------------------

		// Creates a kinematic character controller (Jolt CharacterVirtual) in this world.
		// The returned controller is owned by the caller and MUST be destroyed before the
		// Physics3D it came from (it holds the world). Returns nullptr if the world isn't
		// live. See CharacterController3D for the per-frame drive contract.
		virtual std::unique_ptr<CharacterController3D> CreateCharacterController(const CharacterControllerParams3D& params) = 0;

		template<typename T>
		T& As() { return static_cast<T&>(*this); }
	};

}
