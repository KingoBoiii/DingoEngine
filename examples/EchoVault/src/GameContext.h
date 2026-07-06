#pragma once
#include <DingoEngine.h>
#include <glm/glm.hpp>

#include <memory>

namespace Dingo
{
	// Shared state the course controller owns and the per-entity scripts read. Passed
	// to scripts by pointer at AddScript time, mirroring DungeonCrawler3D's GameContext.
	struct GameContext
	{
		// Built-in primitives (owned by Renderer3D, not us).
		Mesh* BoxMesh = nullptr;
		Mesh* SphereMesh = nullptr;

		// The player's character-controller entity, so orbs/sentries can find it.
		Entity Player;
		glm::vec3 SpawnPoint{ 0.0f };

		int Collected = 0;
		int TotalOrbs = 0;
		float ElapsedTime = 0.0f;

		// Decoded audio clips (owned here; shared into AudioSourceComponents and one-shots).
		std::shared_ptr<AudioClip> OrbClip;
		std::shared_ptr<AudioClip> HumClip;
		std::shared_ptr<AudioClip> AmbientClip;
		std::shared_ptr<AudioClip> FootstepClip;
		std::shared_ptr<AudioClip> PickupClip;
		std::shared_ptr<AudioClip> JumpClip;
		std::shared_ptr<AudioClip> DetectionClip;
	};
}
