#pragma once
#include <DingoEngine.h>

#include "GameContext.h"

// Gameplay behaviours live in ScriptableEntity subclasses, exactly as in the
// Space Invaders example. The structure blocks and the bird need no per-entity
// logic — they are pure rigid bodies driven by the engine's physics world — so
// the only behaviour here is the pig target.
namespace Dingo
{

	// A target pig. Pigs are ordinary dynamic circle bodies; this behaviour just
	// watches the body the engine simulates and "pops" it once it is struck hard
	// enough (a fast impact) or knocked out of the world. Popping decrements the
	// level's pig count and awards score via the shared GameContext.
	class PigScript : public ScriptableEntity
	{
	public:
		PigScript(GameContext* context) : m_Context(context) {}

	protected:
		void OnUpdate(float deltaTime) override;

	private:
		GameContext* m_Context = nullptr;
		bool m_Popped = false;
	};

}
