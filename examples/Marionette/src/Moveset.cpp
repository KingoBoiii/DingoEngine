#include "Moveset.h"
#include "GameTuning.h"

#include <DingoEngine.h>

#include <algorithm>
#include <array>
#include <format>
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
		{ FighterId::Knight, "Knight", "characters/Knight.glb", "characters/knight_texture.png", SCALE_KNIGHT, PACE_KNIGHT,
			"weapons/sword_1handed.gltf", "weapons/shield_round.gltf",
			k_KnightChain, "Melee_1H_Attack_Jump_Chop", "Death_A", nullptr },
		{ FighterId::Minion, "Skeleton Minion", "characters/Skeleton_Minion.glb", "characters/skeleton_texture.png", SCALE_MINION, PACE_MINION,
			"weapons/Skeleton_Blade.gltf", nullptr,
			k_MinionChain, nullptr, "Skeletons_Death", "Skeletons_Taunt" },
		{ FighterId::Barbarian, "Barbarian", "characters/Barbarian.glb", "characters/barbarian_texture.png", SCALE_BARBARIAN, PACE_BARBARIAN,
			"weapons/axe_2handed.gltf", nullptr,
			k_BarbarianChain, "Melee_2H_Attack_Spin", "Death_B", "Skeletons_Taunt_Longer" },
		{ FighterId::Warrior, "Skeleton Warrior", "characters/Skeleton_Warrior.glb", "characters/skeleton_texture.png", SCALE_WARRIOR, PACE_WARRIOR,
			"weapons/Skeleton_Axe.gltf", "weapons/Skeleton_Shield_Small_A.gltf",
			k_WarriorChain, "Melee_1H_Attack_Jump_Chop", "Skeletons_Death", "Skeletons_Taunt" },
	} };

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

	std::vector<std::string_view> GetUsedClipNames()
	{
		std::vector<std::string_view> names;
		for (const char* name : { Clips::IDLE, Clips::WALK, Clips::RUN, Clips::BACKWARDS, Clips::STRAFE_LEFT, Clips::STRAFE_RIGHT,
			Clips::DODGE_FORWARD, Clips::DODGE_BACKWARD, Clips::DODGE_LEFT, Clips::DODGE_RIGHT, Clips::HIT_REACT, Clips::STAGGER,
			Clips::BLOCK_RAISE, Clips::BLOCKING, Clips::BLOCK_HIT, Clips::BLOCK_RIPOSTE })
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
