#pragma once
#include <glm/glm.hpp>

namespace Dingo
{

	class Fighter;

	struct FighterIntent
	{
		// World ground axes (x, z). Its length is 0 to 1, 1 being a full run.
		glm::vec2 Move{ 0.0f };
		// False turns the fighter to its move direction even with an opponent in range.
		bool FaceOpponent = true;
		// Edges: true for the one frame the input went down.
		bool Light = false;
		bool Heavy = false;
		bool Dodge = false;
		// Held.
		bool Block = false;
	};

	// Writes one fighter's intent each frame, so the player and the AI share the Fighter's code.
	class Brain
	{
	public:
		virtual ~Brain() = default;
		virtual FighterIntent Think(float deltaTime, const Fighter& self, const Fighter& opponent) = 0;
		// A clip's events were replaced while the bout ran, so whatever the brain remembers of them is stale.
		virtual void OnEventsChanged() {}
	};

}
