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
	};

	// Opaque handle to a body inside a Physics3D world. No 3D-physics-backend type
	// (Jolt) ever appears in the public API; this is all the client ever holds.
	using PhysicsBodyId3D = std::uint32_t;
	inline constexpr PhysicsBodyId3D k_InvalidBody3D = 0xFFFFFFFFu;

	// Describes a rigid body to create. Box uses HalfExtents; Sphere uses Radius.
	struct RigidBodyParams3D
	{
		BodyType3D Type = BodyType3D::Static;
		ColliderShape3D Shape = ColliderShape3D::Box;

		glm::vec3 Position{ 0.0f };
		glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f }; // identity (w, x, y, z)

		glm::vec3 HalfExtents{ 0.5f }; // ColliderShape3D::Box
		float Radius = 0.5f;           // ColliderShape3D::Sphere

		float Friction = 0.5f;
		float Restitution = 0.0f; // bounciness, [0,1]

		RigidBodyParams3D() = default;
		RigidBodyParams3D(BodyType3D type, ColliderShape3D shape) : Type(type), Shape(shape) {}
	};

}
