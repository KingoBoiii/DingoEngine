#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <vector>

namespace Dingo
{

	/// How a body participates in the simulation.
	enum class BodyType3D
	{
		Static,    ///< Never moves (ground, walls).
		Dynamic,   ///< Fully simulated - gravity, forces, collisions.
		Kinematic, ///< Moved only by velocity you set; ignores forces/gravity.
	};

	/// The collision shape of a body; RigidBodyParams3D says which fields each one reads.
	enum class ColliderShape3D
	{
		Box,
		Sphere,
		Capsule,
		Mesh,       ///< The mesh's triangles as-is; Static and Kinematic bodies only.
		ConvexHull, ///< The convex hull of the mesh's vertices; any body type.
	};

	class Mesh;

	/// Opaque handle to a body inside a Physics3D world. No 3D-physics-backend type
	/// (Jolt) ever appears in the public API; this is all the client ever holds.
	/// A handle is only meaningful for the world that made it; k_InvalidBody3D is "none" (not 0).
	using PhysicsBodyId3D = std::uint32_t;
	/// The "no body" handle, 0xFFFFFFFF. Not 0: 0 is a valid body.
	inline constexpr PhysicsBodyId3D k_InvalidBody3D = 0xFFFFFFFFu;

	/// The collision group of a body that is in none (Physics3D::SetCollisionGroup).
	inline constexpr std::uint32_t k_NoCollisionGroup = 0;

	/// Describes a rigid body to create. Box uses HalfExtents; Sphere uses Radius;
	/// Capsule uses Radius plus HalfHeight (half the length of the cylinder section
	/// between the two hemispherical caps - the total capsule height is
	/// 2*(HalfHeight + Radius)), aligned to the body's local +Y axis.
	///
	/// Mesh and ConvexHull read Mesh's vertex positions and indices, scaled per axis by
	/// MeshScale. The shape is copied out at creation, so the Mesh may be freed once
	/// CreateBody returns. A Dynamic body asking for Mesh gets the ConvexHull instead
	/// (with a warning): a triangle soup has no volume to derive mass from, and Jolt
	/// does not collide mesh against mesh.
	struct RigidBodyParams3D
	{
		BodyType3D Type = BodyType3D::Static;               ///< Static by default.
		/// Which of the shape fields below are read.
		ColliderShape3D Shape = ColliderShape3D::Box;

		glm::vec3 Position{ 0.0f };                         ///< World position of the body origin.
		/// World rotation, identity (w, x, y, z) by default.
		glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };

		/// Box: half-size along each local axis (ColliderShape3D::Box).
		glm::vec3 HalfExtents{ 0.5f };
		/// Sphere / Capsule: radius (ColliderShape3D::Sphere / Capsule).
		float Radius = 0.5f;
		/// Capsule: half the cylinder section (ColliderShape3D::Capsule).
		float HalfHeight = 0.5f;

		/// Mesh / ConvexHull: the source mesh, required; CreateBody fails without it.
		const Dingo::Mesh* Mesh = nullptr;
		/// Mesh / ConvexHull: per-axis scale; no axis may be zero (CreateBody fails).
		glm::vec3 MeshScale{ 1.0f };

		/// Coulomb friction coefficient, 0 for ice and 1 for rubber-like grip.
		float Friction = 0.5f;
		float Restitution = 0.0f;                           ///< Bounciness, [0,1].
		/// Dynamic bodies: the mass in kg, the inertia scaled from the shape to match. 0 (the default),
		/// a negative or a non-finite value takes the shape's own mass at 1000 kg per cubic metre.
		float Mass = 0.0f;

		/// Sweeps the body along its motion each step so it cannot pass through thin
		/// geometry (a Mesh collider's triangles) when it moves further than its own radius
		/// in one step: fast projectiles, long falls, low frame rates. Costs a cast only on
		/// steps where the body is that fast. Ignored for Static bodies.
		bool ContinuousCollision = false;

		/// A trigger volume: nothing collides with it, character controllers walk through it, and
		/// scene queries skip it unless their QueryFilter3D includes sensors.
		/// Physics3D::GetSensorOverlaps lists the bodies inside it. A Mesh sensor doesn't find
		/// Mesh colliders (Jolt can't collide two triangle meshes).
		bool IsSensor = false;
		/// The query layers the body is in, one bit each; a QueryFilter3D only hits bodies that
		/// share a bit with its Layers. Not a contact filter: bodies on any layers still collide.
		std::uint32_t QueryLayers = 1u;
		/// Free for the caller (Physics3D::GetUserData). A Scene keeps its entity here.
		std::uint32_t UserData = 0;

		/// The collision group the body starts in (Physics3D::SetCollisionGroup); k_NoCollisionGroup is none.
		std::uint32_t CollisionGroup = k_NoCollisionGroup;
		/// Whether the body collides with others of its group that also set this (Physics3D::SetCollisionGroup).
		bool CollideWithinGroup = false;

		/// A Static box at the origin with default material.
		RigidBodyParams3D() = default;
		/// Sets the body type and collider shape; everything else keeps its default.
		RigidBodyParams3D(BodyType3D type, ColliderShape3D shape) : Type(type), Shape(shape) {}
	};

	/// Opaque handle to a constraint (joint) inside a Physics3D world. Never reused within a world, so a
	/// handle to a destroyed constraint stays invalid. k_InvalidConstraint3D is "none" (not 0).
	using ConstraintId3D = std::uint32_t;
	/// The "no constraint" handle, 0xFFFFFFFF. Not 0: 0 is a valid constraint.
	inline constexpr ConstraintId3D k_InvalidConstraint3D = 0xFFFFFFFFu;

	/// The kind of joint a ConstraintParams3D builds; the params say which fields each one reads.
	enum class ConstraintType3D
	{
		Fixed,      ///< Welds the two anchors and orientations together; no relative motion.
		Point,      ///< Joins the two anchors; the bodies rotate freely about it (ball and socket).
		Hinge,      ///< Rotation about one axis only, optionally limited to MinLimit..MaxLimit radians.
		SwingTwist, ///< A ball joint limited to a cone (SwingLimitY, SwingLimitZ) and a twist range (TwistMin..TwistMax).
		Distance,   ///< Keeps the two anchors between MinLimit and MaxLimit metres apart; Min == Max is a rigid rod.
		Slider,     ///< Translation along one axis only, optionally limited to MinLimit..MaxLimit metres.
	};

	/// Describes a constraint to create (Physics3D::CreateConstraint). Anchors and axes are in each body's
	/// local space; with BodyB invalid, BodyA is pinned to the world and AnchorB, AxisB and NormalAxisB are
	/// world values. Fields a type does not read are ignored. Axes need not be normalised, but an axis the
	/// type reads may not be zero, and NormalAxis may not be parallel to Axis.
	struct ConstraintParams3D
	{
		ConstraintType3D Type = ConstraintType3D::Fixed; ///< Which joint to build.

		/// The first body. Required; it may not be Static when BodyB is invalid or Static.
		PhysicsBodyId3D BodyA = k_InvalidBody3D;
		/// The second body, or k_InvalidBody3D to pin BodyA to the world.
		PhysicsBodyId3D BodyB = k_InvalidBody3D;

		glm::vec3 AnchorA{ 0.0f }; ///< The joint point in BodyA's local space (Distance, Fixed, Point, Hinge, SwingTwist, Slider).
		glm::vec3 AnchorB{ 0.0f }; ///< The joint point in BodyB's local space, or a world point when BodyB is invalid.

		/// Hinge axis, SwingTwist twist axis or Slider axis in BodyA's local space; Fixed uses it as its Y axis.
		glm::vec3 AxisA{ 0.0f, 1.0f, 0.0f };
		/// The same axis in BodyB's local space, or in world space when BodyB is invalid.
		glm::vec3 AxisB{ 0.0f, 1.0f, 0.0f };
		/// A direction perpendicular to AxisA in BodyA's local space (Fixed, Hinge, SwingTwist plane axis, Slider); made perpendicular for you.
		glm::vec3 NormalAxisA{ 1.0f, 0.0f, 0.0f };
		/// The same direction in BodyB's local space, or in world space when BodyB is invalid.
		glm::vec3 NormalAxisB{ 1.0f, 0.0f, 0.0f };

		/// Lower limit: Hinge radians in [-pi, 0], Slider and Distance metres. For Hinge and Slider, MinLimit >= MaxLimit
		/// means unlimited. For Distance, Min == Max is a rigid rod, Min < Max a rope that may slack and stretch within
		/// the range, and Min > Max or a negative value fails.
		float MinLimit = 0.0f;
		float MaxLimit = 0.0f; ///< Upper limit, see MinLimit; Hinge radians in [0, pi].
		/// SwingTwist: the half cone angle in radians, in [0, pi], of the swing about the plane axis (NormalAxis).
		float SwingLimitY = 0.0f;
		float SwingLimitZ = 0.0f; ///< SwingTwist: the half cone angle in radians, in [0, pi], of the swing about the axis perpendicular to both.
		float TwistMin = 0.0f;    ///< SwingTwist: the lowest twist about the twist axis in radians, in [-pi, pi].
		float TwistMax = 0.0f;    ///< SwingTwist: the highest twist, in [TwistMin, pi].

		/// False (the default) stops the two bodies colliding while the constraint exists, like
		/// Physics3D::IgnoreCollision; true lets them collide.
		bool CollideConnected = false;
		/// Free for the caller (Physics3D::GetConstraintUserData).
		std::uint32_t UserData = 0;

		/// Newtons of reaction force above which the constraint breaks and is destroyed (Physics3D::ConsumeBrokenConstraints).
		/// 0 never breaks. A negative, NaN or infinite value is treated as 0 with a warning.
		float BreakForce = 0.0f;
		/// Newton-metres of reaction torque above which the constraint breaks. 0 never breaks. Point and Distance have
		/// no torque, so it has no effect on them. A negative, NaN or infinite value is treated as 0 with a warning.
		float BreakTorque = 0.0f;

		/// A Fixed constraint with no bodies; CreateConstraint fails until BodyA is set.
		ConstraintParams3D() = default;
		/// Sets the type and the two bodies (b invalid pins a to the world); everything else keeps its default.
		ConstraintParams3D(ConstraintType3D type, PhysicsBodyId3D a, PhysicsBodyId3D b) : Type(type), BodyA(a), BodyB(b) {}
	};

	/// One constraint that broke (Physics3D::ConsumeBrokenConstraints). A snapshot: the handle is already stale and
	/// the bodies may be destroyed by the time it is read.
	struct ConstraintBreak3D
	{
		ConstraintId3D Constraint = k_InvalidConstraint3D; ///< The destroyed constraint's handle.
		PhysicsBodyId3D BodyA = k_InvalidBody3D;           ///< The constraint's first body.
		PhysicsBodyId3D BodyB = k_InvalidBody3D;           ///< The second body, or k_InvalidBody3D for a world pin.
		std::uint32_t UserData = 0;                        ///< The UserData the constraint was created with.
		float Force = 0.0f;                                ///< The reaction force in N at the break.
		float Torque = 0.0f;                               ///< The reaction torque in N*m at the break.
	};

	/// Which rotation axis of a constraint a motor drives (Physics3D::SetMotor).
	enum class MotorAxis3D
	{
		Hinge, ///< The one axis of a Hinge.
		Swing, ///< Both cone axes of a SwingTwist, driven together.
		Twist, ///< The twist axis of a SwingTwist.
	};

	/// How a motor drives its axis.
	enum class MotorMode3D
	{
		Off,      ///< No drive: the joint is passive.
		Velocity, ///< Drives the relative angular velocity toward the target velocity, within MaxTorque.
		Position, ///< Drives the relative rotation toward the target rotation like a spring, within MaxTorque.
	};

	/// The strength of one motor (Physics3D::SetMotor). A negative or NaN field makes SetMotor ignore the call.
	struct MotorParams3D
	{
		float Stiffness = 0.0f; ///< Position mode spring constant, N*m per radian of error.
		float Damping = 0.0f;   ///< N*m per rad/s of velocity error; in Velocity mode above 0 the drive is a damper toward the target velocity.
		float MaxTorque = 0.0f; ///< Cap on the torque in N*m; 0 turns the motor off whatever the mode.

		/// Compares all three fields.
		bool operator==(const MotorParams3D&) const = default;
	};

	/// A single hit from a Physics3D scene query (ray cast / shape cast). Body is the
	/// hit body's handle; a query that misses returns false and leaves the hit as it
	/// was, so a default-constructed one still reads k_InvalidBody3D. Point is the
	/// world-space contact point; Normal is the world-space surface normal at the hit;
	/// Fraction is the hit distance along the query as a fraction of its maximum
	/// distance, i.e. Point == origin + dir * Fraction * maxDistance.
	struct RayCastHit3D
	{
		PhysicsBodyId3D Body = k_InvalidBody3D; ///< The body that was hit.
		glm::vec3 Point{ 0.0f };                ///< World-space contact point.
		glm::vec3 Normal{ 0.0f };               ///< World-space surface normal at the hit.
		/// Hit distance as a fraction of the query's maxDistance, [0,1].
		float Fraction = 0.0f;
	};

	/// Narrows a scene query (RayCast, ShapeCastSphere, OverlapSphere). The default hits every
	/// body that isn't a sensor.
	struct QueryFilter3D
	{
		/// Hits only bodies whose QueryLayers share a bit with it.
		std::uint32_t Layers = 0xFFFFFFFFu;
		std::vector<PhysicsBodyId3D> IgnoredBodies; ///< Bodies the query passes through.
		bool IncludeSensors = false;        ///< True to let the query hit sensor bodies too.

		/// Sets Layers and returns *this, for chaining.
		QueryFilter3D& SetLayers(std::uint32_t layers) { Layers = layers; return *this; }
		/// Adds body to IgnoredBodies and returns *this; call it once per body.
		QueryFilter3D& Ignore(PhysicsBodyId3D body) { IgnoredBodies.push_back(body); return *this; }
		/// Sets IncludeSensors and returns *this, for chaining.
		QueryFilter3D& SetIncludeSensors(bool include) { IncludeSensors = include; return *this; }
	};

}
