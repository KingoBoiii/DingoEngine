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

		/// A Static box at the origin with default material.
		RigidBodyParams3D() = default;
		/// Sets the body type and collider shape; everything else keeps its default.
		RigidBodyParams3D(BodyType3D type, ColliderShape3D shape) : Type(type), Shape(shape) {}
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
