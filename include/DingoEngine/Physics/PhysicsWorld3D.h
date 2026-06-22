#pragma once

#include "DingoEngine/Physics/PhysicsTypes3D.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>

namespace Dingo
{

	namespace Internal { struct Physics3DData; }

	struct PhysicsWorld3DParams
	{
		glm::vec3 Gravity{ 0.0f, -9.81f, 0.0f };
		std::uint32_t MaxBodies = 65536;
	};

	// A standalone 3D rigid-body world backed by Jolt Physics. Jolt is fully hidden
	// behind an opaque pointer — no Jolt type appears in this header, mirroring how
	// EnTT/Box2D are confined to the engine's src/ tree.
	//
	// Unlike the 2D Scene physics, this is NOT wired into the ECS: you drive it
	// directly — create bodies, Step() each frame, then read each body's transform
	// (GetTransform / GetPosition / GetRotation) to render it however you like.
	class PhysicsWorld3D
	{
	public:
		static PhysicsWorld3D* Create(const PhysicsWorld3DParams& params = {});
		void Destroy();

		// --- Bodies ---
		PhysicsBodyId3D CreateBody(const RigidBodyParams3D& params);
		void DestroyBody(PhysicsBodyId3D body);
		bool IsBodyValid(PhysicsBodyId3D body) const;

		// --- Simulation ---
		// Advances the world by deltaTime. Use more collision steps for large steps
		// (Jolt recommends one step per 1/60s, rounded up).
		void Step(float deltaTime, int collisionSteps = 1);

		void SetGravity(const glm::vec3& gravity);
		glm::vec3 GetGravity() const;

		// --- Per-body state (no-ops / identity if the body isn't valid) ---
		glm::vec3 GetPosition(PhysicsBodyId3D body) const;
		glm::quat GetRotation(PhysicsBodyId3D body) const;
		glm::mat4 GetTransform(PhysicsBodyId3D body) const; // translation * rotation (no scale)

		void SetLinearVelocity(PhysicsBodyId3D body, const glm::vec3& velocity);
		glm::vec3 GetLinearVelocity(PhysicsBodyId3D body) const;
		void ApplyImpulse(PhysicsBodyId3D body, const glm::vec3& impulse); // at center of mass
		void ApplyForce(PhysicsBodyId3D body, const glm::vec3& force);     // at center of mass

	private:
		PhysicsWorld3D() = default;
		~PhysicsWorld3D() = default;
		PhysicsWorld3D(const PhysicsWorld3D&) = delete;
		PhysicsWorld3D& operator=(const PhysicsWorld3D&) = delete;

	private:
		Internal::Physics3DData* m_Data = nullptr;
	};

}
