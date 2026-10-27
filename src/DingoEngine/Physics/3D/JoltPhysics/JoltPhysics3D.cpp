#include "depch.h"
#include "DingoEngine/Physics/3D/JoltPhysics/JoltPhysics3D.h"

// All Jolt usage is confined to this .cpp (+ the engine-internal JoltPhysics3DData.h).
#include "DingoEngine/Physics/3D/JoltPhysics/JoltPhysics3DData.h"

#include "DingoEngine/Physics/3D/JoltPhysics/JoltCharacterController3D.h"
#include "DingoEngine/Graphics/Mesh.h"

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/ScaleHelpers.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <cmath>
#include <Jolt/Physics/Body/BodyFilter.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cstdarg>
#include <cstdio>

JPH_SUPPRESS_WARNINGS

namespace Dingo
{

	namespace
	{
		using Internal::ToJolt;
		using Internal::ToGlm;

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

		inline JPH::BodyID ToBodyId(PhysicsBodyId3D body) { return JPH::BodyID(body); }

		// A body's Jolt user data: the query layers in the high half, the caller's UserData in the low.
		inline JPH::uint64 PackUserData(std::uint32_t queryLayers, std::uint32_t userData)
		{
			return (static_cast<JPH::uint64>(queryLayers) << 32) | userData;
		}

		inline std::uint32_t QueryLayersOf(const JPH::Body& body) { return static_cast<std::uint32_t>(body.GetUserData() >> 32); }

		// Unit axis, and a normal made perpendicular to it; false for a zero axis or a parallel normal.
		bool ResolveAxes(const glm::vec3& axis, const glm::vec3& normal, JPH::Vec3& outAxis, JPH::Vec3& outNormal)
		{
			if (!(glm::dot(axis, axis) > 1e-12f) || !(glm::dot(normal, normal) > 1e-12f))
				return false;

			const glm::vec3 unitAxis = glm::normalize(axis);
			glm::vec3 unitNormal = glm::normalize(normal);
			unitNormal -= unitAxis * glm::dot(unitAxis, unitNormal);
			if (!(glm::dot(unitNormal, unitNormal) > 1e-6f))
				return false;

			outAxis = ToJolt(unitAxis);
			outNormal = ToJolt(glm::normalize(unitNormal));
			return true;
		}

		using MotorState = Internal::JoltPhysics3DData::ConstraintRecord::MotorState;

		bool IsMotorValue(float value)
		{
			return std::isfinite(value) && value >= 0.0f;
		}

		bool JointHasMotorAxis(ConstraintType3D type, MotorAxis3D axis)
		{
			if (type == ConstraintType3D::Hinge)
				return axis == MotorAxis3D::Hinge;
			if (type == ConstraintType3D::SwingTwist)
				return axis == MotorAxis3D::Swing || axis == MotorAxis3D::Twist;
			return false;
		}

		bool JointHasMotors(ConstraintType3D type)
		{
			return type == ConstraintType3D::Hinge || type == ConstraintType3D::SwingTwist;
		}

		JPH::EMotorState WriteMotorSettings(JPH::MotorSettings& settings, const MotorState& motor)
		{
			float stiffness = motor.Params.Stiffness;
			JPH::EMotorState state = JPH::EMotorState::Off;
			if (motor.Mode != MotorMode3D::Off && motor.Params.MaxTorque > 0.0f)
			{
				if (motor.Mode == MotorMode3D::Position)
					state = JPH::EMotorState::PositionAndVelocity;
				else if (motor.Params.Damping > 0.0f)
				{
					state = JPH::EMotorState::PositionAndVelocity;
					stiffness = 0.0f;
				}
				else
					state = JPH::EMotorState::Velocity;
			}
			settings.mSpringSettings = JPH::SpringSettings(JPH::ESpringMode::StiffnessAndDamping, stiffness, motor.Params.Damping);
			settings.SetTorqueLimit(motor.Params.MaxTorque);
			return state;
		}

		void ApplyMotors(Internal::JoltPhysics3DData::ConstraintRecord& record)
		{
			const MotorState& hingeMotor = record.Motors[static_cast<int>(MotorAxis3D::Hinge)];
			const MotorState& swingMotor = record.Motors[static_cast<int>(MotorAxis3D::Swing)];
			const MotorState& twistMotor = record.Motors[static_cast<int>(MotorAxis3D::Twist)];
			const JPH::Quat targetRotation = ToJolt(record.TargetRotation);
			const JPH::Vec3 targetVelocity = ToJolt(record.TargetVelocity);

			if (record.Type == ConstraintType3D::Hinge)
			{
				auto* hinge = static_cast<JPH::HingeConstraint*>(record.Constraint.GetPtr());
				const JPH::EMotorState state = WriteMotorSettings(hinge->GetMotorSettings(), hingeMotor);
				hinge->SetTargetOrientationBS(targetRotation);
				hinge->SetTargetAngularVelocity(hingeMotor.Mode == MotorMode3D::Velocity ? record.HingeAxisA.Dot(targetVelocity) : 0.0f);
				hinge->SetMotorState(state);
			}
			else if (record.Type == ConstraintType3D::SwingTwist)
			{
				auto* joint = static_cast<JPH::SwingTwistConstraint*>(record.Constraint.GetPtr());
				const JPH::EMotorState swingState = WriteMotorSettings(joint->GetSwingMotorSettings(), swingMotor);
				const JPH::EMotorState twistState = WriteMotorSettings(joint->GetTwistMotorSettings(), twistMotor);
				joint->SetTargetOrientationBS(targetRotation);

				const JPH::Vec3 local = joint->GetConstraintToBody1().InverseRotate(targetVelocity);
				const float swing = swingMotor.Mode == MotorMode3D::Velocity ? 1.0f : 0.0f;
				const float twist = twistMotor.Mode == MotorMode3D::Velocity ? 1.0f : 0.0f;
				joint->SetTargetAngularVelocityCS(JPH::Vec3(local.GetX() * twist, local.GetY() * swing, local.GetZ() * swing));
				joint->SetSwingMotorState(swingState);
				joint->SetTwistMotorState(twistState);
			}
		}

		bool AnyMotorInMode(const Internal::JoltPhysics3DData::ConstraintRecord& record, MotorMode3D mode)
		{
			for (const MotorState& motor : record.Motors)
			{
				if (motor.Mode == mode)
					return true;
			}
			return false;
		}

		void WakeConstraintBodies(JPH::BodyInterface& bodyInterface, const Internal::JoltPhysics3DData::ConstraintRecord& record)
		{
			bodyInterface.ActivateBody(ToBodyId(record.BodyA));
			if (record.BodyB != k_InvalidBody3D)
				bodyInterface.ActivateBody(ToBodyId(record.BodyB));
		}

		class QueryBodyFilter final : public JPH::BodyFilter
		{
		public:
			QueryBodyFilter(const QueryFilter3D& filter, JPH::BodyID exclude = JPH::BodyID())
				: m_Filter(filter), m_Exclude(exclude)
			{
			}

			virtual bool ShouldCollide(const JPH::BodyID& inBodyID) const override
			{
				if (inBodyID == m_Exclude)
					return false;
				const PhysicsBodyId3D id = inBodyID.GetIndexAndSequenceNumber();
				return std::find(m_Filter.IgnoredBodies.begin(), m_Filter.IgnoredBodies.end(), id) == m_Filter.IgnoredBodies.end();
			}

			virtual bool ShouldCollideLocked(const JPH::Body& inBody) const override
			{
				if (inBody.IsSensor() && !m_Filter.IncludeSensors)
					return false;
				return (QueryLayersOf(inBody) & m_Filter.Layers) != 0;
			}

		private:
			const QueryFilter3D& m_Filter;
			JPH::BodyID m_Exclude;
		};

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

	// A body joins the filter's group with its first ignored partner and leaves it with its last, so
	// the pairs of bodies that ignore nothing never reach the filter.
	static void LinkIgnoredPartner(Internal::JoltPhysics3DData& data, PhysicsBodyId3D body, PhysicsBodyId3D partner)
	{
		std::vector<std::uint32_t>& partners = data.IgnoredPartners[body];
		if (partners.empty())
			data.PhysicsSystem.GetBodyInterface().SetCollisionGroup(ToBodyId(body), JPH::CollisionGroup(data.PairFilter.GetPtr(), body, 0));
		partners.push_back(partner);
	}

	static void UnlinkIgnoredPartner(Internal::JoltPhysics3DData& data, PhysicsBodyId3D body, PhysicsBodyId3D partner)
	{
		auto it = data.IgnoredPartners.find(body);
		if (it == data.IgnoredPartners.end())
			return;

		std::erase(it->second, partner);
		if (it->second.empty())
		{
			data.IgnoredPartners.erase(it);
			data.PhysicsSystem.GetBodyInterface().SetCollisionGroup(ToBodyId(body), JPH::CollisionGroup());
		}
	}

	static void AddPairIgnore(Internal::JoltPhysics3DData& data, PhysicsBodyId3D a, PhysicsBodyId3D b)
	{
		Internal::IgnoredPairFilter& filter = *data.PairFilter;
		const bool fresh = filter.Find(a, b) == nullptr;
		++filter.Entry(a, b).Constraints;
		if (fresh)
		{
			LinkIgnoredPartner(data, a, b);
			LinkIgnoredPartner(data, b, a);
		}
	}

	static void ReleasePairIgnore(Internal::JoltPhysics3DData& data, PhysicsBodyId3D a, PhysicsBodyId3D b)
	{
		Internal::IgnoredPairFilter& filter = *data.PairFilter;
		if (!filter.Find(a, b))
			return;

		Internal::IgnoredPairFilter::PairSources& sources = filter.Entry(a, b);
		if (sources.Constraints > 0)
			--sources.Constraints;
		if (!sources.User && sources.Constraints == 0)
		{
			filter.Erase(a, b);
			UnlinkIgnoredPartner(data, a, b);
			UnlinkIgnoredPartner(data, b, a);
		}
	}

	void JoltPhysics3D::DestroyConstraintInternal(Internal::JoltPhysics3DData& data, ConstraintId3D id)
	{
		auto it = data.Constraints.find(id);
		if (it == data.Constraints.end())
			return;

		Internal::JoltPhysics3DData::ConstraintRecord record = std::move(it->second);
		data.Constraints.erase(it);

		data.PhysicsSystem.RemoveConstraint(record.Constraint.GetPtr());

		JPH::BodyInterface& bodyInterface = data.PhysicsSystem.GetBodyInterface();
		bodyInterface.ActivateBody(ToBodyId(record.BodyA));
		if (record.BodyB != k_InvalidBody3D)
			bodyInterface.ActivateBody(ToBodyId(record.BodyB));

		for (PhysicsBodyId3D body : { record.BodyA, record.BodyB })
		{
			auto list = data.BodyConstraints.find(body);
			if (list == data.BodyConstraints.end())
				continue;
			std::erase(list->second, id);
			if (list->second.empty())
				data.BodyConstraints.erase(list);
		}

		if (record.IgnoresPair)
			ReleasePairIgnore(data, record.BodyA, record.BodyB);
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

		// A constraint holds raw Body pointers, so each goes before the bodies the world frees.
		std::vector<ConstraintId3D> ids;
		ids.reserve(m_Data->Constraints.size());
		for (const auto& entry : m_Data->Constraints)
			ids.push_back(entry.first);
		std::sort(ids.begin(), ids.end());
		for (ConstraintId3D id : ids)
			DestroyConstraintInternal(*m_Data, id);

		delete m_Data; // tears down PhysicsSystem, job system, allocators
		m_Data = nullptr;
		ReleaseJolt();
	}

	bool JoltPhysics3D::IsValid() const
	{
		return m_Data != nullptr;
	}

	// File-local rather than a member: JoltPhysics3D.h deliberately names no JPH type.
	static JPH::ShapeRefC GetOrCreateShape(Internal::JoltPhysics3DData& data, const RigidBodyParams3D& params)
	{
		Internal::ShapeKey key;
		key.Type = static_cast<int>(params.Shape);
		switch (params.Shape)
		{
			case ColliderShape3D::Sphere:
				key.A = params.Radius;
				break;
			case ColliderShape3D::Capsule:
				key.A = params.HalfHeight;
				key.B = params.Radius;
				break;
			default:
				key.A = params.HalfExtents.x;
				key.B = params.HalfExtents.y;
				key.C = params.HalfExtents.z;
				break;
		}

		auto existing = data.ShapeCache.find(key);
		if (existing != data.ShapeCache.end())
			return existing->second;

		JPH::ShapeRefC shape;
		switch (params.Shape)
		{
			case ColliderShape3D::Sphere:  shape = new JPH::SphereShape(params.Radius); break;
			case ColliderShape3D::Capsule: shape = new JPH::CapsuleShape(params.HalfHeight, params.Radius); break; // (halfHeightOfCylinder, radius)
			default:                       shape = new JPH::BoxShape(ToJolt(params.HalfExtents)); break;
		}

		data.ShapeCache.emplace(key, shape);
		return shape;
	}

	static JPH::ShapeRefC BuildMeshShape(const Mesh& mesh, bool convex)
	{
		const std::vector<MeshVertex>& vertices = mesh.GetVertices();
		const std::vector<uint32_t>& indices = mesh.GetIndices();

		JPH::ShapeSettings::ShapeResult result;
		if (convex)
		{
			JPH::Array<JPH::Vec3> points;
			points.reserve(vertices.size());
			for (const MeshVertex& vertex : vertices)
				points.push_back(ToJolt(vertex.Position));

			result = JPH::ConvexHullShapeSettings(points).Create();
		}
		else
		{
			if (indices.empty() || indices.size() % 3 != 0)
			{
				DE_CORE_ERROR("Physics3D: mesh collider needs a triangle list, got {} indices", indices.size());
				return nullptr;
			}

			JPH::VertexList triangleVertices;
			triangleVertices.reserve(vertices.size());
			for (const MeshVertex& vertex : vertices)
				triangleVertices.push_back(JPH::Float3(vertex.Position.x, vertex.Position.y, vertex.Position.z));

			// MeshShapeSettings reads every index while it sanitizes, before its own range check.
			JPH::IndexedTriangleList triangles;
			triangles.reserve(indices.size() / 3);
			for (std::size_t i = 0; i < indices.size(); i += 3)
			{
				if (indices[i] >= vertices.size() || indices[i + 1] >= vertices.size() || indices[i + 2] >= vertices.size())
				{
					DE_CORE_ERROR("Physics3D: mesh collider index out of range at triangle {} ({} vertices)", i / 3, vertices.size());
					return nullptr;
				}
				triangles.push_back(JPH::IndexedTriangle(indices[i], indices[i + 1], indices[i + 2], 0));
			}

			result = JPH::MeshShapeSettings(std::move(triangleVertices), std::move(triangles)).Create();
		}

		if (result.HasError())
		{
			DE_CORE_ERROR("Physics3D: building a {} collider from a {}-vertex mesh failed: {}",
				convex ? "convex hull" : "mesh", vertices.size(), result.GetError().c_str());
			return nullptr;
		}
		return result.Get();
	}

	static JPH::ShapeRefC GetOrCreateMeshShape(Internal::JoltPhysics3DData& data, const RigidBodyParams3D& params)
	{
		if (!params.Mesh)
		{
			DE_CORE_ERROR("Physics3D: a Mesh or ConvexHull collider needs RigidBodyParams3D::Mesh");
			return nullptr;
		}

		bool convex = params.Shape == ColliderShape3D::ConvexHull;
		if (!convex && params.Type == BodyType3D::Dynamic)
		{
			DE_CORE_WARN("Physics3D: a Dynamic body cannot use a Mesh collider; using the mesh's convex hull instead");
			convex = true;
		}

		auto& cache = convex ? data.ConvexHullShapeCache : data.MeshShapeCache;
		JPH::ShapeRefC shape;
		if (auto existing = cache.find(params.Mesh->GetId()); existing != cache.end())
		{
			shape = existing->second;
		}
		else
		{
			shape = BuildMeshShape(*params.Mesh, convex);
			if (!shape)
				return nullptr;

			std::erase_if(cache, [](const auto& entry) { return entry.second->GetRefCount() == 1; });
			cache.emplace(params.Mesh->GetId(), shape);
		}

		const JPH::Vec3 scale = ToJolt(params.MeshScale);
		if (!shape->IsValidScale(scale))
		{
			DE_CORE_ERROR("Physics3D: mesh collider scale ({}, {}, {}) has a zero axis",
				params.MeshScale.x, params.MeshScale.y, params.MeshScale.z);
			return nullptr;
		}

		if (JPH::ScaleHelpers::IsNotScaled(scale))
			return shape;
		return new JPH::ScaledShape(shape, scale);
	}

	PhysicsBodyId3D JoltPhysics3D::CreateBody(const RigidBodyParams3D& params)
	{
		if (!m_Data)
			return k_InvalidBody3D;

		JPH::BodyInterface& bodyInterface = m_Data->PhysicsSystem.GetBodyInterface();

		const bool meshBacked = params.Shape == ColliderShape3D::Mesh || params.Shape == ColliderShape3D::ConvexHull;
		const JPH::ShapeRefC shape = meshBacked ? GetOrCreateMeshShape(*m_Data, params) : GetOrCreateShape(*m_Data, params);
		if (!shape)
			return k_InvalidBody3D;

		const bool isStatic = params.Type == BodyType3D::Static;
		const JPH::ObjectLayer layer = isStatic ? Internal::Layers::NON_MOVING : Internal::Layers::MOVING;

		JPH::BodyCreationSettings settings(shape,
			JPH::RVec3(params.Position.x, params.Position.y, params.Position.z),
			JPH::Quat(params.Rotation.x, params.Rotation.y, params.Rotation.z, params.Rotation.w),
			ToMotionType(params.Type), layer);
		settings.mFriction = params.Friction;
		settings.mRestitution = params.Restitution;
		settings.mMotionQuality = params.ContinuousCollision ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
		settings.mIsSensor = params.IsSensor;
		settings.mUserData = PackUserData(params.QueryLayers, params.UserData);

		// A triangle mesh or a flat hull has no volume to derive mass from. A kinematic
		// body's mass never reaches the solver, so any valid value satisfies Jolt; a
		// dynamic one would integrate to NaN.
		if (meshBacked && params.Type == BodyType3D::Kinematic)
		{
			settings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
			settings.mMassPropertiesOverride.SetMassAndInertiaOfSolidBox(JPH::Vec3::sReplicate(1.0f), 1.0f);
		}
		else if (meshBacked && params.Type == BodyType3D::Dynamic && !(shape->GetMassProperties().mMass > 0.0f))
		{
			DE_CORE_ERROR("Physics3D: a Dynamic body's convex hull has no volume (is the mesh flat?)");
			return k_InvalidBody3D;
		}

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
		if (!bodyInterface.IsAdded(id))
			return;

		if (auto it = m_Data->BodyConstraints.find(body); it != m_Data->BodyConstraints.end())
		{
			const std::vector<ConstraintId3D> constraints = it->second;
			for (ConstraintId3D constraint : constraints)
				DestroyConstraintInternal(*m_Data, constraint);
		}

		if (auto it = m_Data->IgnoredPartners.find(body); it != m_Data->IgnoredPartners.end())
		{
			const std::vector<std::uint32_t> partners = std::move(it->second);
			m_Data->IgnoredPartners.erase(it);
			for (std::uint32_t partner : partners)
			{
				m_Data->PairFilter->Erase(body, partner);
				UnlinkIgnoredPartner(*m_Data, partner, body);
			}
		}

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
		if (Internal::CrashHandler::ProbeFunction probe = Internal::CrashHandler::TakeProbe(Internal::CrashHandler::ProbeThread::PhysicsWorker))
			m_Data->JobSystem.CreateJob("CrashProbe", JPH::Color::sRed, [probe] { probe(); });
		m_Data->LastDeltaTime = deltaTime;
		m_Data->LastCollisionSteps = (std::max)(collisionSteps, 1);
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

	void JoltPhysics3D::IgnoreCollision(PhysicsBodyId3D a, PhysicsBodyId3D b, bool ignore)
	{
		if (!m_Data || a == k_InvalidBody3D || b == k_InvalidBody3D || a == b)
			return;

		Internal::IgnoredPairFilter& filter = *m_Data->PairFilter;
		const Internal::IgnoredPairFilter::PairSources* existing = filter.Find(a, b);
		if ((existing && existing->User) == ignore)
			return;

		JPH::BodyInterface& bodyInterface = m_Data->PhysicsSystem.GetBodyInterface();
		if (ignore)
		{
			if (!bodyInterface.IsAdded(ToBodyId(a)) || !bodyInterface.IsAdded(ToBodyId(b)))
				return;

			const bool fresh = existing == nullptr;
			filter.Entry(a, b).User = true;
			if (fresh)
			{
				LinkIgnoredPartner(*m_Data, a, b);
				LinkIgnoredPartner(*m_Data, b, a);
			}
		}
		else
		{
			Internal::IgnoredPairFilter::PairSources& sources = filter.Entry(a, b);
			sources.User = false;
			if (sources.Constraints == 0)
			{
				filter.Erase(a, b);
				UnlinkIgnoredPartner(*m_Data, a, b);
				UnlinkIgnoredPartner(*m_Data, b, a);
			}
		}

		// The broad phase only pairs a body that is awake: a sleeping pair would keep its old
		// contact state until something else woke one of them.
		bodyInterface.ActivateBody(ToBodyId(a));
		bodyInterface.ActivateBody(ToBodyId(b));
	}

	bool JoltPhysics3D::IsCollisionIgnored(PhysicsBodyId3D a, PhysicsBodyId3D b) const
	{
		return m_Data && m_Data->PairFilter->Contains(a, b);
	}

	void JoltPhysics3D::SetPosition(PhysicsBodyId3D body, const glm::vec3& position)
	{
		if (!m_Data || body == k_InvalidBody3D)
			return;
		m_Data->PhysicsSystem.GetBodyInterface().SetPosition(ToBodyId(body), ToJolt(position), JPH::EActivation::Activate);
	}

	void JoltPhysics3D::SetRotation(PhysicsBodyId3D body, const glm::quat& rotation)
	{
		if (!m_Data || body == k_InvalidBody3D)
			return;
		m_Data->PhysicsSystem.GetBodyInterface().SetRotation(ToBodyId(body), ToJolt(rotation), JPH::EActivation::Activate);
	}

	void JoltPhysics3D::SetPositionAndRotation(PhysicsBodyId3D body, const glm::vec3& position, const glm::quat& rotation)
	{
		if (!m_Data || body == k_InvalidBody3D)
			return;
		m_Data->PhysicsSystem.GetBodyInterface().SetPositionAndRotation(ToBodyId(body), ToJolt(position), ToJolt(rotation), JPH::EActivation::Activate);
	}

	void JoltPhysics3D::MoveKinematic(PhysicsBodyId3D body, const glm::vec3& targetPosition, const glm::quat& targetRotation, float deltaTime)
	{
		if (!m_Data || body == k_InvalidBody3D || deltaTime <= 0.0f)
			return;
		JPH::BodyInterface& bodyInterface = m_Data->PhysicsSystem.GetBodyInterface();
#if !DE_RELEASE
		// Jolt's MoveKinematic on a non-kinematic body misbehaves instead of no-oping.
		// Guard is compiled out in Release: it costs a body lookup on every call.
		if (bodyInterface.GetMotionType(ToBodyId(body)) != JPH::EMotionType::Kinematic)
		{
			DE_CORE_WARN("MoveKinematic called on non-kinematic body {}; ignored.", body);
			return;
		}
#endif
		bodyInterface.MoveKinematic(ToBodyId(body), ToJolt(targetPosition), ToJolt(targetRotation), deltaTime);
	}

	void JoltPhysics3D::SetAngularVelocity(PhysicsBodyId3D body, const glm::vec3& angularVelocity)
	{
		if (!m_Data || body == k_InvalidBody3D)
			return;
		m_Data->PhysicsSystem.GetBodyInterface().SetAngularVelocity(ToBodyId(body), ToJolt(angularVelocity));
	}

	glm::vec3 JoltPhysics3D::GetAngularVelocity(PhysicsBodyId3D body) const
	{
		if (!m_Data || body == k_InvalidBody3D)
			return glm::vec3(0.0f);
		return ToGlm(m_Data->PhysicsSystem.GetBodyInterface().GetAngularVelocity(ToBodyId(body)));
	}

	ConstraintId3D JoltPhysics3D::CreateConstraint(const ConstraintParams3D& params)
	{
		if (!m_Data)
			return k_InvalidConstraint3D;

		Internal::JoltPhysics3DData& data = *m_Data;
		JPH::BodyInterface& bodyInterface = data.PhysicsSystem.GetBodyInterface();

		auto fail = [](const char* reason)
		{
			DE_CORE_WARN("Physics3D::CreateConstraint: {}", reason);
			return k_InvalidConstraint3D;
		};

		const bool pinned = params.BodyB == k_InvalidBody3D;
		if (params.BodyA == k_InvalidBody3D || !bodyInterface.IsAdded(ToBodyId(params.BodyA)))
			return fail("BodyA is invalid or destroyed");
		if (!pinned && !bodyInterface.IsAdded(ToBodyId(params.BodyB)))
			return fail("BodyB is destroyed");
		if (params.BodyA == params.BodyB)
			return fail("the constraint needs two different bodies");

		const JPH::BodyID idA = ToBodyId(params.BodyA);
		const JPH::BodyID idB = pinned ? JPH::BodyID() : ToBodyId(params.BodyB);
		const bool staticA = bodyInterface.GetMotionType(idA) == JPH::EMotionType::Static;
		const bool staticB = pinned || bodyInterface.GetMotionType(idB) == JPH::EMotionType::Static;
		if (staticA && staticB)
			return fail("both sides are static (a world pin counts as static)");
		if (data.NextConstraintId == k_InvalidConstraint3D)
			return fail("constraint ids are exhausted");

		const bool readsAxes = params.Type != ConstraintType3D::Point && params.Type != ConstraintType3D::Distance;
		JPH::Vec3 axisA = JPH::Vec3::sAxisY(), axisB = JPH::Vec3::sAxisY();
		JPH::Vec3 normalA = JPH::Vec3::sAxisX(), normalB = JPH::Vec3::sAxisX();
		if (readsAxes)
		{
			if (!ResolveAxes(params.AxisA, params.NormalAxisA, axisA, normalA)
				|| !ResolveAxes(params.AxisB, params.NormalAxisB, axisB, normalB))
				return fail("an axis is zero or NormalAxis is parallel to Axis");
		}

		const JPH::Vec3 pointA = ToJolt(params.AnchorA) - bodyInterface.GetShape(idA)->GetCenterOfMass();
		const JPH::Vec3 pointB = pinned ? ToJolt(params.AnchorB) : ToJolt(params.AnchorB) - bodyInterface.GetShape(idB)->GetCenterOfMass();
		const JPH::EConstraintSpace space = JPH::EConstraintSpace::LocalToBodyCOM;

		JPH::Ref<JPH::TwoBodyConstraint> constraint;
		auto create = [&](const JPH::TwoBodyConstraintSettings& settings)
		{
			constraint = bodyInterface.CreateConstraint(&settings, idA, idB);
		};

		switch (params.Type)
		{
			case ConstraintType3D::Fixed:
			{
				JPH::FixedConstraintSettings settings;
				settings.mSpace = space;
				settings.mPoint1 = JPH::RVec3(pointA);
				settings.mPoint2 = JPH::RVec3(pointB);
				settings.mAxisX1 = normalA;
				settings.mAxisY1 = axisA;
				settings.mAxisX2 = normalB;
				settings.mAxisY2 = axisB;
				create(settings);
				break;
			}
			case ConstraintType3D::Point:
			{
				JPH::PointConstraintSettings settings;
				settings.mSpace = space;
				settings.mPoint1 = JPH::RVec3(pointA);
				settings.mPoint2 = JPH::RVec3(pointB);
				create(settings);
				break;
			}
			case ConstraintType3D::Hinge:
			{
				JPH::HingeConstraintSettings settings;
				settings.mSpace = space;
				settings.mPoint1 = JPH::RVec3(pointA);
				settings.mPoint2 = JPH::RVec3(pointB);
				settings.mHingeAxis1 = axisA;
				settings.mNormalAxis1 = normalA;
				settings.mHingeAxis2 = axisB;
				settings.mNormalAxis2 = normalB;
				if (params.MinLimit < params.MaxLimit)
				{
					if (params.MinLimit < -JPH::JPH_PI || params.MinLimit > 0.0f || params.MaxLimit < 0.0f || params.MaxLimit > JPH::JPH_PI)
						return fail("hinge limits must satisfy -pi <= MinLimit <= 0 <= MaxLimit <= pi");
					settings.mLimitsMin = params.MinLimit;
					settings.mLimitsMax = params.MaxLimit;
				}
				create(settings);
				break;
			}
			case ConstraintType3D::SwingTwist:
			{
				if (params.TwistMin > params.TwistMax)
					return fail("TwistMin is above TwistMax");
				JPH::SwingTwistConstraintSettings settings;
				settings.mSpace = space;
				settings.mPosition1 = JPH::RVec3(pointA);
				settings.mPosition2 = JPH::RVec3(pointB);
				settings.mTwistAxis1 = axisA;
				settings.mPlaneAxis1 = normalA;
				settings.mTwistAxis2 = axisB;
				settings.mPlaneAxis2 = normalB;
				settings.mSwingType = JPH::ESwingType::Cone;
				settings.mNormalHalfConeAngle = std::clamp(params.SwingLimitY, 0.0f, JPH::JPH_PI);
				settings.mPlaneHalfConeAngle = std::clamp(params.SwingLimitZ, 0.0f, JPH::JPH_PI);
				settings.mTwistMinAngle = std::clamp(params.TwistMin, -JPH::JPH_PI, JPH::JPH_PI);
				settings.mTwistMaxAngle = std::clamp(params.TwistMax, -JPH::JPH_PI, JPH::JPH_PI);
				create(settings);
				break;
			}
			case ConstraintType3D::Distance:
			{
				if (!(params.MinLimit >= 0.0f) || !(params.MaxLimit >= 0.0f))
					return fail("distance limits may not be negative");
				if (params.MinLimit > params.MaxLimit)
					return fail("distance MinLimit is above MaxLimit");
				JPH::DistanceConstraintSettings settings;
				settings.mSpace = space;
				settings.mPoint1 = JPH::RVec3(pointA);
				settings.mPoint2 = JPH::RVec3(pointB);
				settings.mMinDistance = params.MinLimit;
				settings.mMaxDistance = params.MaxLimit;
				create(settings);
				break;
			}
			case ConstraintType3D::Slider:
			{
				if (params.MinLimit < params.MaxLimit && (params.MinLimit > 0.0f || params.MaxLimit < 0.0f))
					return fail("slider limits must satisfy MinLimit <= 0 <= MaxLimit");
				JPH::SliderConstraintSettings settings;
				settings.mSpace = space;
				settings.mPoint1 = JPH::RVec3(pointA);
				settings.mPoint2 = JPH::RVec3(pointB);
				settings.mSliderAxis1 = axisA;
				settings.mNormalAxis1 = normalA;
				settings.mSliderAxis2 = axisB;
				settings.mNormalAxis2 = normalB;
				if (params.MinLimit < params.MaxLimit)
				{
					settings.mLimitsMin = params.MinLimit;
					settings.mLimitsMax = params.MaxLimit;
				}
				create(settings);
				break;
			}
		}

		if (!constraint)
			return fail("the physics engine rejected the constraint");

		data.PhysicsSystem.AddConstraint(constraint.GetPtr());
		bodyInterface.ActivateBody(idA);
		if (!pinned)
			bodyInterface.ActivateBody(idB);

		const ConstraintId3D id = data.NextConstraintId++;
		Internal::JoltPhysics3DData::ConstraintRecord record;
		record.Constraint = constraint;
		record.Type = params.Type;
		record.BodyA = params.BodyA;
		record.BodyB = params.BodyB;
		record.LocalA = pointA;
		record.LocalB = pointB;
		record.UserData = params.UserData;
		record.HingeAxisA = axisA;
		if (!params.CollideConnected && !pinned)
		{
			AddPairIgnore(data, params.BodyA, params.BodyB);
			record.IgnoresPair = true;
		}
		data.Constraints.emplace(id, std::move(record));

		data.BodyConstraints[params.BodyA].push_back(id);
		if (!pinned)
			data.BodyConstraints[params.BodyB].push_back(id);
		return id;
	}

	void JoltPhysics3D::DestroyConstraint(ConstraintId3D constraint)
	{
		if (m_Data)
			DestroyConstraintInternal(*m_Data, constraint);
	}

	bool JoltPhysics3D::IsConstraintValid(ConstraintId3D constraint) const
	{
		return m_Data && m_Data->Constraints.contains(constraint);
	}

	void JoltPhysics3D::SetConstraintEnabled(ConstraintId3D constraint, bool enabled)
	{
		if (!m_Data)
			return;
		auto it = m_Data->Constraints.find(constraint);
		if (it == m_Data->Constraints.end())
			return;

		it->second.Constraint->SetEnabled(enabled);
		JPH::BodyInterface& bodyInterface = m_Data->PhysicsSystem.GetBodyInterface();
		bodyInterface.ActivateBody(ToBodyId(it->second.BodyA));
		if (it->second.BodyB != k_InvalidBody3D)
			bodyInterface.ActivateBody(ToBodyId(it->second.BodyB));
	}

	bool JoltPhysics3D::IsConstraintEnabled(ConstraintId3D constraint) const
	{
		if (!m_Data)
			return false;
		auto it = m_Data->Constraints.find(constraint);
		return it != m_Data->Constraints.end() && it->second.Constraint->GetEnabled();
	}

	PhysicsBodyId3D JoltPhysics3D::GetConstraintBody(ConstraintId3D constraint, int index) const
	{
		if (!m_Data)
			return k_InvalidBody3D;
		auto it = m_Data->Constraints.find(constraint);
		if (it == m_Data->Constraints.end())
			return k_InvalidBody3D;
		return index == 0 ? it->second.BodyA : index == 1 ? it->second.BodyB : k_InvalidBody3D;
	}

	float JoltPhysics3D::GetConstraintValue(ConstraintId3D constraint) const
	{
		if (!m_Data)
			return 0.0f;
		auto it = m_Data->Constraints.find(constraint);
		if (it == m_Data->Constraints.end())
			return 0.0f;

		const Internal::JoltPhysics3DData::ConstraintRecord& record = it->second;
		switch (record.Type)
		{
			case ConstraintType3D::Hinge:
				return static_cast<const JPH::HingeConstraint*>(record.Constraint.GetPtr())->GetCurrentAngle();
			case ConstraintType3D::Slider:
				return static_cast<const JPH::SliderConstraint*>(record.Constraint.GetPtr())->GetCurrentPosition();
			case ConstraintType3D::Distance:
			{
				const JPH::BodyInterface& bodyInterface = m_Data->PhysicsSystem.GetBodyInterface();
				const JPH::RVec3 a = bodyInterface.GetCenterOfMassTransform(ToBodyId(record.BodyA)) * JPH::RVec3(record.LocalA);
				const JPH::RVec3 b = record.BodyB == k_InvalidBody3D
					? JPH::RVec3(record.LocalB)
					: bodyInterface.GetCenterOfMassTransform(ToBodyId(record.BodyB)) * JPH::RVec3(record.LocalB);
				return static_cast<float>((b - a).Length());
			}
			default:
				return 0.0f;
		}
	}

	std::uint32_t JoltPhysics3D::GetConstraintUserData(ConstraintId3D constraint) const
	{
		if (!m_Data)
			return 0;
		auto it = m_Data->Constraints.find(constraint);
		return it != m_Data->Constraints.end() ? it->second.UserData : 0;
	}

	void JoltPhysics3D::SetMotor(ConstraintId3D constraint, MotorAxis3D axis, MotorMode3D mode, const MotorParams3D& params)
	{
		if (!m_Data)
			return;
		auto it = m_Data->Constraints.find(constraint);
		if (it == m_Data->Constraints.end())
			return;
		if (!IsMotorValue(params.Stiffness) || !IsMotorValue(params.Damping) || !IsMotorValue(params.MaxTorque))
			return;
		if (mode != MotorMode3D::Off && mode != MotorMode3D::Velocity && mode != MotorMode3D::Position)
			return;

		Internal::JoltPhysics3DData::ConstraintRecord& record = it->second;
		if (!JointHasMotorAxis(record.Type, axis))
		{
			DE_CORE_WARN("Physics3D::SetMotor: the constraint has no such motor axis; ignored");
			return;
		}

		MotorState& motor = record.Motors[static_cast<int>(axis)];
		if (motor.Mode == mode && motor.Params == params)
			return;

		const bool wasActive = motor.Mode != MotorMode3D::Off;
		motor.Mode = mode;
		motor.Params = params;
		ApplyMotors(record);
		if (wasActive || mode != MotorMode3D::Off)
			WakeConstraintBodies(m_Data->PhysicsSystem.GetBodyInterface(), record);
	}

	MotorParams3D JoltPhysics3D::GetMotorParams(ConstraintId3D constraint, MotorAxis3D axis) const
	{
		if (!m_Data)
			return {};
		auto it = m_Data->Constraints.find(constraint);
		if (it == m_Data->Constraints.end() || !JointHasMotorAxis(it->second.Type, axis))
			return {};
		return it->second.Motors[static_cast<int>(axis)].Params;
	}

	MotorMode3D JoltPhysics3D::GetMotorMode(ConstraintId3D constraint, MotorAxis3D axis) const
	{
		if (!m_Data)
			return MotorMode3D::Off;
		auto it = m_Data->Constraints.find(constraint);
		if (it == m_Data->Constraints.end() || !JointHasMotorAxis(it->second.Type, axis))
			return MotorMode3D::Off;
		return it->second.Motors[static_cast<int>(axis)].Mode;
	}

	void JoltPhysics3D::SetMotorTargetRotation(ConstraintId3D constraint, const glm::quat& rotation)
	{
		if (!m_Data)
			return;
		auto it = m_Data->Constraints.find(constraint);
		if (it == m_Data->Constraints.end() || !JointHasMotors(it->second.Type))
			return;

		const float lengthSquared = glm::dot(rotation, rotation);
		if (!std::isfinite(lengthSquared) || !(lengthSquared > 1e-12f))
			return;

		Internal::JoltPhysics3DData::ConstraintRecord& record = it->second;
		const glm::quat unit = rotation / std::sqrt(lengthSquared);
		if (unit == record.TargetRotation)
			return;

		record.TargetRotation = unit;
		ApplyMotors(record);
		if (AnyMotorInMode(record, MotorMode3D::Position))
			WakeConstraintBodies(m_Data->PhysicsSystem.GetBodyInterface(), record);
	}

	void JoltPhysics3D::SetMotorTargetVelocity(ConstraintId3D constraint, const glm::vec3& angularVelocity)
	{
		if (!m_Data)
			return;
		auto it = m_Data->Constraints.find(constraint);
		if (it == m_Data->Constraints.end() || !JointHasMotors(it->second.Type))
			return;
		if (!std::isfinite(angularVelocity.x) || !std::isfinite(angularVelocity.y) || !std::isfinite(angularVelocity.z))
			return;

		Internal::JoltPhysics3DData::ConstraintRecord& record = it->second;
		if (angularVelocity == record.TargetVelocity)
			return;

		record.TargetVelocity = angularVelocity;
		ApplyMotors(record);
		if (AnyMotorInMode(record, MotorMode3D::Velocity))
			WakeConstraintBodies(m_Data->PhysicsSystem.GetBodyInterface(), record);
	}

	float JoltPhysics3D::GetMotorTorque(ConstraintId3D constraint, MotorAxis3D axis) const
	{
		if (!m_Data || !(m_Data->LastDeltaTime > 0.0f))
			return 0.0f;
		auto it = m_Data->Constraints.find(constraint);
		if (it == m_Data->Constraints.end() || !JointHasMotorAxis(it->second.Type, axis))
			return 0.0f;

		const Internal::JoltPhysics3DData::ConstraintRecord& record = it->second;
		const MotorState& motor = record.Motors[static_cast<int>(axis)];
		if (motor.Mode == MotorMode3D::Off || !(motor.Params.MaxTorque > 0.0f))
			return 0.0f;

		const JPH::BodyInterface& bodyInterface = m_Data->PhysicsSystem.GetBodyInterface();
		const bool awake = bodyInterface.IsActive(ToBodyId(record.BodyA))
			|| (record.BodyB != k_InvalidBody3D && bodyInterface.IsActive(ToBodyId(record.BodyB)));
		if (!awake)
			return 0.0f;

		float lambda = 0.0f;
		if (record.Type == ConstraintType3D::Hinge)
			lambda = static_cast<const JPH::HingeConstraint*>(record.Constraint.GetPtr())->GetTotalLambdaMotor();
		else
		{
			const JPH::Vec3 total = static_cast<const JPH::SwingTwistConstraint*>(record.Constraint.GetPtr())->GetTotalLambdaMotor();
			lambda = axis == MotorAxis3D::Twist ? total.GetX() : (std::max)(std::abs(total.GetY()), std::abs(total.GetZ()));
		}
		return std::abs(lambda) * static_cast<float>(m_Data->LastCollisionSteps) / m_Data->LastDeltaTime;
	}

	bool JoltPhysics3D::RayCast(const Ray& ray, float maxDistance, RayCastHit3D& outHit, const QueryFilter3D& filter) const
	{
		if (!m_Data || maxDistance <= 0.0f)
			return false;

		// mDirection carries both direction AND length; scale the normalized ray by
		// maxDistance so the reported fraction is [0,1] over the query distance.
		const JPH::RRayCast joltRay(JPH::RVec3(ray.Origin.x, ray.Origin.y, ray.Origin.z), ToJolt(ray.Direction * maxDistance));

		JPH::RayCastResult hit;
		const QueryBodyFilter bodyFilter(filter);
		if (!m_Data->PhysicsSystem.GetNarrowPhaseQuery().CastRay(joltRay, hit, {}, {}, bodyFilter))
			return false;

		const glm::vec3 point = ToGlm(joltRay.GetPointOnRay(hit.mFraction));

		// The surface normal must be read from the hit body under a read lock.
		glm::vec3 normal(0.0f);
		{
			JPH::BodyLockRead lock(m_Data->PhysicsSystem.GetBodyLockInterface(), hit.mBodyID);
			if (lock.Succeeded())
				normal = ToGlm(lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, joltRay.GetPointOnRay(hit.mFraction)));
		}

		outHit.Body = hit.mBodyID.GetIndexAndSequenceNumber();
		outHit.Point = point;
		outHit.Normal = normal;
		outHit.Fraction = hit.mFraction;
		return true;
	}

	bool JoltPhysics3D::ShapeCastSphere(const glm::vec3& center, const glm::vec3& direction, float radius, float maxDistance, RayCastHit3D& outHit,
		const QueryFilter3D& filter) const
	{
		if (!m_Data || radius <= 0.0f || maxDistance <= 0.0f)
			return false;

		JPH::SphereShape sphere(radius);
		sphere.SetEmbedded(); // stack-allocated: don't let a Ref taken to it free it
		const JPH::RMat44 start = JPH::RMat44::sTranslation(JPH::RVec3(center.x, center.y, center.z));
		const JPH::Vec3 displacement = ToJolt(direction * maxDistance);
		const JPH::RShapeCast shapeCast(&sphere, JPH::Vec3::sReplicate(1.0f), start, displacement);

		JPH::ShapeCastSettings settings;
		JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
		// Return hits relative to world origin (base offset zero) so contact points are world-space.
		const QueryBodyFilter bodyFilter(filter);
		m_Data->PhysicsSystem.GetNarrowPhaseQuery().CastShape(shapeCast, settings, JPH::RVec3::sZero(), collector, {}, {}, bodyFilter);
		if (!collector.HadHit())
			return false;

		outHit.Body = collector.mHit.mBodyID2.GetIndexAndSequenceNumber();
		outHit.Point = ToGlm(collector.mHit.mContactPointOn2);
		outHit.Normal = ToGlm(-collector.mHit.mPenetrationAxis.NormalizedOr(JPH::Vec3::sZero())); // points from hit surface toward the cast shape
		outHit.Fraction = collector.mHit.mFraction;
		return true;
	}

	bool JoltPhysics3D::OverlapSphere(const glm::vec3& center, float radius, std::vector<PhysicsBodyId3D>& out, const QueryFilter3D& filter) const
	{
		out.clear();
		if (!m_Data || radius <= 0.0f)
			return false;

		JPH::SphereShape sphere(radius);
		sphere.SetEmbedded(); // stack-allocated: don't let a Ref taken to it free it
		const JPH::RMat44 transform = JPH::RMat44::sTranslation(JPH::RVec3(center.x, center.y, center.z));

		JPH::CollideShapeSettings settings;
		JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
		const QueryBodyFilter bodyFilter(filter);
		m_Data->PhysicsSystem.GetNarrowPhaseQuery().CollideShape(&sphere, JPH::Vec3::sReplicate(1.0f), transform, settings, JPH::RVec3::sZero(), collector, {}, {}, bodyFilter);

		for (const JPH::CollideShapeResult& hit : collector.mHits)
			out.push_back(hit.mBodyID2.GetIndexAndSequenceNumber());

		return !out.empty();
	}

	bool JoltPhysics3D::GetSensorOverlaps(PhysicsBodyId3D sensor, std::vector<PhysicsBodyId3D>& out) const
	{
		out.clear();
		if (!m_Data || sensor == k_InvalidBody3D)
			return false;

		// Copied out of the lock: the query below locks bodies itself.
		JPH::RefConst<JPH::Shape> shape;
		JPH::RMat44 transform;
		{
			JPH::BodyLockRead lock(m_Data->PhysicsSystem.GetBodyLockInterface(), ToBodyId(sensor));
			if (!lock.Succeeded() || !lock.GetBody().IsSensor())
				return false;
			shape = lock.GetBody().GetShape();
			transform = lock.GetBody().GetCenterOfMassTransform();
		}

		const QueryFilter3D filter;
		const QueryBodyFilter bodyFilter(filter, ToBodyId(sensor));
		JPH::CollideShapeSettings settings;
		JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
		m_Data->PhysicsSystem.GetNarrowPhaseQuery().CollideShape(shape.GetPtr(), JPH::Vec3::sReplicate(1.0f), transform, settings, JPH::RVec3::sZero(), collector, {}, {}, bodyFilter);

		for (const JPH::CollideShapeResult& hit : collector.mHits)
		{
			const PhysicsBodyId3D body = hit.mBodyID2.GetIndexAndSequenceNumber();
			if (std::find(out.begin(), out.end(), body) == out.end())
				out.push_back(body);
		}
		return !out.empty();
	}

	bool JoltPhysics3D::IsSensor(PhysicsBodyId3D body) const
	{
		if (!m_Data || body == k_InvalidBody3D)
			return false;

		JPH::BodyLockRead lock(m_Data->PhysicsSystem.GetBodyLockInterface(), ToBodyId(body));
		return lock.Succeeded() && lock.GetBody().IsSensor();
	}

	std::uint32_t JoltPhysics3D::GetUserData(PhysicsBodyId3D body) const
	{
		if (!m_Data || body == k_InvalidBody3D)
			return 0;

		JPH::BodyLockRead lock(m_Data->PhysicsSystem.GetBodyLockInterface(), ToBodyId(body));
		return lock.Succeeded() ? static_cast<std::uint32_t>(lock.GetBody().GetUserData()) : 0;
	}

	void JoltPhysics3D::SetUserData(PhysicsBodyId3D body, std::uint32_t userData)
	{
		if (!m_Data || body == k_InvalidBody3D)
			return;

		JPH::BodyLockWrite lock(m_Data->PhysicsSystem.GetBodyLockInterface(), ToBodyId(body));
		if (lock.Succeeded())
			lock.GetBody().SetUserData(PackUserData(QueryLayersOf(lock.GetBody()), userData));
	}

	std::unique_ptr<CharacterController3D> JoltPhysics3D::CreateCharacterController(const CharacterControllerParams3D& params)
	{
		if (!m_Data)
			return nullptr;
		return std::make_unique<Internal::JoltCharacterController3D>(m_Data, params);
	}

}
