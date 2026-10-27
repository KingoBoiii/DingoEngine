#pragma once

// Engine-internal: this header lives under src/ and is NEVER shipped or included
// by client code. It is the only place the EnTT registry is named, which keeps
// EnTT a private implementation detail of the engine.

#include "DingoEngine/Core/UUID.h"
#include "DingoEngine/Graphics/Animator.h"
#include "DingoEngine/Graphics/Frustum.h"
#include "DingoEngine/Graphics/SkinnedVisibility.h"
#include "DingoEngine/Scene/Systems/AnimationSystem.h"
#include "DingoEngine/Scene/Systems/HierarchySystem.h"
#include "DingoEngine/Scene/Systems/LightSystem.h"
#include "DingoEngine/Scene/Systems/PhysicsInterpolation.h"
#include "DingoEngine/Scene/Systems/PhysicsSync.h"
#include "DingoEngine/Scene/Systems/ScriptSystem.h"
#include "DingoEngine/Scene/Systems/StaticGeometry.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Dingo
{

	namespace Internal
	{

		struct SceneData
		{
			entt::registry Registry;
			std::unordered_map<UUID, entt::entity> EntityMap;

			ScriptSystem Scripts{ Registry };
			PhysicsSync Physics;

			float FixedTimeStep = 1.0f / 60.0f;
			float FixedTimeAccumulator = 0.0f;

			// SceneParams::FixedTick only.
			std::uint64_t Tick = 0;
			std::uint32_t FixedStepsLastUpdate = 0;
			std::uint64_t DroppedFixedSteps = 0;
			PhysicsInterpolation Interpolation;

			// Deferred-destruction support: while scripts are updating we queue
			// destroys and apply them after the update pass, so a script can safely
			// destroy its own (or another) entity mid-update.
			bool Updating = false;
			std::vector<entt::entity> PendingDestroy;

			// Joined to SceneDebug's live-scene list (the F7 and F9 tabs) at the first OnUpdate, on the main thread, so a scene built on
			// a loading thread isn't read while it is filled.
			bool DebugRegistered = false;

			// Reused every frame by the 2D z-sort in RenderEntities: clear() keeps the
			// capacity, so a steady-state frame allocates nothing to sort.
			enum class Draw2DKind : std::uint8_t { Sprite, Circle, Text };
			struct Draw2D
			{
				glm::vec3 Position; // world; z is the sort key, then Kind, then Depth
				float Rotation;
				std::uint32_t Depth;
				entt::entity Entity;
				Draw2DKind Kind;
			};
			std::vector<Draw2D> Draw2DSortBuffer;

			// Scratch for the per-entity readers (rendering, lights, audio); reset by each pass.
			HierarchySystem::WorldMemo Memo;

			// RenderEntities3D's results from earlier frames, by entity index, reused only while what
			// they were worked out from comes back bit-identical: a root's world from its position,
			// rotation and scale, the world bounds from the world and the mesh's bounds. Each is a pure
			// function of those, so a reused value is the one the frame would have worked out.
			struct MeshRenderMemo
			{
				glm::mat4 World;
				glm::vec3 Position;
				glm::quat Rotation;
				glm::vec3 Scale;
				glm::vec3 MeshMin;
				glm::vec3 MeshMax;
				AABB Bounds;
				bool Root = false;      // World is the root's Position, Rotation and Scale
				bool HasBounds = false; // Bounds and Bounded are World's with MeshMin and MeshMax
				bool Bounded = false;
			};
			std::vector<MeshRenderMemo> MeshMemo;

			AnimationSystem::EventScratch AnimationEvents;

			LightSystem::ShadowProbeState ShadowProbes;

			StaticGeometry StaticMeshes;

			// The renderers RenderEntities3D drew with, which the animate pass asks what was culled.
			std::vector<std::weak_ptr<const SkinnedVisibility>> SkinnedViews;
			SkinnedViewStamp SkinnedViewFrame; // the frame of the last RenderEntities3D that drew
		};

	}

}
