#pragma once

// Engine-internal: this header lives under src/ and is NEVER shipped or included
// by client code. It is the only place Jolt is named, keeping it a private
// implementation detail of the engine (the same treatment as EnTT/Box2D).

// depch.h (the PCH) pulls in <Windows.h>, whose min/max macros would break Jolt's
// headers. Drop them before including any Jolt header.
#ifdef min
	#undef min
#endif
#ifdef max
	#undef max
#endif

// Jolt.h must be included before any other Jolt header.
#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

#include <algorithm>
#include <thread>

namespace Dingo::Internal
{

	// Two object layers: static (non-moving) and dynamic/kinematic (moving). This is
	// the standard minimal Jolt setup from its HelloWorld sample.
	namespace Layers
	{
		static constexpr JPH::ObjectLayer NON_MOVING = 0;
		static constexpr JPH::ObjectLayer MOVING = 1;
		static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
	}

	namespace BroadPhaseLayers
	{
		static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
		static constexpr JPH::BroadPhaseLayer MOVING(1);
		static constexpr JPH::uint NUM_LAYERS(2);
	}

	class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
	{
	public:
		virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
		{
			switch (inObject1)
			{
				case Layers::NON_MOVING: return inObject2 == Layers::MOVING; // static only vs moving
				case Layers::MOVING:     return true;                        // moving vs everything
				default:                 return false;
			}
		}
	};

	class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
	{
	public:
		BPLayerInterfaceImpl()
		{
			m_ObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
			m_ObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
		}

		virtual JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }

		virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
		{
			return m_ObjectToBroadPhase[inLayer];
		}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer) const override { return "Layer"; }
#endif

	private:
		JPH::BroadPhaseLayer m_ObjectToBroadPhase[Layers::NUM_LAYERS];
	};

	class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
		{
			switch (inLayer1)
			{
				case Layers::NON_MOVING: return inLayer2 == BroadPhaseLayers::MOVING;
				case Layers::MOVING:     return true;
				default:                 return false;
			}
		}
	};

	// Holds the whole Jolt world for one PhysicsWorld3D. Declaration order matters:
	// the layer interfaces are referenced by PhysicsSystem, so PhysicsSystem is the
	// last member (destroyed first).
	struct Physics3DData
	{
		BPLayerInterfaceImpl BroadPhaseLayerInterface;
		ObjectVsBroadPhaseLayerFilterImpl ObjectVsBroadPhaseLayerFilter;
		ObjectLayerPairFilterImpl ObjectLayerPairFilter;

		JPH::TempAllocatorImpl TempAllocator;
		JPH::JobSystemThreadPool JobSystem;
		JPH::PhysicsSystem PhysicsSystem;

		explicit Physics3DData(JPH::uint maxBodies)
			: TempAllocator(32 * 1024 * 1024) // per-Update working memory; must cover the limits below
		{
			const int threads = (int)std::max(1u, std::thread::hardware_concurrency() - 1u);
			JobSystem.Init(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, threads);

			// maxBodyPairs / maxContactConstraints are Jolt's documented "real project"
			// values and are independent of maxBodies. The contact-constraint buffer
			// dominates the temp-allocator footprint (~480 bytes each), so keep it sane.
			constexpr JPH::uint k_MaxBodyPairs = 65536;
			constexpr JPH::uint k_MaxContactConstraints = 10240;

			PhysicsSystem.Init(maxBodies, /*numBodyMutexes*/ 0, k_MaxBodyPairs, k_MaxContactConstraints,
				BroadPhaseLayerInterface, ObjectVsBroadPhaseLayerFilter, ObjectLayerPairFilter);
		}
	};

}
