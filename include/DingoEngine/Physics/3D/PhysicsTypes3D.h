#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>

namespace Dingo
{

	enum class BodyType3D
	{
		Static,    // never moves (ground, walls)
		Dynamic,   // fully simulated — gravity, forces, collisions
		Kinematic, // moved only by velocity you set; ignores forces/gravity
	};

	enum class ColliderShape3D
	{
		Box,
		Sphere,
		Capsule,
		Mesh,       // the mesh's triangles as-is; Static and Kinematic bodies only
		ConvexHull, // the convex hull of the mesh's vertices; any body type
	};

	class Mesh;

	// Opaque handle to a body inside a Physics3D world. No 3D-physics-backend type
	// (Jolt) ever appears in the public API; this is all the client ever holds.
	using PhysicsBodyId3D = std::uint32_t;
	inline constexpr PhysicsBodyId3D k_InvalidBody3D = 0xFFFFFFFFu;

	// Describes a rigid body to create. Box uses HalfExtents; Sphere uses Radius;
	// Capsule uses Radius plus HalfHeight (half the length of the cylinder section
	// between the two hemispherical caps — the total capsule height is
	// 2*(HalfHeight + Radius)), aligned to the body's local +Y axis.
	//
	// Mesh and ConvexHull read Mesh's vertex positions and indices, scaled per axis by
	// MeshScale. The shape is copied out at creation, so the Mesh may be freed once
	// CreateBody returns. A Dynamic body asking for Mesh gets the ConvexHull instead
	// (with a warning): a triangle soup has no volume to derive mass from, and Jolt
	// does not collide mesh against mesh.
	struct RigidBodyParams3D
	{
		BodyType3D Type = BodyType3D::Static;
		ColliderShape3D Shape = ColliderShape3D::Box;

		glm::vec3 Position{ 0.0f };
		glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f }; // identity (w, x, y, z)

		glm::vec3 HalfExtents{ 0.5f }; // ColliderShape3D::Box
		float Radius = 0.5f;           // ColliderShape3D::Sphere / Capsule
		float HalfHeight = 0.5f;       // ColliderShape3D::Capsule (half the cylinder section)

		const Dingo::Mesh* Mesh = nullptr; // ColliderShape3D::Mesh / ConvexHull
		glm::vec3 MeshScale{ 1.0f };       // ColliderShape3D::Mesh / ConvexHull; no axis may be zero

		float Friction = 0.5f;
		float Restitution = 0.0f; // bounciness, [0,1]

		// Sweeps the body along its motion each step so it cannot pass through thin
		// geometry (a Mesh collider's triangles) when it moves further than its own radius
		// in one step: fast projectiles, long falls, low frame rates. Costs a cast only on
		// steps where the body is that fast. Ignored for Static bodies.
		bool ContinuousCollision = false;

		RigidBodyParams3D() = default;
		RigidBodyParams3D(BodyType3D type, ColliderShape3D shape) : Type(type), Shape(shape) {}
	};

	// A single hit from a Physics3D scene query (ray cast / shape cast). Body is the
	// hit body's handle (k_InvalidBody3D if the query missed and the caller ignored
	// the return value); Point is the world-space contact point; Normal is the
	// world-space surface normal at the hit; Fraction is the hit distance along the
	// query as a fraction of its maximum distance, i.e. Point == origin + dir * Fraction * maxDistance.
	struct RayCastHit3D
	{
		PhysicsBodyId3D Body = k_InvalidBody3D;
		glm::vec3 Point{ 0.0f };
		glm::vec3 Normal{ 0.0f };
		float Fraction = 0.0f;
	};

}
