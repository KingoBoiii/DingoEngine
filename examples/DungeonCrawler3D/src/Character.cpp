#include "Character.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/common.hpp>

#include <algorithm>
#include <cmath>

namespace
{
	// Skeleton-space proportions (feet at y = 0, facing +Z) — must match the part meshes
	// authored by assets/models/generate_models.js.
	constexpr float HIP_Y = 0.45f;       // top of the legs
	constexpr float LEG_X = 0.12f;       // hip half-spacing
	constexpr float LEG_ORIGIN_Y = 0.225f;
	constexpr float SHOULDER_Y = 0.85f;  // top of the arms
	constexpr float ARM_X = 0.295f;      // shoulder half-spacing
	constexpr float ARM_ORIGIN_Y = 0.64f;
	constexpr float TORSO_Y = 0.66f;
	constexpr float HEAD_Y = 1.04f;
	constexpr float HAND_Y = 0.43f;      // bottom of the arm (the grip sits here)

	// Animation tuning.
	constexpr float IDLE_FREQ = 2.2f;
	constexpr float WALK_FREQ = 9.0f;
	constexpr float LEG_AMP = 0.70f;
	constexpr float ARM_AMP = 0.55f;
	constexpr float ARM_IDLE = 0.06f;

	// Right (weapon) arm: a steady "ready" bend so it presents the sword rather than
	// letting it hang flat against the arm; it sways only a little while walking.
	constexpr float ARM_R_HOLD = -0.40f;    // forward bend of the weapon arm (rad, about X)
	constexpr float ARM_R_SWAY = 0.30f;     // fraction of the normal walk swing it keeps
	// Fixed grip orientation: tilts the blade off the forearm so it reads as "held",
	// pointing up-and-forward out of the fist (about X; ~63deg from the arm).
	constexpr float SWORD_GRIP_TILT = 1.10f;

	// Attack = anticipation (wind up) -> fast strike -> recovery, as fractions of the
	// total duration. The short strike window between them is what gives the swing snap.
	constexpr float ATTACK_DUR = 0.36f;
	constexpr float ANTIC_END = 0.30f;   // wind-up finishes at 30% of the swing
	constexpr float STRIKE_END = 0.52f;  // impact lands at 52%; the rest is recovery
	constexpr float RAISE_ANGLE = -1.5f; // wound up/back (weapon-arm angle, rad about X)
	constexpr float STRIKE_ANGLE = 1.7f; // chopped down/forward (weapon-arm angle, rad about X)
	constexpr float ATTACK_LUNGE = 0.22f; // forward body lean peaking at the strike (rad)

	constexpr float HIT_DUR = 0.20f;      // hit-reaction duration (s)
	constexpr float HIT_LEAN = -0.45f;    // peak backward recoil (rad, about X)

	const glm::vec3 X_AXIS{ 1.0f, 0.0f, 0.0f };
	const glm::vec3 Y_AXIS{ 0.0f, 1.0f, 0.0f };

	const glm::vec4 STEEL{ 0.80f, 0.82f, 0.88f, 1.0f };

	float WrapAngle(float a)
	{
		const float twoPi = glm::two_pi<float>();
		while (a > glm::pi<float>()) a -= twoPi;
		while (a < -glm::pi<float>()) a += twoPi;
		return a;
	}
}

namespace Dingo
{
	void Character::Create(Scene& scene, const CharacterMeshes& meshes, const glm::vec4& color, bool withSword)
	{
		if (m_Created)
			return;

		m_HasSword = withSword;

		auto setup = [&](Part part, Mesh* mesh, const glm::vec3& center, const glm::vec3& pivot,
			const glm::vec4& col, const char* name)
		{
			Bone& bone = m_Bones[part];
			bone.entity = scene.CreateEntity(name);
			bone.entity.AddComponent<Transform3DComponent>();
			bone.entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(mesh, col));
			bone.restCenter = center;
			bone.pivot = pivot;
			bone.baseColor = col;
			bone.jointRot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		};

		// Rigid parts: pivot == restCenter (no joint swing).
		setup(Head, meshes.Head, { 0.0f, HEAD_Y, 0.0f }, { 0.0f, HEAD_Y, 0.0f }, color, "Char_Head");
		setup(Torso, meshes.Torso, { 0.0f, TORSO_Y, 0.0f }, { 0.0f, TORSO_Y, 0.0f }, color, "Char_Torso");

		// Legs pivot about the hips, arms about the shoulders.
		setup(LegL, meshes.Leg, { -LEG_X, LEG_ORIGIN_Y, 0.0f }, { -LEG_X, HIP_Y, 0.0f }, color, "Char_LegL");
		setup(LegR, meshes.Leg, { LEG_X, LEG_ORIGIN_Y, 0.0f }, { LEG_X, HIP_Y, 0.0f }, color, "Char_LegR");
		setup(ArmL, meshes.Arm, { -ARM_X, ARM_ORIGIN_Y, 0.0f }, { -ARM_X, SHOULDER_Y, 0.0f }, color, "Char_ArmL");
		setup(ArmR, meshes.Arm, { ARM_X, ARM_ORIGIN_Y, 0.0f }, { ARM_X, SHOULDER_Y, 0.0f }, color, "Char_ArmR");

		if (m_HasSword && meshes.Sword)
		{
			// Grip at the right hand (so it shares the arm's swing about the shoulder),
			// with a fixed grip tilt so the blade angles out of the fist instead of lying
			// flat against the arm.
			setup(Sword, meshes.Sword, { ARM_X, HAND_Y, 0.0f }, { ARM_X, SHOULDER_Y, 0.0f }, STEEL, "Char_Sword");
			m_Bones[Sword].meshRot = glm::angleAxis(SWORD_GRIP_TILT, X_AXIS);
		}

		m_Created = true;
	}

	void Character::Destroy()
	{
		if (!m_Created)
			return;

		for (int i = 0; i < PartCount; ++i)
		{
			if (m_Bones[i].entity.IsValid())
				m_Bones[i].entity.Destroy();
			m_Bones[i].entity = {};
		}
		m_Created = false;
	}

	void Character::SetFlash(const glm::vec4& color, float amount)
	{
		m_FlashColor = color;
		m_FlashAmount = glm::clamp(amount, 0.0f, 1.0f);
	}

	void Character::TriggerAttack()
	{
		m_AttackTime = ATTACK_DUR;
	}

	void Character::TriggerHit()
	{
		m_HitTime = HIT_DUR;
	}

	void Character::PlaceBone(Bone& bone, const glm::vec3& feet, const glm::quat& yaw, float bobY)
	{
		if (!bone.entity.IsValid())
			return;

		const glm::vec3 local = bone.pivot + bone.jointRot * (bone.restCenter - bone.pivot);
		const glm::vec3 worldPos = feet + glm::vec3(0.0f, bobY, 0.0f) + yaw * local;

		Transform3DComponent& transform = bone.entity.GetComponent<Transform3DComponent>();
		transform.Position = worldPos;
		transform.Rotation = yaw * bone.jointRot * bone.meshRot;

		bone.entity.GetComponent<MeshRendererComponent>().Color =
			glm::mix(bone.baseColor, m_FlashColor, m_FlashAmount);
	}

	void Character::Update(const glm::vec3& feetPosition, float targetYaw, float walkSpeed01, float deltaTime)
	{
		if (!m_Created)
			return;

		walkSpeed01 = glm::clamp(walkSpeed01, 0.0f, 1.0f);

		// Ease the facing toward the target along the shortest arc.
		const float blend = 1.0f - std::exp(-12.0f * deltaTime);
		m_Yaw += WrapAngle(targetYaw - m_Yaw) * blend;

		// Advance the cycle: slow idle breath + a faster walk the more we move.
		m_WalkPhase += deltaTime * (IDLE_FREQ + WALK_FREQ * walkSpeed01);
		const float s = std::sin(m_WalkPhase);

		const float legAmp = LEG_AMP * walkSpeed01;
		const float armAmp = ARM_AMP * walkSpeed01 + ARM_IDLE;

		// Gait: legs swing opposite each other; the free (left) arm counter-swings, while
		// the weapon (right) arm holds a steady ready pose and only sways a little.
		m_Bones[LegL].jointRot = glm::angleAxis(s * legAmp, X_AXIS);
		m_Bones[LegR].jointRot = glm::angleAxis(-s * legAmp, X_AXIS);
		m_Bones[ArmL].jointRot = glm::angleAxis(-s * armAmp, X_AXIS);
		m_Bones[ArmR].jointRot = glm::angleAxis(ARM_R_HOLD + s * armAmp * ARM_R_SWAY, X_AXIS);
		m_Bones[Head].jointRot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		m_Bones[Torso].jointRot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

		// Attack: wind the weapon arm up over the shoulder, snap it down/forward, then
		// recover to the ready pose — a three-phase chop instead of one even sweep.
		float attackLean = 0.0f;
		if (m_AttackTime > 0.0f)
		{
			m_AttackTime = std::max(0.0f, m_AttackTime - deltaTime);
			const float p = 1.0f - (m_AttackTime / ATTACK_DUR); // 0 -> 1

			float chop;
			if (p < ANTIC_END)
			{
				const float u = p / ANTIC_END;
				chop = glm::mix(ARM_R_HOLD, RAISE_ANGLE, glm::smoothstep(0.0f, 1.0f, u));
			}
			else if (p < STRIKE_END)
			{
				const float u = (p - ANTIC_END) / (STRIKE_END - ANTIC_END);
				chop = glm::mix(RAISE_ANGLE, STRIKE_ANGLE, glm::smoothstep(0.0f, 1.0f, u));
			}
			else
			{
				const float u = (p - STRIKE_END) / (1.0f - STRIKE_END);
				chop = glm::mix(STRIKE_ANGLE, ARM_R_HOLD, glm::smoothstep(0.0f, 1.0f, u));
			}
			m_Bones[ArmR].jointRot = glm::angleAxis(chop, X_AXIS);

			// Body leans forward into the swing, peaking around the strike.
			attackLean = ATTACK_LUNGE * std::sin(p * glm::pi<float>());
		}

		// The sword always tracks the right arm (rests with it, swings with the chop).
		if (m_HasSword)
			m_Bones[Sword].jointRot = m_Bones[ArmR].jointRot;

		// A little vertical bounce: a step bob while walking, a gentle breath while idle.
		const float walkBob = 0.030f * walkSpeed01 * std::abs(std::sin(m_WalkPhase));
		const float idleBob = 0.010f * (1.0f - walkSpeed01) * std::sin(m_WalkPhase * 0.5f);
		const float bob = walkBob + idleBob;

		// Hit reaction: the whole body recoils backward about the feet, decaying fast.
		if (m_HitTime > 0.0f)
			m_HitTime = std::max(0.0f, m_HitTime - deltaTime);
		const float hitLean = HIT_LEAN * (m_HitTime / HIT_DUR); // peak -> 0 over the window

		// Root orientation = facing, then the body lean (attack lunge forward + hit recoil
		// backward) in the character's local frame.
		const glm::quat root = glm::angleAxis(m_Yaw, Y_AXIS) * glm::angleAxis(attackLean + hitLean, X_AXIS);
		for (int i = 0; i < PartCount; ++i)
			PlaceBone(m_Bones[i], feetPosition, root, bob);
	}
}
