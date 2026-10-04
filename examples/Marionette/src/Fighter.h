#pragma once
#include "Audio.h"
#include "FighterIntent.h"
#include "GameAssets.h"
#include "GameTuning.h"
#include "Locomotion.h"
#include "Moveset.h"

#include <DingoEngine.h>

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
	};

	struct FighterSpawn
	{
		glm::vec3 Position{ 0.0f };
		float YawDegrees = 0.0f;
		bool Controlled = true;
	};

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

		void SetIntent(const FighterIntent& intent) { m_Intent = intent; }

		void Think(float deltaTime, const Fighter* opponent);
		static void Separate(Fighter& a, Fighter& b, float deltaTime);
		void Apply(float deltaTime);

		void OnAnimationEvent(const AnimationEvent& event);

		glm::vec3 GetPosition() const;
		glm::vec2 GetGroundPosition() const;
		float GetGroundSpeed() const { return m_GroundSpeed; }
		float GetMoveParameter() const { return m_MoveParameter; }
		double GetLastStepTime() const { return m_LastStepTime; }
		float GetYaw() const { return m_Yaw; }
		LocomotionZone GetZone() const { return m_Zone; }
		float GetRadius() const { return m_Radius; }
		// What a clip's speed becomes on this fighter: its scale times its pace.
		float GetSpeedFactor() const { return m_Def.Scale * m_Def.Pace; }

		// Rest-pose model-space height, before the fighter's scale.
		float GetModelHeight() const { return m_ModelHeight; }

	private:
		void SpawnWeapon(const char* path, const char* joint);
		void PlayZone(Animator& animator, float deltaTime);

	private:
		FighterContext m_Context;
		const FighterDef& m_Def;
		Entity m_Entity;
		LocomotionStates m_States;
		FighterIntent m_Intent;

		float m_ModelHeight = 0.0f;
		float m_Radius = FIGHTER_RADIUS;

		glm::vec2 m_Velocity{ 0.0f };
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
