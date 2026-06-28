#pragma once
#include <DingoEngine.h>

#include "GameTuning.h"
#include "Character.h"

// Shared game state, owned by the DungeonControllerScript and handed (by pointer) to
// the per-entity scripts it spawns. This is the 3D crawler's equivalent of the 2D
// DungeonCrawler's GameContext — but it lives in the scene (on the controller entity)
// rather than in the layer, so the layer holds no game state at all.
namespace Dingo
{
	struct GameContext
	{
		enum class State { Playing, Won, Lost };

		State CurrentState = State::Playing;

		float PlayerHealth = PLAYER_MAX_HEALTH;
		int Collected = 0;
		int TotalTreasure = 0;
		int EnemiesSlain = 0;
		float ElapsedTime = 0.0f; // drives treasure spin / bob
		unsigned int Seed = 0;

		// Built-in unit meshes (1x1x1 box, unit-diameter sphere), filled by the
		// controller from Renderer3D so the scripts can spawn renderable entities.
		Mesh* BoxMesh = nullptr;
		Mesh* SphereMesh = nullptr;

		// Low-poly character part meshes loaded from OBJ models (assets/models/parts/)
		// by the controller; the player / enemy scripts assemble these into an animated
		// Character rig.
		CharacterMeshes CharParts;

		// The player entity, so enemies / camera / attack FX find it without a search.
		Entity Player;
	};
}
