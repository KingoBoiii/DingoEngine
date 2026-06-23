#include "depch.h"
#include "DingoEngine/Physics/3D/JoltPhysics/JoltPhysics3D.h"

// All Jolt usage is confined to this .cpp (+ the engine-internal JoltPhysics3DData.h).
#include "DingoEngine/Physics/3D/JoltPhysics/JoltPhysics3DData.h"

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdarg>
#include <cstdio>

JPH_SUPPRESS_WARNINGS

namespace Dingo
{

	namespace
	{
		// Route Jolt's internal trace/assert output to the engine log so misuse is
		// visible instead of silently aborting.
		void JoltTrace(const char* format, ...)
		{
			va_list args;
			va_start(args, format);
			char buffer[1024];
			vsnprintf(buffer, sizeof(buffer), format, args);
			va_end(args);
			DE_CORE_WARN("[Jolt] {}", buffer);
		}

#ifdef JPH_ENABLE_ASSERTS
		bool JoltAssertFailed(const char* expression, const char* message, const char* file, JPH::uint line)
		{
			DE_CORE_ERROR("[Jolt] Assert failed: {}:{}: ({}) {}", file, line, expression, message != nullptr ? message : "");
			std::fprintf(stderr, "[Jolt] Assert failed: %s:%u: (%s) %s\n", file, line, expression, message != nullptr ? message : "");
			return false; // don't breakpoint — we've logged it
		}
#endif

		JPH::EMotionType ToMotionType(BodyType3D type)
		{
			switch (type)
			{
				case BodyType3D::Static:    return JPH::EMotionType::Static;
				case BodyType3D::Dynamic:   return JPH::EMotionType::Dynamic;
				case BodyType3D::Kinematic: return JPH::EMotionType::Kinematic;
			}
			return JPH::EMotionType::Static;
		}

		inline JPH::Vec3 ToJolt(const glm::vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }
		inline glm::vec3 ToGlm(JPH::Vec3Arg v) { return { v.GetX(), v.GetY(), v.GetZ() }; }
		inline JPH::BodyID ToBodyId(PhysicsBodyId3D body) { return JPH::BodyID(body); }

		// Jolt's allocator/factory/type registration is process-global; ref-count it
		// across worlds so it's set up on the first world and torn down with the last.
		int s_WorldCount = 0;

		void AcquireJolt()
		{
			if (s_WorldCount++ == 0)
			{
				JPH::RegisterDefaultAllocator();
				JPH::Trace = JoltTrace;
				JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = JoltAssertFailed;)
				JPH::Factory::sInstance = new JPH::Factory();
				JPH::RegisterTypes();
			}
		}

		void ReleaseJolt()
		{
			if (--s_WorldCount == 0)
			{
				JPH::UnregisterTypes();
				delete JPH::Factory::sInstance;
				JPH::Factory::sInstance = nullptr;
			}
		}
	}

	JoltPhysics3D::~JoltPhysics3D()
	{
		Shutdown();
	}

	void JoltPhysics3D::Initialize(const Physics3DParams& params)
	{
		if (m_Data)
			return; // already live

		AcquireJolt();
		m_Data = new Internal::JoltPhysics3DData(params.MaxBodies);
		m_Data->PhysicsSystem.SetGravity(ToJolt(params.Gravity));
	}

	void JoltPhysics3D::Shutdown()
	{
		if (!m_Data)
			return;

		delete m_Data; // tears down PhysicsSystem, job system, allocators
		m_Data = nullptr;
		ReleaseJolt();
	}

	bool JoltPhysics3D::IsValid() const
	{
		return m_Data != nullptr;
	}

	PhysicsBodyId3D JoltPhysics3D::CreateBody(const RigidBodyParams3D& params)
	{
		if (!m_Data)
			return k_InvalidBody3D;

		JPH::BodyInterface& bodyInterface = m_Data->PhysicsSystem.GetBodyInterface();

		JPH::ShapeRefC shape;
		if (params.Shape == ColliderShape3D::Sphere)
			shape = new JPH::SphereShape(params.Radius);
		else
			shape = new JPH::BoxShape(ToJolt(params.HalfExtents));

		const bool isStatic = params.Type == BodyType3D::Static;
		const JPH::ObjectLayer layer = isStatic ? Internal::Layers::NON_MOVING : Internal::Layers::MOVING;

		JPH::BodyCreationSettings settings(shape,
			JPH::RVec3(params.Position.x, params.Position.y, params.Position.z),
			JPH::Quat(params.Rotation.x, params.Rotation.y, params.Rotation.z, params.Rotation.w),
			ToMotionType(params.Type), layer);
		settings.mFriction = params.Friction;
		settings.mRestitution = params.Restitution;

		const JPH::EActivation activation = isStatic ? JPH::EActivation::DontActivate : JPH::EActivation::Activate;
		const JPH::BodyID id = bodyInterface.CreateAndAddBody(settings, activation);
		if (id.IsInvalid())
			return k_InvalidBody3D;

		return id.GetIndexAndSequenceNumber();
	}

	void JoltPhysics3D::DestroyBody(PhysicsBodyId3D body)
	{
		if (!m_Data || body == k_InvalidBody3D)
			return;

		JPH::BodyInterface& bodyInterface = m_Data->PhysicsSystem.GetBodyInterface();
		const JPH::BodyID id = ToBodyId(body);
		bodyInterface.RemoveBody(id);
		bodyInterface.DestroyBody(id);
	}

	bool JoltPhysics3D::IsBodyValid(PhysicsBodyId3D body) const
	{
		if (!m_Data || body == k_InvalidBody3D)
			return false;
		return m_Data->PhysicsSystem.GetBodyInterface().IsAdded(ToBodyId(body));
	}

	void JoltPhysics3D::Step(float deltaTime, int collisionSteps)
	{
		if (!m_Data || deltaTime <= 0.0f)
			return;
		m_Data->PhysicsSystem.Update(deltaTime, collisionSteps, &m_Data->TempAllocator, &m_Data->JobSystem);
	}

	void JoltPhysics3D::SetGravity(const glm::vec3& gravity)
	{
		if (!m_Data)
			return;
		m_Data->PhysicsSystem.SetGravity(ToJolt(gravity));
	}

	glm::vec3 JoltPhysics3D::GetGravity() const
	{
		if (!m_Data)
			return glm::vec3(0.0f);
		return ToGlm(m_Data->PhysicsSystem.GetGravity());
	}

	glm::vec3 JoltPhysics3D::GetPosition(PhysicsBodyId3D body) const
	{
		if (!m_Data || body == k_InvalidBody3D)
			return glm::vec3(0.0f);

		const JPH::RVec3 p = m_Data->PhysicsSystem.GetBodyInterface().GetPosition(ToBodyId(body));
		return { (float)p.GetX(), (float)p.GetY(), (float)p.GetZ() };
	}

	glm::quat JoltPhysics3D::GetRotation(PhysicsBodyId3D body) const
	{
		if (!m_Data || body == k_InvalidBody3D)
			return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

		const JPH::Quat q = m_Data->PhysicsSystem.GetBodyInterface().GetRotation(ToBodyId(body));
		return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); // glm::quat is (w, x, y, z)
	}

	glm::mat4 JoltPhysics3D::GetTransform(PhysicsBodyId3D body) const
	{
		return glm::translate(glm::mat4(1.0f), GetPosition(body)) * glm::mat4_cast(GetRotation(body));
	}

	void JoltPhysics3D::SetLinearVelocity(PhysicsBodyId3D body, const glm::vec3& velocity)
	{
		if (!m_Data || body == k_InvalidBody3D)
			return;
		m_Data->PhysicsSystem.GetBodyInterface().SetLinearVelocity(ToBodyId(body), ToJolt(velocity));
	}

	glm::vec3 JoltPhysics3D::GetLinearVelocity(PhysicsBodyId3D body) const
	{
		if (!m_Data || body == k_InvalidBody3D)
			return glm::vec3(0.0f);
		return ToGlm(m_Data->PhysicsSystem.GetBodyInterface().GetLinearVelocity(ToBodyId(body)));
	}

	void JoltPhysics3D::ApplyImpulse(PhysicsBodyId3D body, const glm::vec3& impulse)
	{
		if (!m_Data || body == k_InvalidBody3D)
			return;
		m_Data->PhysicsSystem.GetBodyInterface().AddImpulse(ToBodyId(body), ToJolt(impulse));
	}

	void JoltPhysics3D::ApplyForce(PhysicsBodyId3D body, const glm::vec3& force)
	{
		if (!m_Data || body == k_InvalidBody3D)
			return;
		m_Data->PhysicsSystem.GetBodyInterface().AddForce(ToBodyId(body), ToJolt(force));
	}

}
