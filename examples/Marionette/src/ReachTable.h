#pragma once
#include "Moveset.h"

#include <glm/glm.hpp>

#include <array>
#include <vector>

namespace Dingo
{

	class GameAssets;

	struct MoveReach
	{
		float Min = 0.0f;
		float Max = 0.0f;
		bool Valid = false;
		// Clip seconds after the hitbox opens at which the swing first touches, for each REACH_SCAN_STEP
		// from REACH_SCAN_MIN; negative where it never does. A long hitbox can touch late in it.
		std::vector<float> ContactDelay;

		float DelayAt(float distance) const;
	};

	// How far each move reaches: its weapon spheres swept through the clip's hitbox against the hurt spheres of a
	// target standing still and facing the attacker, which is the geometry Combat tests. Rows fill in on first use.
	class ReachTable
	{
	public:
		explicit ReachTable(const GameAssets& assets);

		// Centre-to-centre ground distances at which the swing touches.
		const MoveReach& Get(const FighterDef& attacker, const MoveDef& move, const FighterDef& target) const;

		// How long `Melee_Block`'s parry window lasts from the raise, as its clip says now (so a live edit counts).
		float GetParryWindow() const;

		void Log(const FighterDef& attacker, const FighterDef& target) const;

		// Forgets every row, so the next question re-reads the clips' events: after a hot-reload.
		void Invalidate();

	private:
		struct Sweep
		{
			// Model space, before the attacker's scale: the three weapon spheres at each sample of the hitbox.
			std::vector<std::array<glm::vec3, 3>> Centers;
			// Clip seconds after the hitbox opens, per sample.
			std::vector<float> Delays;
			float Radius = 0.0f;
			bool Computed = false;
			bool Valid = false;
		};

		struct Hurt
		{
			glm::vec3 Center{ 0.0f };
			float Radius = 0.0f;
		};

		size_t SweepIndex(const FighterDef& attacker, size_t move) const;
		size_t ReachIndex(const FighterDef& attacker, size_t move, const FighterDef& target) const;
		const Sweep& GetSweep(const FighterDef& attacker, size_t move) const;
		std::vector<Hurt> GetHurt(const FighterDef& target) const;
		MoveReach Scan(const Sweep& sweep, const FighterDef& attacker, const MoveDef& move, const FighterDef& target) const;

	private:
		const GameAssets& m_Assets;
		size_t m_FighterCount;
		size_t m_MoveCount;
		mutable std::vector<Sweep> m_Sweeps;
		mutable std::vector<MoveReach> m_Reach;
		mutable std::vector<bool> m_ReachDone;
		mutable std::vector<bool> m_Logged;
	};

}
