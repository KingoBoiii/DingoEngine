#pragma once

// Engine-internal: render interpolation for SceneParams::FixedTick. After each tick it records the
// local pose physics wrote for every moving body and controller; while a scene is drawn (or
// heard) it puts the pose between the last two ticks into the transform components and then
// puts the latest tick back, so every world reader (children, lights, audio, sockets) follows
// the drawn pose and scripts only ever see the simulated one.

#include "DingoEngine/Scene/Systems/PhysicsSync.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>

namespace Dingo
{

	namespace Internal
	{

		class PhysicsInterpolation
		{
		public:
			// Before a tick's step: a pose changed since the last tick (the component written, the
			// body or controller moved) starts again from the next tick instead of sliding there.
			void BeforeTick(entt::registry& registry, const PhysicsSync& physics);
			// After the step's write-back: the previous pose becomes the one before, the written one current.
			void AfterTick(entt::registry& registry);
			// Forgets every recorded pose (physics stopped).
			void Clear(entt::registry& registry);

			// Nested calls are counted; only the outermost Begin writes and its End restores.
			void Begin(entt::registry& registry, const PhysicsSync& physics, float alpha);
			void End(entt::registry& registry);

		private:
			struct Saved3D
			{
				entt::entity Handle;
				glm::vec3 Position;
				glm::quat Rotation;
			};

			struct Saved2D
			{
				entt::entity Handle;
				glm::vec3 Position;
				float Rotation;
			};

			std::vector<Saved3D> m_Saved3D;
			std::vector<Saved2D> m_Saved2D;
			std::vector<entt::entity> m_Scratch;
			int m_Depth = 0;
		};

	}

}
