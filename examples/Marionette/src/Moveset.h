#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Dingo
{

	class AnimationClip;
	class Model;

	namespace Clips
	{
		inline constexpr const char* IDLE            = "Idle_A";
		inline constexpr const char* WALK            = "Walking_A";
		inline constexpr const char* RUN             = "Running_A";
		inline constexpr const char* BACKWARDS       = "Walking_Backwards";
		inline constexpr const char* STRAFE_LEFT     = "Running_Strafe_Left";
		inline constexpr const char* STRAFE_RIGHT    = "Running_Strafe_Right";
		inline constexpr const char* DODGE_FORWARD   = "Dodge_Forward";
		inline constexpr const char* DODGE_BACKWARD  = "Dodge_Backward";
		inline constexpr const char* DODGE_LEFT      = "Dodge_Left";
		inline constexpr const char* DODGE_RIGHT     = "Dodge_Right";
		inline constexpr const char* HIT_REACT       = "Hit_A";
		inline constexpr const char* STAGGER         = "Hit_B";
		inline constexpr const char* BLOCK_RAISE     = "Melee_Block";
		inline constexpr const char* BLOCKING        = "Melee_Blocking";
		inline constexpr const char* BLOCK_HIT       = "Melee_Block_Hit";
		inline constexpr const char* BLOCK_RIPOSTE   = "Melee_Block_Attack";
	}

	namespace Events
	{
		inline constexpr const char* STEP_LEFT       = "step_l";
		inline constexpr const char* STEP_RIGHT      = "step_r";
	}

	namespace Joints
	{
		inline constexpr const char* HIPS            = "hips";
		inline constexpr const char* FOOT_LEFT       = "foot.l";
		inline constexpr const char* FOOT_RIGHT      = "foot.r";
		inline constexpr const char* HAND_RIGHT      = "handslot.r";
		inline constexpr const char* HAND_LEFT       = "handslot.l";
	}

	enum class FighterId : uint8_t
	{
		Knight,
		Minion,
		Barbarian,
		Warrior
	};

	struct FighterDef
	{
		FighterId Id;
		const char* Name;
		const char* Model;
		const char* Texture;
		float Scale;
		float Pace;
		const char* RightWeapon;
		const char* LeftWeapon;
		std::span<const char* const> LightChain;
		const char* Heavy;
		const char* Death;
		const char* Intro;
	};

	struct LibraryDef
	{
		const char* Label;
		const char* Path;
	};

	std::span<const FighterDef> GetFighterDefs();
	const FighterDef& GetPlayerDef();
	const FighterDef& GetOpponentDef(int bout);
	std::span<const LibraryDef> GetLibraryDefs();

	std::vector<std::string_view> GetUsedClipNames();

	class ClipSet
	{
	public:
		struct Entry
		{
			std::string Name;
			const AnimationClip* Clip = nullptr;
			size_t Library = 0;
		};

		const AnimationClip* Find(std::string_view name) const;
		const Entry* FindEntry(std::string_view name) const;

		const std::vector<Entry>& GetEntries() const { return m_Entries; }
		const std::vector<std::string>& GetMissing() const { return m_Missing; }

	private:
		friend ClipSet ResolveClips(std::span<Model* const> libraries);

		std::vector<Entry> m_Entries;
		std::vector<std::string> m_Missing;
	};

	ClipSet ResolveClips(std::span<Model* const> libraries);

}
