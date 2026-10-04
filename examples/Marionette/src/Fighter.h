#pragma once
#include "Audio.h"
#include "FighterIntent.h"
#include "GameAssets.h"
#include "GameTuning.h"
#include "HitGeometry.h"
#include "Locomotion.h"
#include "Moveset.h"

#include <DingoEngine.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Dingo
{

	struct FighterContext
	{
		Scene& World;
		const GameAssets& Assets;
		const GameAudio& Audio;
		// The director's clock, which timestamps the footsteps.
		const double& Time;
		bool LogSteps = false;
		bool LogCombat = false;
		const HitDebugView* Debug = nullptr;
	};

	struct FighterSpawn
	{
		glm::vec3 Position{ 0.0f };
		float YawDegrees = 0.0f;
		bool Controlled = true;
	};

	enum class FighterState : uint8_t
	{
		Locomotion,
		Attack,
		Block,
		Dodge,
		HitReact,
		Stagger,
		Taunt,
		Dead
	};

	const char* ToString(FighterState state);

	// The pack's characters face +Z, where the engine's own forward is -Z.
	class Fighter
	{
	public:
		Fighter(const FighterContext& context, const FighterDef& def, const FighterSpawn& spawn);

		Fighter(const Fighter&) = delete;
		Fighter& operator=(const Fighter&) = delete;

		Animator* GetAnimator() const;
		const FighterDef& GetDef() const { return m_Def; }

		void ShowIdle(float time, bool freeze);

		void StartLocomotion(float phase);

		// A negative phase takes the lineup's idle pose.
		void Freeze(float move, float phase);
		void FreezePose(const AnimationClip& clip, float seconds);

		void SetIntent(const FighterIntent& intent) { m_Intent = intent; }

		void Update(float deltaTime, const Fighter* opponent);
		static void Separate(Fighter& a, Fighter& b, float deltaTime);
		void Apply(float deltaTime);

		void OnAnimationEvent(const AnimationEvent& event);

		glm::vec3 GetPosition() const;
		glm::vec2 GetGroundPosition() const;
		float GetGroundSpeed() const { return m_GroundSpeed; }
		float GetMoveParameter() const { return m_MoveParameter; }
		double GetLastStepTime() const { return m_LastStepTime; }
		float GetYaw() const { return m_Yaw; }
		glm::vec2 GetFacing() const;
		LocomotionZone GetZone() const { return m_Zone; }
		float GetRadius() const { return m_Radius; }
		// What a clip's speed becomes on this fighter: its scale times its pace.
		float GetSpeedFactor() const { return m_Def.Scale * m_Def.Pace; }

		// Rest-pose model-space height, before the fighter's scale.
		float GetModelHeight() const { return m_ModelHeight; }

		FighterState GetState() const { return m_State; }
		bool IsCalm() const { return m_State == FighterState::Locomotion; }
		bool IsDead() const { return m_State == FighterState::Dead; }
		float GetHealth() const { return m_Health; }
		float GetMaxHealth() const { return m_Def.Health; }
		const MoveDef* GetMove() const { return m_Move; }
		int GetChainIndex() const { return m_ChainIndex; }
		uint32_t GetSwingId() const { return m_SwingId; }
		bool IsSwingResolved() const { return m_SwingResolved; }
		void ResolveSwing() { m_SwingResolved = true; }
		bool CanRiposte() const;
		// The parry window is open, or the block was raised this frame and the animator has not said so yet.
		bool IsParryOpen() const;

		bool IsWindowActive(std::string_view name) const;
		// The hitbox is open in an attack: the animator says so, or it opened and closed since the last look.
		bool IsHitboxLive() const;
		void ClearHitboxPulse() { m_HitboxPulse = false; }
		// Seconds of real time until the current attack's hitbox opens; negative once it has, infinite off an attack.
		float GetSecondsToHitbox() const;
		// The attack clip's time, held inside the hitbox when it opened and closed since the last look.
		float GetHitboxTime() const;
		const AnimationClip* GetLayerClip(uint32_t layer) const;
		float GetLayerTime(uint32_t layer) const;

		void CollectHurtSpheres(std::vector<WorldSphere>& out) const;
		void AdvanceWeaponTrail();
		void CollectSwingSpheres(std::vector<SweptSphere>& out) const;

		void TakeHit(float damage);
		void TakeBlock(float chip, const glm::vec2& awayDirection);
		void Stagger();
		// Plays a clip as a one-shot over whatever the fighter is doing; a dead fighter stays down.
		void Taunt(const char* clipName);
		void OpenRiposteWindow();
		// The animator runs at HITSTOP_SPEED for this many seconds, from the next update.
		void StartHitStop(float seconds);

		void UpdateDebugTint();

	private:
		struct RigSphere
		{
			Entity Node;
			float Radius = 0.0f;
		};

		Entity SpawnWeapon(const char* path, const char* joint);
		void BuildHitRig(const Model* weapon, Entity weaponPart);
		RigSphere SpawnRigSphere(const std::string& name, Entity parent, const char* joint, const glm::vec3& offset, float radius);
		void SetupLayers(Animator& animator) const;
		void PlayZone(Animator& animator, float deltaTime);

		void Act(Animator& animator, float deltaTime);
		void ActLocomotion(Animator& animator);
		void ActBlock(Animator& animator);
		void ActAttack(Animator& animator);
		void EnterLocomotion();
		void StartAttack(Animator& animator, const MoveDef& move, int chainIndex);
		void StartDodge(Animator& animator);
		void RaiseBlock(Animator& animator);
		void LowerBlock(Animator& animator, float fadeSeconds);
		void Interrupt(const char* clipName, FighterState state, float fadeIn, float fadeOut);
		void Die();
		const char* PickDodgeClip() const;
		bool IsComboOpen(const Animator& animator) const;
		void UpdateHitStop(float deltaTime);

		void ThinkMove(float deltaTime, const Fighter* opponent);
		void ThinkAttack(float deltaTime, const Fighter* opponent);
		void ThinkStill(float deltaTime);

		void BeginTravel(const AnimationClip& clip, float fadeOut, float dashDistance);
		glm::vec2 StepTravel(float deltaTime);
		void UpdateExtra(float deltaTime);

	private:
		// The ground the capsule owes a one-shot: the pose already carries the clip's own hips travel, so
		// the capsule stays put during the move and pays the net of it while the one-shot returns, plus
		// a dodge's extra distance over its dash window. Windows are clip seconds on the move's own clock.
		struct Travel
		{
			bool Active = false;
			float Clock = 0.0f;
			ClipRange Dash;
			glm::vec2 DashStep{ 0.0f };
			ClipRange Return;
			glm::vec2 ReturnStep{ 0.0f };
		};

	private:
		FighterContext m_Context;
		const FighterDef& m_Def;
		Entity m_Entity;
		const Skeleton* m_Skeleton = nullptr;
		LocomotionStates m_States;
		FighterIntent m_Intent;

		float m_ModelHeight = 0.0f;
		float m_Radius = FIGHTER_RADIUS;

		glm::vec2 m_Velocity{ 0.0f };
		glm::vec2 m_Extra{ 0.0f };
		glm::vec2 m_Knockback{ 0.0f };
		glm::vec2 m_LastGround{ 0.0f };
		float m_Yaw = 0.0f;
		float m_VerticalVelocity = 0.0f;
		float m_GroundSpeed = 0.0f;
		float m_MoveParameter = 0.0f;
		float m_LastDelta = 0.0f;
		double m_LastStepTime = -1.0e9;
		LocomotionZone m_Zone = LocomotionZone::Forward;
		LocomotionZone m_PlayedZone = LocomotionZone::Forward;
		bool m_FacingOpponent = false;
		bool m_Frozen = false;
		bool m_Placed = false;
		bool m_Valid = false;

		FighterState m_State = FighterState::Locomotion;
		float m_Health = 0.0f;
		const MoveDef* m_Move = nullptr;
		const AnimationClip* m_MoveClip = nullptr;
		int m_ChainIndex = -1;
		uint32_t m_SwingId = 0;
		bool m_SwingResolved = false;
		Travel m_Travel;
		bool m_BlockUp = false;
		bool m_BlockRaising = false;
		bool m_ParryBegun = false;
		float m_LightBuffer = 0.0f;
		float m_RiposteLeft = 0.0f;
		float m_HitStopLeft = 0.0f;
		float m_AnimationRate = 1.0f;
		bool m_HitboxPulse = false;

		std::vector<RigSphere> m_Hurt;
		std::array<RigSphere, WEAPON_SPHERE_FRACTIONS.size()> m_WeaponSpheres{};
		std::array<glm::vec3, WEAPON_SPHERE_FRACTIONS.size()> m_WeaponPrevious{};
		std::array<glm::vec3, WEAPON_SPHERE_FRACTIONS.size()> m_WeaponCurrent{};
		bool m_HasWeaponSpheres = false;
		bool m_TrailValid = false;

		const AnimationClip* m_PoseClip = nullptr;
		float m_PoseTime = 0.0f;
	};

	class FighterScript : public ScriptableEntity
	{
	public:
		explicit FighterScript(Fighter* fighter) : m_Fighter(fighter) {}

	protected:
		void OnAnimationEvent(const AnimationEvent& event) override { m_Fighter->OnAnimationEvent(event); }

	private:
		Fighter* m_Fighter;
	};

}
