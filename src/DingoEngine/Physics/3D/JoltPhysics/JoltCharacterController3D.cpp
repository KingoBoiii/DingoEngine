#include "depch.h"
#include "DingoEngine/Physics/3D/JoltPhysics/JoltCharacterController3D.h"

#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

namespace Dingo::Internal
{

	namespace
	{
		class IgnoredBodyFilter final : public JPH::BodyFilter
		{
		public:
			explicit IgnoredBodyFilter(const std::vector<JPH::BodyID>& bodies) : m_Bodies(bodies) {}

			virtual bool ShouldCollide(const JPH::BodyID& inBodyID) const override
			{
				return std::find(m_Bodies.begin(), m_Bodies.end(), inBodyID) == m_Bodies.end();
			}

		private:
			const std::vector<JPH::BodyID>& m_Bodies;
		};
	}

	JoltCharacterController3D::JoltCharacterController3D(JoltPhysics3DData* world, const CharacterControllerParams3D& params)
		: m_World(world), m_Up(ToJolt(glm::normalize(params.Up))), m_StepHeight(params.StepHeight)
	{
		// Capsule cylinder section = full height minus the two hemispherical caps. Clamp so
		// a too-short Height still yields a valid (degenerate-free) capsule.
		const float radius = params.Radius;
		const float halfCylinder = std::max(0.01f, params.Height * 0.5f - radius);

		// The shape must sit with its bottom at the character's origin (feet), so lift the
		// centered capsule up by (halfCylinder + radius) — this is Jolt's documented setup.
		JPH::RefConst<JPH::Shape> capsule = new JPH::CapsuleShape(halfCylinder, radius);
		JPH::RefConst<JPH::Shape> shape = JPH::RotatedTranslatedShapeSettings(
			JPH::Vec3(0.0f, halfCylinder + radius, 0.0f), JPH::Quat::sIdentity(), capsule)
			.Create().Get();

		JPH::CharacterVirtualSettings settings;
		settings.mShape = shape;
		settings.mUp = m_Up;
		settings.mMass = params.Mass;
		settings.mMaxSlopeAngle = glm::radians(params.MaxSlopeAngle);
		// Supporting-volume plane: contacts below the character's feet (offset by the
		// capsule radius) count as ground. Mirrors the Jolt CharacterVirtual sample.
		settings.mSupportingVolume = JPH::Plane(m_Up, -radius);

		m_Character = new JPH::CharacterVirtual(&settings, ToJolt(params.Position), ToJolt(params.Rotation), &m_World->PhysicsSystem);
		if (params.CollideWithCharacters)
		{
			m_World->CharacterCollision.Add(m_Character.GetPtr());
			m_Character->SetCharacterVsCharacterCollision(&m_World->CharacterCollision);
		}
	}

	JoltCharacterController3D::~JoltCharacterController3D()
	{
		// JPH::Ref releases the CharacterVirtual; it is not registered with the
		// PhysicsSystem, only with the world's character set.
		m_World->CharacterCollision.Remove(m_Character.GetPtr());
		m_Character = nullptr;
	}

	glm::vec3 JoltCharacterController3D::GetPosition() const
	{
		return ToGlm(m_Character->GetPosition());
	}

	void JoltCharacterController3D::SetPosition(const glm::vec3& position)
	{
		m_Character->SetPosition(ToJolt(position));
	}

	glm::quat JoltCharacterController3D::GetRotation() const
	{
		return ToGlm(m_Character->GetRotation());
	}

	void JoltCharacterController3D::SetRotation(const glm::quat& rotation)
	{
		m_Character->SetRotation(ToJolt(rotation));
	}

	glm::vec3 JoltCharacterController3D::GetLinearVelocity() const
	{
		return ToGlm(m_Character->GetLinearVelocity());
	}

	void JoltCharacterController3D::SetLinearVelocity(const glm::vec3& velocity)
	{
		m_Character->SetLinearVelocity(ToJolt(velocity));
	}

	void JoltCharacterController3D::Update(float deltaTime)
	{
		if (deltaTime <= 0.0f)
			return;

		// ExtendedUpdate = MoveShape + StickToFloor + WalkStairs. The caller has already
		// folded gravity into the velocity (see CharacterController3D docs); the gravity
		// passed here is only used to push down bodies the character stands on.
		JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
		updateSettings.mWalkStairsStepUp = m_Up * m_StepHeight;

		const JPH::Vec3 gravity = m_World->PhysicsSystem.GetGravity();

		// A destroyed body's id comes back once its slot's 8-bit sequence number wraps.
		if (!m_IgnoredBodies.empty())
		{
			const JPH::BodyInterface& bodies = m_World->PhysicsSystem.GetBodyInterface();
			std::erase_if(m_IgnoredBodies, [&bodies](const JPH::BodyID& id) { return !bodies.IsAdded(id); });
		}

		// Any other CharacterVirtual call that queries the world (RefreshContacts, SetShape) needs it too.
		const IgnoredBodyFilter bodyFilter(m_IgnoredBodies);
		m_Character->ExtendedUpdate(deltaTime, gravity, updateSettings,
			m_World->PhysicsSystem.GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
			m_World->PhysicsSystem.GetDefaultLayerFilter(Layers::MOVING),
			bodyFilter, JPH::ShapeFilter{}, m_World->TempAllocator);
	}

	void JoltCharacterController3D::IgnoreBody(PhysicsBodyId3D body, bool ignore)
	{
		if (body == k_InvalidBody3D)
			return;

		const JPH::BodyID id(body);
		auto it = std::find(m_IgnoredBodies.begin(), m_IgnoredBodies.end(), id);
		if (!ignore)
		{
			if (it != m_IgnoredBodies.end())
				m_IgnoredBodies.erase(it);
			return;
		}

		if (it == m_IgnoredBodies.end() && m_World->PhysicsSystem.GetBodyInterface().IsAdded(id))
			m_IgnoredBodies.push_back(id);
	}

	bool JoltCharacterController3D::IsBodyIgnored(PhysicsBodyId3D body) const
	{
		return body != k_InvalidBody3D
			&& std::find(m_IgnoredBodies.begin(), m_IgnoredBodies.end(), JPH::BodyID(body)) != m_IgnoredBodies.end();
	}

	bool JoltCharacterController3D::IsGrounded() const
	{
		return m_Character->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
	}

	glm::vec3 JoltCharacterController3D::GetGroundNormal() const
	{
		if (!m_Character->IsSupported())
			return glm::vec3(0.0f);
		return ToGlm(m_Character->GetGroundNormal());
	}

	glm::vec3 JoltCharacterController3D::GetGroundVelocity() const
	{
		if (!m_Character->IsSupported())
			return glm::vec3(0.0f);
		return ToGlm(m_Character->GetGroundVelocity());
	}

}
