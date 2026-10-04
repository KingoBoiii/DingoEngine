#include "Moveset.h"
#include "GameTuning.h"

#include <DingoEngine.h>

#include <algorithm>
#include <array>
#include <format>
#include <initializer_list>
#include <string>
#include <vector>

namespace
{
	using namespace Dingo;

	constexpr std::array<const char*, 3> k_KnightChain = {
		"Melee_1H_Attack_Slice_Diagonal", "Melee_1H_Attack_Slice_Horizontal", "Melee_1H_Attack_Chop" };
	constexpr std::array<const char*, 1> k_MinionChain = { "Melee_1H_Attack_Slice_Diagonal" };
	constexpr std::array<const char*, 2> k_BarbarianChain = { "Melee_2H_Attack_Slice", "Melee_2H_Attack_Chop" };
	constexpr std::array<const char*, 3> k_WarriorChain = {
		"Melee_1H_Attack_Slice_Diagonal", "Melee_1H_Attack_Slice_Horizontal", "Melee_1H_Attack_Stab" };

	const std::array<FighterDef, 4> k_Fighters = { {
		{ FighterId::Knight, "Knight", "characters/Knight.glb", "characters/knight_texture.png", SCALE_KNIGHT, PACE_KNIGHT, HEALTH_KNIGHT,
			"weapons/sword_1handed.gltf", "weapons/shield_round.gltf",
			k_KnightChain, "Melee_1H_Attack_Jump_Chop", "Death_A", nullptr, nullptr },
		{ FighterId::Minion, "Skeleton Minion", "characters/Skeleton_Minion.glb", "characters/skeleton_texture.png", SCALE_MINION, PACE_MINION, HEALTH_MINION,
			"weapons/Skeleton_Blade.gltf", nullptr,
			k_MinionChain, nullptr, "Skeletons_Death", "Skeletons_Taunt", "The Recruit" },
		{ FighterId::Barbarian, "Barbarian", "characters/Barbarian.glb", "characters/barbarian_texture.png", SCALE_BARBARIAN, PACE_BARBARIAN, HEALTH_BARBARIAN,
			"weapons/axe_2handed.gltf", nullptr,
			k_BarbarianChain, "Melee_2H_Attack_Spin", "Death_B", "Skeletons_Taunt_Longer", "The Veteran" },
		{ FighterId::Warrior, "Skeleton Warrior", "characters/Skeleton_Warrior.glb", "characters/skeleton_texture.png", SCALE_WARRIOR, PACE_WARRIOR, HEALTH_WARRIOR,
			"weapons/Skeleton_Axe.gltf", "weapons/Skeleton_Shield_Small_A.gltf",
			k_WarriorChain, "Melee_1H_Attack_Jump_Chop", "Skeletons_Death", "Skeletons_Taunt", "The Champion" },
	} };

	const std::array<MoveDef, 9> k_Moves = { {
		{ "Melee_1H_Attack_Slice_Diagonal",   MoveKind::Light,   DAMAGE_LIGHT_1H,             ATTACK_FADE_IN, ATTACK_FADE_OUT },
		{ "Melee_1H_Attack_Slice_Horizontal", MoveKind::Light,   DAMAGE_LIGHT_1H,             ATTACK_FADE_IN, ATTACK_FADE_OUT },
		{ "Melee_1H_Attack_Chop",             MoveKind::Light,   DAMAGE_LIGHT_FINISHER_1H,    ATTACK_FADE_IN, ATTACK_FADE_OUT, AIM_CHOP_DEG },
		{ "Melee_1H_Attack_Stab",             MoveKind::Light,   DAMAGE_LIGHT_FINISHER_1H,    ATTACK_FADE_IN, ATTACK_FADE_OUT },
		{ "Melee_2H_Attack_Slice",            MoveKind::Light,   DAMAGE_LIGHT_2H,             ATTACK_FADE_IN, ATTACK_FADE_OUT },
		{ "Melee_2H_Attack_Chop",             MoveKind::Light,   DAMAGE_LIGHT_FINISHER_2H,    ATTACK_FADE_IN, ATTACK_FADE_OUT },
		{ "Melee_1H_Attack_Jump_Chop",        MoveKind::Heavy,   DAMAGE_HEAVY_1H,             ATTACK_FADE_IN, ATTACK_FADE_OUT },
		{ "Melee_2H_Attack_Spin",             MoveKind::Heavy,   DAMAGE_HEAVY_2H,             ATTACK_FADE_IN, ATTACK_FADE_OUT },
		{ "Melee_Block_Attack",               MoveKind::Riposte, DAMAGE_RIPOSTE,              ATTACK_FADE_IN, ATTACK_FADE_OUT },
	} };

	constexpr std::array<const char*, 4> k_DodgeClips = {
		Clips::DODGE_FORWARD, Clips::DODGE_BACKWARD, Clips::DODGE_LEFT, Clips::DODGE_RIGHT };

	// The dash is not listed: the Fighter reads its range from the clip, so it may run on into the fade-out.
	constexpr std::array<const char*, 5> k_Windows = {
		Events::WINDUP, Events::HITBOX, Events::COMBO, Events::IFRAMES, Events::PARRY };

	bool IsWindowName(std::string_view name)
	{
		return std::find(k_Windows.begin(), k_Windows.end(), name) != k_Windows.end();
	}

	constexpr float k_EndSlack = 1.0e-4f;

	int WarnTail(const AnimationClip& clip, float fadeOut)
	{
		int problems = 0;
		const float limit = clip.GetDuration() - fadeOut;
		for (const AnimationClipEvent& event : clip.GetEvents())
		{
			if (!event.Range || !IsWindowName(event.Name) || event.EndTime <= limit + 1.0e-4f)
				continue;

			DE_WARN("Marionette: {}: {} {:.2f}..{:.2f} ends after {:.2f} s, where its one-shot starts returning ({:.2f} s less its {:.2f} s fade-out), so it never fires",
				clip.GetName(), event.Name, event.Time, event.EndTime, limit, clip.GetDuration(), fadeOut);
			++problems;
		}
		return problems;
	}

	int WarnMissing(const AnimationClip& clip, std::initializer_list<const char*> names)
	{
		int problems = 0;
		for (const char* name : names)
		{
			if (FindRange(clip, name))
				continue;

			DE_WARN("Marionette: {}: no '{}' range in its .events, so the rules that read it have nothing to go on", clip.GetName(), name);
			++problems;
		}
		return problems;
	}

	const std::array<LibraryDef, 5> k_Libraries = { {
		{ "General", "animations/Rig_Medium_General.glb" },
		{ "MovementBasic", "animations/Rig_Medium_MovementBasic.glb" },
		{ "MovementAdvanced", "animations/Rig_Medium_MovementAdvanced.glb" },
		{ "CombatMelee", "animations/Rig_Medium_CombatMelee.glb" },
		{ "Special", "animations/Rig_Medium_Special.glb" },
	} };

	const char* LibraryLabel(size_t index)
	{
		return index < k_Libraries.size() ? k_Libraries[index].Label : "?";
	}

	void AddUnique(std::vector<std::string_view>& names, const char* name)
	{
		if (name && std::find(names.begin(), names.end(), std::string_view(name)) == names.end())
			names.emplace_back(name);
	}
}

namespace Dingo
{

	std::span<const FighterDef> GetFighterDefs()
	{
		return k_Fighters;
	}

	std::span<const LibraryDef> GetLibraryDefs()
	{
		return k_Libraries;
	}

	const FighterDef& GetPlayerDef()
	{
		return k_Fighters[static_cast<size_t>(FighterId::Knight)];
	}

	const FighterDef& GetOpponentDef(int bout)
	{
		constexpr std::array<FighterId, 3> order = { FighterId::Minion, FighterId::Barbarian, FighterId::Warrior };
		return k_Fighters[static_cast<size_t>(order[static_cast<size_t>(std::clamp(bout, 1, 3) - 1)])];
	}

	std::vector<std::string_view> GetUsedClipNames()
	{
		std::vector<std::string_view> names;
		for (const char* name : { Clips::IDLE, Clips::WALK, Clips::RUN, Clips::BACKWARDS, Clips::STRAFE_LEFT, Clips::STRAFE_RIGHT,
			Clips::DODGE_FORWARD, Clips::DODGE_BACKWARD, Clips::DODGE_LEFT, Clips::DODGE_RIGHT, Clips::HIT_REACT, Clips::STAGGER,
			Clips::BLOCK_RAISE, Clips::BLOCKING, Clips::BLOCK_HIT, Clips::BLOCK_RIPOSTE, Clips::TAUNT })
			AddUnique(names, name);

		for (const FighterDef& fighter : k_Fighters)
		{
			for (const char* name : fighter.LightChain)
				AddUnique(names, name);
			AddUnique(names, fighter.Heavy);
			AddUnique(names, fighter.Death);
			AddUnique(names, fighter.Intro);
		}
		return names;
	}

	std::span<const MoveDef> GetMoves()
	{
		return k_Moves;
	}

	const MoveDef* FindMove(std::string_view clip)
	{
		const auto it = std::find_if(k_Moves.begin(), k_Moves.end(), [&](const MoveDef& move) { return clip == move.Clip; });
		return it != k_Moves.end() ? &*it : nullptr;
	}

	const MoveDef& GetRiposteMove()
	{
		return *FindMove(Clips::BLOCK_RIPOSTE);
	}

	const MoveDef* GetHeavyMove(const FighterDef& fighter)
	{
		return fighter.Heavy ? FindMove(fighter.Heavy) : nullptr;
	}

	int FindChainIndex(const FighterDef& fighter, std::string_view clip)
	{
		for (size_t i = 0; i < fighter.LightChain.size(); ++i)
		{
			if (clip == fighter.LightChain[i])
				return static_cast<int>(i);
		}
		return -1;
	}

	const MoveDef* GetNextInChain(const FighterDef& fighter, std::string_view clip)
	{
		const int index = FindChainIndex(fighter, clip);
		if (index < 0 || static_cast<size_t>(index) + 1 >= fighter.LightChain.size())
			return nullptr;
		return FindMove(fighter.LightChain[static_cast<size_t>(index) + 1]);
	}

	bool IsChainLink(std::string_view clip)
	{
		for (const FighterDef& fighter : k_Fighters)
		{
			if (GetNextInChain(fighter, clip))
				return true;
		}
		return false;
	}

	std::optional<ClipRange> FindRange(const AnimationClip& clip, std::string_view name)
	{
		for (const AnimationClipEvent& event : clip.GetEvents())
		{
			if (event.Range && event.Name == name)
				return ClipRange{ event.Time, event.EndTime };
		}
		return std::nullopt;
	}

	glm::vec2 PoseHipsTravel(const Skeleton& skeleton, const AnimationClip& clip, float begin, float end)
	{
		const int32_t hips = skeleton.FindJoint(Joints::HIPS);
		if (hips == Skeleton::k_InvalidJoint)
			return glm::vec2(0.0f);

		Animator animator(&skeleton);
		animator.Play(&clip);
		const float last = std::max(clip.GetDuration() - k_EndSlack, 0.0f);
		auto at = [&](float time)
		{
			animator.SetTime(std::min(time, last));
			animator.Evaluate();
			const glm::vec4 point = animator.GetJointTransform(hips)[3];
			return glm::vec2(point.x, point.z);
		};
		const glm::vec2 from = at(begin);
		return at(end) - from;
	}

	glm::vec2 NetHipsTravel(const Skeleton& skeleton, const AnimationClip& clip, float fadeOut)
	{
		return PoseHipsTravel(skeleton, clip, 0.0f, std::max(clip.GetDuration() - fadeOut, 0.0f));
	}

	int ValidateMoveset(const ClipSet& clips)
	{
		int problems = 0;
		for (const MoveDef& move : k_Moves)
		{
			const AnimationClip* clip = clips.Find(move.Clip);
			if (!clip)
				continue;

			problems += WarnTail(*clip, move.FadeOut);
			problems += WarnMissing(*clip, { Events::WINDUP, Events::HITBOX });
			if (IsChainLink(move.Clip))
				problems += WarnMissing(*clip, { Events::COMBO });
		}

		for (const char* name : k_DodgeClips)
		{
			const AnimationClip* clip = clips.Find(name);
			if (!clip)
				continue;

			problems += WarnTail(*clip, DODGE_FADE_OUT);
			problems += WarnMissing(*clip, { Events::DASH, Events::IFRAMES });
		}

		if (const AnimationClip* raise = clips.Find(Clips::BLOCK_RAISE))
			problems += WarnMissing(*raise, { Events::PARRY });
		return problems;
	}

	const AnimationClip* ClipSet::Find(std::string_view name) const
	{
		const Entry* entry = FindEntry(name);
		return entry ? entry->Clip : nullptr;
	}

	const ClipSet::Entry* ClipSet::FindEntry(std::string_view name) const
	{
		const auto it = std::find_if(m_Entries.begin(), m_Entries.end(), [&](const Entry& entry) { return entry.Name == name; });
		return it != m_Entries.end() ? &*it : nullptr;
	}

	ClipSet ResolveClips(std::span<Model* const> libraries)
	{
		ClipSet set;
		for (const std::string_view name : GetUsedClipNames())
		{
			std::vector<size_t> hits;
			const AnimationClip* chosen = nullptr;
			for (size_t i = 0; i < libraries.size(); ++i)
			{
				const AnimationClip* clip = libraries[i] ? libraries[i]->FindAnimation(name) : nullptr;
				if (!clip)
					continue;
				if (hits.empty())
					chosen = clip;
				hits.push_back(i);
			}

			if (hits.empty())
			{
				set.m_Missing.emplace_back(name);
				DE_ERROR("Marionette: clip '{}' is in none of the {} libraries", name, libraries.size());
				continue;
			}

			set.m_Entries.push_back({ std::string(name), chosen, hits.front() });
			if (hits.size() > 1)
			{
				std::string where;
				for (const size_t index : hits)
					where += std::format("{}{}", where.empty() ? "" : ", ", LibraryLabel(index));
				DE_WARN("Marionette: clip '{}' is in {} libraries ({}); using {}", name, hits.size(), where, LibraryLabel(hits.front()));
			}
		}
		return set;
	}

}
