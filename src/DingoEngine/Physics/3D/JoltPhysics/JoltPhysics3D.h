#pragma once

#include "DingoEngine/Physics/3D/Physics3D.h"

namespace Dingo
{

	namespace Internal { struct JoltPhysics3DData; }

	// Jolt Physics implementation of the Physics3D interface. Jolt is fully hidden
	// behind an opaque pointer (Internal::JoltPhysics3DData) — no Jolt type appears in
	// this header; every JPH::* type and call is confined to JoltPhysics3D.cpp and the
	// engine-internal JoltPhysics3DData.h. This is the 3D counterpart to Box2DPhysics2D.
	class JoltPhysics3D final : public Physics3D
	{
	public:
		JoltPhysics3D() = default;
		~JoltPhysics3D() override;

		void Initialize(const Physics3DParams& params) override;
		void Shutdown() override;
		bool IsValid() const override;

		PhysicsBodyId3D CreateBody(const RigidBodyParams3D& params) override;
		void DestroyBody(PhysicsBodyId3D body) override;
		bool IsBodyValid(PhysicsBodyId3D body) const override;

		void Step(float deltaTime, int collisionSteps) override;

		void SetGravity(const glm::vec3& gravity) override;
		glm::vec3 GetGravity() const override;

		glm::vec3 GetPosition(PhysicsBodyId3D body) const override;
		glm::quat GetRotation(PhysicsBodyId3D body) const override;
		glm::mat4 GetTransform(PhysicsBodyId3D body) const override;

		void SetPosition(PhysicsBodyId3D body, const glm::vec3& position) override;
		void SetRotation(PhysicsBodyId3D body, const glm::quat& rotation) override;
		void SetPositionAndRotation(PhysicsBodyId3D body, const glm::vec3& position, const glm::quat& rotation) override;
		void MoveKinematic(PhysicsBodyId3D body, const glm::vec3& targetPosition, const glm::quat& targetRotation, float deltaTime) override;

		void SetLinearVelocity(PhysicsBodyId3D body, const glm::vec3& velocity) override;
		glm::vec3 GetLinearVelocity(PhysicsBodyId3D body) const override;
		void SetAngularVelocity(PhysicsBodyId3D body, const glm::vec3& angularVelocity) override;
		glm::vec3 GetAngularVelocity(PhysicsBodyId3D body) const override;
		void ApplyImpulse(PhysicsBodyId3D body, const glm::vec3& impulse) override;
		void ApplyForce(PhysicsBodyId3D body, const glm::vec3& force) override;

		void IgnoreCollision(PhysicsBodyId3D a, PhysicsBodyId3D b, bool ignore) override;
		bool IsCollisionIgnored(PhysicsBodyId3D a, PhysicsBodyId3D b) const override;

		using Physics3D::RayCast;
		using Physics3D::ShapeCastSphere;
		using Physics3D::OverlapSphere;
		bool RayCast(const Ray& ray, float maxDistance, RayCastHit3D& outHit, const QueryFilter3D& filter) const override;
		bool ShapeCastSphere(const glm::vec3& center, const glm::vec3& direction, float radius, float maxDistance, RayCastHit3D& outHit,
			const QueryFilter3D& filter) const override;
		bool OverlapSphere(const glm::vec3& center, float radius, std::vector<PhysicsBodyId3D>& out, const QueryFilter3D& filter) const override;

		bool GetSensorOverlaps(PhysicsBodyId3D sensor, std::vector<PhysicsBodyId3D>& out) const override;
		bool IsSensor(PhysicsBodyId3D body) const override;
		std::uint32_t GetUserData(PhysicsBodyId3D body) const override;
		void SetUserData(PhysicsBodyId3D body, std::uint32_t userData) override;

		ConstraintId3D CreateConstraint(const ConstraintParams3D& params) override;
		void DestroyConstraint(ConstraintId3D constraint) override;
		bool IsConstraintValid(ConstraintId3D constraint) const override;
		void SetConstraintEnabled(ConstraintId3D constraint, bool enabled) override;
		bool IsConstraintEnabled(ConstraintId3D constraint) const override;
		PhysicsBodyId3D GetConstraintBody(ConstraintId3D constraint, int index) const override;
		float GetConstraintValue(ConstraintId3D constraint) const override;
		std::uint32_t GetConstraintUserData(ConstraintId3D constraint) const override;
		void SetMotor(ConstraintId3D constraint, MotorAxis3D axis, MotorMode3D mode, const MotorParams3D& params) override;
		MotorParams3D GetMotorParams(ConstraintId3D constraint, MotorAxis3D axis) const override;
		MotorMode3D GetMotorMode(ConstraintId3D constraint, MotorAxis3D axis) const override;
		void SetMotorTargetRotation(ConstraintId3D constraint, const glm::quat& rotation) override;
		void SetMotorTargetVelocity(ConstraintId3D constraint, const glm::vec3& angularVelocity) override;
		float GetMotorTorque(ConstraintId3D constraint, MotorAxis3D axis) const override;

		std::unique_ptr<CharacterController3D> CreateCharacterController(const CharacterControllerParams3D& params) override;

	private:
		static void DestroyConstraintInternal(Internal::JoltPhysics3DData& data, ConstraintId3D id);

		Internal::JoltPhysics3DData* m_Data = nullptr;
	};

}
