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

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

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

	bool JoltPhysics3D::RayCast(const Ray& ray, float maxDistance, RayCastHit3D& outHit) const
	{
		if (!m_Data || maxDistance <= 0.0f)
			return false;

		// mDirection carries both direction AND length; scale the normalized ray by
		// maxDistance so the reported fraction is [0,1] over the query distance.
		const JPH::RRayCast joltRay(JPH::RVec3(ray.Origin.x, ray.Origin.y, ray.Origin.z), ToJolt(ray.Direction * maxDistance));

		JPH::RayCastResult hit;
		if (!m_Data->PhysicsSystem.GetNarrowPhaseQuery().CastRay(joltRay, hit))
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

	bool JoltPhysics3D::ShapeCastSphere(const glm::vec3& center, const glm::vec3& direction, float radius, float maxDistance, RayCastHit3D& outHit) const
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
		m_Data->PhysicsSystem.GetNarrowPhaseQuery().CastShape(shapeCast, settings, JPH::RVec3::sZero(), collector);
		if (!collector.HadHit())
			return false;

		outHit.Body = collector.mHit.mBodyID2.GetIndexAndSequenceNumber();
		outHit.Point = ToGlm(collector.mHit.mContactPointOn2);
		outHit.Normal = ToGlm(-collector.mHit.mPenetrationAxis.NormalizedOr(JPH::Vec3::sZero())); // points from hit surface toward the cast shape
		outHit.Fraction = collector.mHit.mFraction;
		return true;
	}

	bool JoltPhysics3D::OverlapSphere(const glm::vec3& center, float radius, std::vector<PhysicsBodyId3D>& out) const
	{
		out.clear();
		if (!m_Data || radius <= 0.0f)
			return false;

		JPH::SphereShape sphere(radius);
		sphere.SetEmbedded(); // stack-allocated: don't let a Ref taken to it free it
		const JPH::RMat44 transform = JPH::RMat44::sTranslation(JPH::RVec3(center.x, center.y, center.z));

		JPH::CollideShapeSettings settings;
		JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
		m_Data->PhysicsSystem.GetNarrowPhaseQuery().CollideShape(&sphere, JPH::Vec3::sReplicate(1.0f), transform, settings, JPH::RVec3::sZero(), collector);

		for (const JPH::CollideShapeResult& hit : collector.mHits)
			out.push_back(hit.mBodyID2.GetIndexAndSequenceNumber());

		return !out.empty();
	}

	std::unique_ptr<CharacterController3D> JoltPhysics3D::CreateCharacterController(const CharacterControllerParams3D& params)
	{
		if (!m_Data)
			return nullptr;
		return std::make_unique<Internal::JoltCharacterController3D>(m_Data, params);
	}

}
