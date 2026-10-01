#pragma once
#include "BrazierPrompt.h"
#include "KeepMap.h"

#include <DingoEngine.h>

#include <glm/glm.hpp>

#include <cstddef>
#include <optional>
#include <vector>

namespace Dingo
{

	class GameAudio;
	class KeepWorld;
	class Lantern;
	class LightLod;
	class Player;

	// Lighting the keep's braziers: the hold that kindles one, the checkpoint it saves, and the
	// flicker of everything that burns. Every brazier starts dark except the Gatehouse's.
	class Braziers
	{
	public:
		struct Checkpoint
		{
			glm::ivec2 Tile{ 0 };
			float Oil = 0.0f;
		};

		// The first checkpoint is the Gatehouse brazier's, or `startTile` when a debug flag moved the start.
		Braziers(Scene& scene, const KeepMap& map, KeepWorld& world, LightLod& lightLod, GameAudio& audio, float startOil,
			const std::optional<glm::ivec2>& startTile, bool flicker, bool allLit);

		Braziers(const Braziers&) = delete;
		Braziers& operator=(const Braziers&) = delete;

		// Returns the brazier lit this frame, if the player finished a hold. Flicker runs regardless of
		// `canLight`, which the caller clears while the player is caught or the run is won.
		std::optional<size_t> Update(float deltaTime, const Player& player, Lantern& lantern, bool canLight);

		const Checkpoint& GetCheckpoint() const { return m_Checkpoint; }

		BrazierPrompt GetPrompt() const { return m_Prompt; }
		float GetProgress() const { return m_Progress; }

	private:
		void Light(size_t index);
		void Flicker();

	private:
		Scene& m_Scene;
		const KeepMap& m_Map;
		KeepWorld& m_World;
		LightLod& m_LightLod;
		GameAudio& m_Audio;
		bool m_Flicker = true;

		std::vector<bool> m_Lit;
		std::vector<float> m_BaseIntensity;
		Checkpoint m_Checkpoint;

		float m_Clock = 0.0f;
		BrazierPrompt m_Prompt = BrazierPrompt::None;
		float m_Progress = 0.0f;
		bool m_Holding = false;
		std::optional<size_t> m_Target;
	};

}
