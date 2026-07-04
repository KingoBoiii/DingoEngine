#pragma once

// Engine-internal: lives under src/, never shipped. Alongside JoltPhysics3D.cpp and
// JoltPhysics3DData.h this is one of the few places Jolt is named. The public
// CharacterController3D interface stays Jolt-free.

#include "DingoEngine/Physics/3D/CharacterController3D.h"
#include "DingoEngine/Physics/3D/JoltPhysics/JoltPhysics3DData.h"

#include <Jolt/Physics/Character/CharacterVirtual.h>

namespace Dingo::Internal
{

	// Jolt CharacterVirtual-backed character controller. Owns the CharacterVirtual and
	// borrows the JoltPhysics3DData (the world) it was created from — so it must be
	// destroyed before that world, which the Physics3D lifetime contract guarantees.
	class JoltCharacterController3D final : public CharacterController3D
	{
	public:
		JoltCharacterController3D(JoltPhysics3DData* world, const CharacterControllerParams3D& params);
		~JoltCharacterController3D() override;

		glm::vec3 GetPosition() const override;
		void SetPosition(const glm::vec3& position) override;

		glm::quat GetRotation() const override;
		void SetRotation(const glm::quat& rotation) override;

		glm::vec3 GetLinearVelocity() const override;
		void SetLinearVelocity(const glm::vec3& velocity) override;

		void Update(float deltaTime) override;

		bool IsGrounded() const override;
		glm::vec3 GetGroundNormal() const override;
		glm::vec3 GetGroundVelocity() const override;

	private:
		JoltPhysics3DData* m_World = nullptr;
		JPH::Ref<JPH::CharacterVirtual> m_Character;
		JPH::Vec3 m_Up = JPH::Vec3::sAxisY();
		float m_StepHeight = 0.3f;
	};

}
