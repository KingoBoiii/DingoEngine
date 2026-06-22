#include "GameScripts.h"
#include "GameTuning.h"

#include <glm/glm.hpp>

#include <algorithm>

namespace Dingo
{

	void PigScript::OnUpdate(float /*deltaTime*/)
	{
		if (m_Popped)
			return;

		Entity self = GetEntity();
		const glm::vec3 position = GetComponent<TransformComponent>().Position;

		// The pig is a live physics body: ask the scene for its simulated velocity.
		const glm::vec2 velocity = GetScene().GetLinearVelocity(self);
		const float speed = glm::length(velocity);

		const bool struckHard = speed > PIG_POP_SPEED; // hit by the bird or falling debris
		const bool knockedOff = position.y < KILL_Y;   // fell out of the world

		if (struckHard || knockedOff)
		{
			m_Popped = true;
			m_Context->PigsLeft = std::max(0, m_Context->PigsLeft - 1);
			m_Context->Score += PIG_POINTS;
			self.Destroy(); // deferred until the end of the scene update; also frees its body
		}
	}

}
