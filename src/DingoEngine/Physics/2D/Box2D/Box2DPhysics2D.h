#pragma once

#include "DingoEngine/Physics/2D/Physics2D.h"

#include <box2d/box2d.h>

namespace Dingo
{

	// Box2D (v3) implementation of the Physics2D interface. This is the ONLY place
	// box2d.h is included across the whole engine — every b2* type and call is
	// confined here, behind the backend-agnostic Physics2D interface. Opaque body
	// and shape handles are b2BodyId / b2ShapeId packed into 64 bits.
	class Box2DPhysics2D final : public Physics2D
	{
	public:
		Box2DPhysics2D() = default;
		~Box2DPhysics2D() override;

		void Initialize(const glm::vec2& gravity) override;
		void Shutdown() override;
		bool IsValid() const override;

		void Step(float deltaTime, int subStepCount) override;

		void SetGravity(const glm::vec2& gravity) override;
		glm::vec2 GetGravity() const override;

		PhysicsBodyId2D CreateBody(const RigidBodyParams2D& params) override;
		void DestroyBody(PhysicsBodyId2D body) override;
		bool IsBodyValid(PhysicsBodyId2D body) const override;

		PhysicsShapeId2D AddBoxShape(PhysicsBodyId2D body, const BoxShapeParams2D& params) override;
		PhysicsShapeId2D AddCircleShape(PhysicsBodyId2D body, const CircleShapeParams2D& params) override;

		glm::vec2 GetPosition(PhysicsBodyId2D body) const override;
		float GetAngle(PhysicsBodyId2D body) const override;

		void SetLinearVelocity(PhysicsBodyId2D body, const glm::vec2& velocity) override;
		glm::vec2 GetLinearVelocity(PhysicsBodyId2D body) const override;
		void ApplyLinearImpulse(PhysicsBodyId2D body, const glm::vec2& impulse, const glm::vec2& worldPoint, bool wake) override;
		void ApplyLinearImpulseToCenter(PhysicsBodyId2D body, const glm::vec2& impulse, bool wake) override;
		void ApplyForceToCenter(PhysicsBodyId2D body, const glm::vec2& force, bool wake) override;

	private:
		static b2BodyType ToBox2DBodyType(BodyType2D type);

	private:
		b2WorldId m_World = b2_nullWorldId;
	};

}
