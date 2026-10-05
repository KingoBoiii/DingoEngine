#pragma once
#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Dingo
{

	class AnimationClip;
	class ClipSet;
	class Model;
	class Skeleton;

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
		inline constexpr const char* TAUNT           = "Skeletons_Taunt";
	}

	namespace Events
	{
		inline constexpr const char* STEP_LEFT       = "step_l";
		inline constexpr const char* STEP_RIGHT      = "step_r";
		inline constexpr const char* WINDUP          = "windup";
		inline constexpr const char* HITBOX          = "hitbox";
		inline constexpr const char* DASH            = "dash";
		inline constexpr const char* COMBO           = "combo";
		inline constexpr const char* IFRAMES         = "iframes";
		inline constexpr const char* PARRY           = "parry";
	}

	namespace Joints
	{
		inline constexpr const char* HIPS            = "hips";
		inline constexpr const char* SPINE           = "spine";
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
		float Health;
		const char* RightWeapon;
		const char* LeftWeapon;
		std::span<const char* const> LightChain;
		const char* Heavy;
		const char* Death;
		const char* Intro;
		// What the HUD calls an opponent; null for the player.
		const char* Title;
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

	enum class MoveKind : uint8_t
	{
		Light,
		Heavy,
		Riposte
	};

	struct MoveDef
	{
		const char* Clip;
		MoveKind Kind;
		float Damage;
		float FadeIn;
		float FadeOut;
		// Degrees right of the facing at which the blade crosses the line to a target dead ahead.
		float AimDegrees = 0.0f;
	};

	std::span<const MoveDef> GetMoves();
	const MoveDef* FindMove(std::string_view clip);
	const MoveDef& GetRiposteMove();
	const MoveDef* GetHeavyMove(const FighterDef& fighter);
	// The light attack that follows `clip` in this fighter's chain; null at the end of it or off it.
	const MoveDef* GetNextInChain(const FighterDef& fighter, std::string_view clip);
	// Position of `clip` in the fighter's light chain, or -1.
	int FindChainIndex(const FighterDef& fighter, std::string_view clip);
	// True when some fighter's chain continues after `clip`, so it needs a `combo` window.
	bool IsChainLink(std::string_view clip);

	struct ClipRange
	{
		float Begin = 0.0f;
		float End = 0.0f;

		bool Contains(float time) const { return time >= Begin && time <= End; }
	};

	// The first range event of that name on the clip, as the sidecar (or code) gave it.
	std::optional<ClipRange> FindRange(const AnimationClip& clip, std::string_view name);

	// How far the hips move on the ground between two clip times, in the model space of `skeleton` with the
	// clip retargeted onto it: what the pose shows. A clip that moves its root carries the travel there, and
	// the hips' own motion under it is dropped. +z is forward, +x the left.
	glm::vec2 PoseHipsTravel(const Skeleton& skeleton, const AnimationClip& clip, float begin, float end);

	// The same up to where the one-shot starts returning.
	glm::vec2 NetHipsTravel(const Skeleton& skeleton, const AnimationClip& clip, float fadeOut);

	// Warns once per problem: a combat window that ends after its one-shot starts returning (it would
	// never fire), a move missing the windows its rules need, and a combo that opens before the hitbox closes.
	// Returns the number of problems.
	int ValidateMoveset(const ClipSet& clips);

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
