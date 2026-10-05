#pragma once
#include "Audio.h"
#include "Fighter.h"
#include "HitGeometry.h"
#include "Moveset.h"

#include <DingoEngine.h>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Dingo
{

	enum class Outcome : uint8_t
	{
		Hit,
		Blocked,
		Parry,
		Dodged
	};

	const char* ToString(Outcome outcome);

	struct OutcomeRecord
	{
		Outcome Kind = Outcome::Hit;
		std::string Attacker;
		std::string Target;
		std::string Move;
		bool Riposte = false;
		int ChainIndex = -1;
		// Where the attacker's clip stood, and the hitbox the contact has to fall in.
		float AttackTime = 0.0f;
		std::optional<ClipRange> Hitbox;
		bool InHitbox = false;
		// The defence behind a Parry, a Block or a Dodge: the target's clip, where it stood and its window.
		std::string DefenceClip;
		float DefenceTime = 0.0f;
		std::optional<ClipRange> Window;
		// A parry and a dodge fall inside their window; a block falls past the parry window, or has none.
		bool InWindow = false;
		// The parry stood on a raise the animator had not reported yet: it opens at the first frame of the raise.
		bool ParryPending = false;
		// The block was raised inside BLOCK_PARRY_COOLDOWN of the last one coming down: it has no parry window to fall in.
		bool ParryDenied = false;
		float Damage = 0.0f;
		float Health = 0.0f;

		bool IsValid() const { return InHitbox && (Kind == Outcome::Hit || InWindow); }
	};

	struct CombatStats
	{
		int Hits = 0;
		int Blocks = 0;
		int Parries = 0;
		int Dodges = 0;
		bool RiposteLanded = false;
		int BestChain = 0;
		bool AllInRange = true;
	};

	// Reads the poses the last frame left: the swept weapon spheres against the target's hurt spheres,
	// only while the attacker is in Attack with its hitbox open, once per swing.
	class Combat
	{
	public:
		Combat(const GameAudio& audio, bool log);

		void Update(Fighter& a, Fighter& b);
		void UpdateDebug(Fighter& a, Fighter& b);

		const CombatStats& GetStats() const { return m_Stats; }
		const std::vector<OutcomeRecord>& GetOutcomes() const { return m_Outcomes; }

	private:
		struct Decision
		{
			Fighter* Attacker = nullptr;
			Fighter* Target = nullptr;
			OutcomeRecord Record;
		};

		std::optional<Decision> Judge(Fighter& attacker, Fighter& target);
		void Commit(Decision& decision);
		void StartHitStop(Fighter& a, Fighter& b);
		void Track(const Decision& decision);
		void Log(const OutcomeRecord& record) const;

	private:
		const GameAudio& m_Audio;
		bool m_Log;
		CombatStats m_Stats;
		std::vector<OutcomeRecord> m_Outcomes;
		std::unordered_map<const Fighter*, int> m_ChainRuns;
		std::vector<WorldSphere> m_HurtScratch;
		std::vector<SweptSphere> m_SwingScratch;
	};

}
