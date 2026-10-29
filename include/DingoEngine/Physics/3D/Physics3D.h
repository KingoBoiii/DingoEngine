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

	/// Configuration for a 3D physics world. MaxBodies is fixed at Initialize() time
	/// because the backend (Jolt) pre-allocates its body storage up front.
	struct Physics3DParams
	{
		/// World gravity in metres per second squared; default is Earth, -Y.
		glm::vec3 Gravity{ 0.0f, -9.81f, 0.0f };
		/// Capacity of the body storage, fixed at Initialize(); CreateBody fails past it.
		std::uint32_t MaxBodies = 65536;
	};

	/// Abstraction over a 3D rigid-body simulation. A Physics3D owns a single physics
	/// world plus the bodies created inside it, and is driven by stepping it each
	/// frame. The concrete backend (Jolt) is selected by Create() and never appears in
	/// this header - exactly like Physics2D hides Box2D and GraphicsContext hides NVRHI.
	///
	/// Bodies are referred to through opaque PhysicsBodyId3D handles so no backend type
	/// leaks across the interface.
	///
	/// This can be driven directly - create bodies, Step() each frame, then read each
	/// body's transform (GetTransform / GetPosition / GetRotation) to render it however
	/// you like. As of v0.4.1 the Scene also wires it into the ECS via RigidBody3DComponent
	/// (see Scene::OnUpdate), reading each simulated body's transform back automatically.
	///
	/// Main thread only. Positions are metres, rotations are glm quaternions. Every call that takes
	/// a body is a safe no-op, or returns zero/identity, on k_InvalidBody3D.
	class Physics3D
	{
	public:
		/// Creates the default 3D physics backend (currently Jolt). The world is not
		/// live until Initialize() is called. The caller owns the result and deletes it.
		static Physics3D* Create();

	public:
		/// An empty object with no world; call Initialize() to bring the simulation up.
		Physics3D() = default;
		/// Destroys the world and everything in it, like Shutdown(); character controllers made
		/// from it must be gone first.
		virtual ~Physics3D() = default;

		Physics3D(const Physics3D&) = delete;
		Physics3D& operator=(const Physics3D&) = delete;

		// --- World lifecycle --------------------------------------------------

		/// Creates the simulation world from params. No-op if already live.
		virtual void Initialize(const Physics3DParams& params) = 0;
		/// Destroys the world and every body in it. Safe to call when idle.
		virtual void Shutdown() = 0;
		/// True between Initialize() and Shutdown().
		virtual bool IsValid() const = 0;

		// --- Bodies -----------------------------------------------------------

		/// Creates a body from params and returns its handle (k_InvalidBody3D on failure: the world
		/// isn't live, the body storage is full, or the collider can't be built).
		virtual PhysicsBodyId3D CreateBody(const RigidBodyParams3D& params) = 0;
		/// Destroys a body; k_InvalidBody3D is ignored. Destroy each handle once: the backend does
		/// not check that another handle is still live, so pass only ones IsBodyValid accepts.
		virtual void DestroyBody(PhysicsBodyId3D body) = 0;
		/// True while the handle names a body in this world; false for k_InvalidBody3D or a
		/// destroyed body.
		virtual bool IsBodyValid(PhysicsBodyId3D body) const = 0;

		// --- Simulation -------------------------------------------------------

		/// Advances the world by deltaTime seconds. Use more collision steps for large steps
		/// (Jolt recommends one step per 1/60s, rounded up). No-op when the world isn't live or
		/// deltaTime is not positive.
		virtual void Step(float deltaTime, int collisionSteps = 1) = 0;

		/// Sets the world gravity in metres per second squared; no-op when the world isn't live.
		virtual void SetGravity(const glm::vec3& gravity) = 0;
		/// The world gravity in metres per second squared; zero when the world isn't live.
		virtual glm::vec3 GetGravity() const = 0;

		// --- Per-body state (no-ops / identity if the body isn't valid) -------

		/// World position of the body's origin; zero for an invalid handle.
		virtual glm::vec3 GetPosition(PhysicsBodyId3D body) const = 0;
		/// World rotation of the body; identity for an invalid handle.
		virtual glm::quat GetRotation(PhysicsBodyId3D body) const = 0;
		/// World transform: translation * rotation (no scale); identity for an invalid handle.
		virtual glm::mat4 GetTransform(PhysicsBodyId3D body) const = 0; // translation * rotation (no scale)

		/// Teleports: set the body's transform directly, bypassing the simulation, and
		/// re-activate it. Use for spawning / respawning / editor manipulation, not for
		/// per-frame movement of dynamic bodies (that fights the solver). For smoothly
		/// driven kinematic platforms use MoveKinematic instead.
		///@{
		virtual void SetPosition(PhysicsBodyId3D body, const glm::vec3& position) = 0;
		virtual void SetRotation(PhysicsBodyId3D body, const glm::quat& rotation) = 0;
		virtual void SetPositionAndRotation(PhysicsBodyId3D body, const glm::vec3& position, const glm::quat& rotation) = 0;
		///@}

		/// Drives a Kinematic body toward the target transform over deltaTime by giving it
		/// the velocity that reaches the target in one step. Unlike SetPosition (a teleport)
		/// this carries/pushes resting dynamic bodies, so it is the right call for moving
		/// platforms and elevators. deltaTime must match the next Step()'s deltaTime. Only for
		/// Kinematic bodies (non-Release builds warn and ignore the call on any other); no-op when
		/// deltaTime is not positive.
		virtual void MoveKinematic(PhysicsBodyId3D body, const glm::vec3& targetPosition, const glm::quat& targetRotation, float deltaTime) = 0;

		/// Sets the linear velocity in world metres per second.
		virtual void SetLinearVelocity(PhysicsBodyId3D body, const glm::vec3& velocity) = 0;
		/// The linear velocity in world metres per second; zero for an invalid handle.
		virtual glm::vec3 GetLinearVelocity(PhysicsBodyId3D body) const = 0;
		/// Sets the angular velocity as a world-space axis * radians per second.
		virtual void SetAngularVelocity(PhysicsBodyId3D body, const glm::vec3& angularVelocity) = 0;
		/// The angular velocity as a world-space axis * radians per second; zero if invalid.
		virtual glm::vec3 GetAngularVelocity(PhysicsBodyId3D body) const = 0;
		/// Applies an instantaneous impulse (mass * velocity change) at the center of mass, so it
		/// adds no spin.
		virtual void ApplyImpulse(PhysicsBodyId3D body, const glm::vec3& impulse) = 0; // at center of mass
		/// Applies a force at the center of mass for the next Step only; call it every frame to
		/// keep pushing.
		virtual void ApplyForce(PhysicsBodyId3D body, const glm::vec3& force) = 0;     // at center of mass

		// --- Collision filtering ----------------------------------------------

		/// Stops contacts between two bodies (ignore = false restores them); every other pair
		/// is untouched, and scene queries still hit both. Symmetric, dropped when either body
		/// is destroyed, and a change wakes both so it holds from the next Step. A Scene owns
		/// the pairs between a kinematic child and its ancestors' bodies: it applies them on
		/// every step and withdraws them when the child leaves. A pair a constraint ignores
		/// (CollideConnected false) stays ignored until that constraint is destroyed, whatever
		/// ignore says here. No-op on an invalid/stale handle.
		virtual void IgnoreCollision(PhysicsBodyId3D a, PhysicsBodyId3D b, bool ignore = true) = 0;
		/// True if contacts between the two bodies are currently ignored (see IgnoreCollision).
		virtual bool IsCollisionIgnored(PhysicsBodyId3D a, PhysicsBodyId3D b) const = 0;

		/// Puts a body in a collision group: bodies in the same non-zero group pass through each other,
		/// unless both were given collideWithinGroup = true. Different groups, and bodies in
		/// k_NoCollisionGroup, collide as usual. Group 0 takes the body out of its group (and clears
		/// collideWithinGroup). Any value is a legal group; AllocateCollisionGroup hands out unused ones.
		/// A group costs no per-pair state and composes with IgnoreCollision and CollideConnected: either
		/// one dropping a pair drops it. Only contacts are affected; scene queries and character
		/// controllers see grouped bodies as before. Wakes the body so it holds from the next Step; call
		/// it between Steps. No-op on an invalid/stale handle.
		virtual void SetCollisionGroup(PhysicsBodyId3D body, std::uint32_t group, bool collideWithinGroup = false) = 0;
		/// The body's collision group; k_NoCollisionGroup for none or an invalid/stale handle.
		virtual std::uint32_t GetCollisionGroup(PhysicsBodyId3D body) const = 0;
		/// True if the body collides with others of its group that also opted in; false for none or an invalid/stale handle.
		virtual bool GetCollideWithinGroup(PhysicsBodyId3D body) const = 0;
		/// Returns a group id no body has used so far in this world (counting groups set by hand or
		/// through RigidBodyParams3D). Logs an error and returns k_NoCollisionGroup when the ids run out.
		virtual std::uint32_t AllocateCollisionGroup() = 0;

		// --- Constraints ------------------------------------------------------

		/// Joins two bodies (or one body and the world) with a joint and wakes both. Returns
		/// k_InvalidConstraint3D and logs an error for a stale or invalid BodyA, a stale BodyB, the
		/// same body twice, a Static/Static pair (a world pin with a Static BodyA counts), a zero or
		/// parallel axis the type reads, or limits the type rejects. Destroying either body destroys
		/// the constraint first. Unless CollideConnected is set the pair stops colliding for as long as
		/// the constraint exists, even while it is disabled.
		virtual ConstraintId3D CreateConstraint(const ConstraintParams3D& params) = 0;
		/// Removes the constraint and wakes its bodies. No-op on a stale or invalid handle.
		virtual void DestroyConstraint(ConstraintId3D constraint) = 0;
		/// True while the constraint exists: false once destroyed, once either of its bodies was destroyed, or for an invalid handle.
		virtual bool IsConstraintValid(ConstraintId3D constraint) const = 0;
		/// Switches the constraint off or on without destroying it, and wakes its bodies. No-op on a stale handle.
		virtual void SetConstraintEnabled(ConstraintId3D constraint, bool enabled) = 0;
		/// True if the constraint exists and is enabled.
		virtual bool IsConstraintEnabled(ConstraintId3D constraint) const = 0;
		/// The constraint's body: index 0 is BodyA, 1 is BodyB; k_InvalidBody3D for a world pin's second body, any other index or a stale handle.
		virtual PhysicsBodyId3D GetConstraintBody(ConstraintId3D constraint, int index) const = 0;
		/// The current hinge angle in radians, or the slider position or distance between the anchors in metres; 0 for other types and stale handles.
		virtual float GetConstraintValue(ConstraintId3D constraint) const = 0;
		/// The UserData the constraint was created with; 0 for a stale handle.
		virtual std::uint32_t GetConstraintUserData(ConstraintId3D constraint) const = 0;

		/// Sets one motor of a Hinge (axis Hinge) or SwingTwist (axis Swing or Twist) constraint. Values are read
		/// every step, so call it again to retune. Off, or MaxTorque 0, leaves the joint passive. Position mode is a
		/// spring toward the target rotation (SetMotorTargetRotation); Velocity mode drives toward the target
		/// velocity (SetMotorTargetVelocity). An axis the joint lacks is ignored with a warning; a stale handle or
		/// a negative or NaN value is ignored silently. Wakes both bodies only when the motor actually changed.
		virtual void SetMotor(ConstraintId3D constraint, MotorAxis3D axis, MotorMode3D mode, const MotorParams3D& params) = 0;
		/// The params last accepted by SetMotor for the axis; all zero for a stale handle or an axis the joint lacks.
		virtual MotorParams3D GetMotorParams(ConstraintId3D constraint, MotorAxis3D axis) const = 0;
		/// The mode last accepted by SetMotor for the axis; Off for a stale handle or an axis the joint lacks.
		virtual MotorMode3D GetMotorMode(ConstraintId3D constraint, MotorAxis3D axis) const = 0;
		/// The Position mode target: B's rotation relative to A in body space (R_B = R_A * q), the same for
		/// every motor of the joint. A Hinge reads only the angle about its axis, clamped to its limits, which
		/// equals GetConstraintValue when both bodies were created with equal rotations; a SwingTwist takes the
		/// whole rotation, clamped to its limits. A target past a limit pushes against the limit. A stale
		/// handle, a joint without motors or a non-finite value is ignored. Wakes both bodies only on a change.
		virtual void SetMotorTargetRotation(ConstraintId3D constraint, const glm::quat& rotation) = 0;
		/// The Velocity mode target in rad/s of B relative to A, in A's body space, the same for every motor of
		/// the joint. A Hinge uses the component along its axis. A SwingTwist applies it per axis (twist along
		/// its twist axis, swing across the cone axes), exact near the rest pose only. A stale handle, a joint
		/// without motors or a non-finite value is ignored. Wakes both bodies only on a change.
		virtual void SetMotorTargetVelocity(ConstraintId3D constraint, const glm::vec3& angularVelocity) = 0;
		/// The magnitude in N*m the motor applied in the last step; 0 when it is off, the bodies sleep or the
		/// handle or axis is invalid. For Swing it is the larger of the two cone axes. While both bodies sleep no
		/// solver torque is applied, so a motor holding a pose against gravity reads 0 once it sleeps.
		virtual float GetMotorTorque(ConstraintId3D constraint, MotorAxis3D axis) const = 0;

		/// Sets the thresholds above which the constraint breaks (ConstraintParams3D::BreakForce, BreakTorque); 0 for a
		/// threshold turns it off. A negative, NaN or infinite value ignores the whole call; a stale handle is ignored
		/// silently. It does not wake the bodies: a sleeping pair is checked once something wakes it.
		virtual void SetConstraintBreakLimits(ConstraintId3D constraint, float breakForce, float breakTorque) = 0;
		/// Reads the thresholds into the outputs and returns true; false (outputs unchanged) for a stale handle.
		virtual bool GetConstraintBreakLimits(ConstraintId3D constraint, float& outBreakForce, float& outBreakTorque) const = 0;
		/// Reads the reaction the constraint applied in the last step: force in N and torque in N*m, from the last
		/// collision sub-step's impulse (Jolt keeps no earlier one), so with collisionSteps above 1 a spike in an
		/// earlier sub-step goes unseen. Motor impulses are not counted; Point and Distance report a torque of 0.
		/// Returns false (outputs unchanged) for a stale handle; reads 0 while both bodies sleep, while the
		/// constraint is disabled and before the first Step.
		virtual bool GetConstraintReaction(ConstraintId3D constraint, float& outForce, float& outTorque) const = 0;
		/// Appends the constraints that broke since the last call, in the order they broke, and clears the list. A
		/// constraint breaks at the end of Step when its force or torque exceeds a non-zero threshold; it is then
		/// destroyed like DestroyConstraint. The world keeps at most 4096 undrained entries and drops the oldest.
		virtual void ConsumeBrokenConstraints(std::vector<ConstraintBreak3D>& outBroken) = 0;

		// --- Scene queries ----------------------------------------------------

		/// Casts a ray and reports the closest body it hits within maxDistance. Returns
		/// true and fills outHit on a hit; returns false (outHit unchanged) on a miss or
		/// if the world isn't live. ray.Direction is expected to be normalized. Used for
		/// picking, line-of-sight and hitscan weapons. Every query skips sensors and bodies the
		/// filter excludes; the overloads without one use QueryFilter3D's defaults.
		virtual bool RayCast(const Ray& ray, float maxDistance, RayCastHit3D& outHit, const QueryFilter3D& filter) const = 0;
		/// Same as the overload above with a default QueryFilter3D.
		bool RayCast(const Ray& ray, float maxDistance, RayCastHit3D& outHit) const { return RayCast(ray, maxDistance, outHit, QueryFilter3D()); }

		/// Sweeps a sphere of `radius` from `center` along `direction` (expected normalized)
		/// up to maxDistance and reports the closest body it hits. Returns true and fills
		/// outHit on a hit; false (outHit unchanged) on a miss. A "thick" ray cast - use it
		/// for melee reach or a projectile with volume.
		virtual bool ShapeCastSphere(const glm::vec3& center, const glm::vec3& direction, float radius, float maxDistance, RayCastHit3D& outHit,
			const QueryFilter3D& filter) const = 0;
		/// Same as the overload above with a default QueryFilter3D.
		bool ShapeCastSphere(const glm::vec3& center, const glm::vec3& direction, float radius, float maxDistance, RayCastHit3D& outHit) const
		{
			return ShapeCastSphere(center, direction, radius, maxDistance, outHit, QueryFilter3D());
		}

		/// Collects every body whose shape overlaps a sphere at `center` with `radius` into
		/// `out` (cleared first). Returns true if any were found. Cheap area query for melee
		/// AoE, triggers and proximity checks; order is unspecified.
		virtual bool OverlapSphere(const glm::vec3& center, float radius, std::vector<PhysicsBodyId3D>& out, const QueryFilter3D& filter) const = 0;
		/// Same as the overload above with a default QueryFilter3D.
		bool OverlapSphere(const glm::vec3& center, float radius, std::vector<PhysicsBodyId3D>& out) const { return OverlapSphere(center, radius, out, QueryFilter3D()); }

		// --- Sensors and user data --------------------------------------------

		/// Collects every body (not sensor) that overlaps the sensor's shape into `out` (cleared
		/// first), once each, sleeping or not. True if any were found; false for a body that
		/// isn't a sensor, or an invalid/stale handle. Character controllers aren't bodies: ask
		/// CharacterController3D::IsOverlapping, or Scene::GetSensorOverlaps for both.
		virtual bool GetSensorOverlaps(PhysicsBodyId3D sensor, std::vector<PhysicsBodyId3D>& out) const = 0;
		/// True if the body was created with RigidBodyParams3D::IsSensor; false for an invalid
		/// or stale handle.
		virtual bool IsSensor(PhysicsBodyId3D body) const = 0;

		/// RigidBodyParams3D::UserData; 0 on an invalid/stale handle. A Scene owns its bodies'.
		virtual std::uint32_t GetUserData(PhysicsBodyId3D body) const = 0;
		/// Replaces the body's UserData; no-op on an invalid/stale handle. A Scene keeps its entity
		/// there, so leave the bodies it owns alone.
		virtual void SetUserData(PhysicsBodyId3D body, std::uint32_t userData) = 0;

		// --- Character controller ---------------------------------------------

		/// Creates a kinematic character controller (Jolt CharacterVirtual) in this world.
		/// The returned controller is owned by the caller and MUST be destroyed before the
		/// Physics3D it came from (it holds the world). Returns nullptr if the world isn't
		/// live. See CharacterController3D for the per-frame drive contract.
		virtual std::unique_ptr<CharacterController3D> CreateCharacterController(const CharacterControllerParams3D& params) = 0;
	};

}
