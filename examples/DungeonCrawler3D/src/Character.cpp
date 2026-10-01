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

	Dingo::Entity MakeNode(Dingo::Scene& scene, const char* name, Dingo::Entity parent, const glm::vec3& offset)
	{
		Dingo::Entity entity = scene.CreateEntity(name);
		entity.AddComponent<Dingo::Transform3DComponent>(Dingo::Transform3DComponent(offset));
		if (parent)
			entity.SetParent(parent, false);
		return entity;
	}
}

namespace Dingo
{
	void Character::Create(Scene& scene, const CharacterMeshes& meshes, const glm::vec4& color, bool withSword)
	{
		if (m_Created)
			return;

		m_Root = MakeNode(scene, "Char_Root", {}, glm::vec3(0.0f));
		m_Joints[HipL] = MakeNode(scene, "Char_HipL", m_Root, { -LEG_X, HIP_Y, 0.0f });
		m_Joints[HipR] = MakeNode(scene, "Char_HipR", m_Root, { LEG_X, HIP_Y, 0.0f });
		m_Joints[ShoulderL] = MakeNode(scene, "Char_ShoulderL", m_Root, { -ARM_X, SHOULDER_Y, 0.0f });
		m_Joints[ShoulderR] = MakeNode(scene, "Char_ShoulderR", m_Root, { ARM_X, SHOULDER_Y, 0.0f });

		auto part = [&](Part id, Mesh* mesh, Entity parent, const glm::vec3& offset, const glm::vec4& tint, const char* name)
		{
			m_Parts[id] = MakeNode(scene, name, parent, offset);
			m_Parts[id].AddComponent<MeshRendererComponent>(MeshRendererComponent(mesh, tint));
			m_BaseColors[id] = tint;
		};

		part(Head, meshes.Head, m_Root, { 0.0f, HEAD_Y, 0.0f }, color, "Char_Head");
		part(Torso, meshes.Torso, m_Root, { 0.0f, TORSO_Y, 0.0f }, color, "Char_Torso");
		part(LegL, meshes.Leg, m_Joints[HipL], { 0.0f, LEG_ORIGIN_Y - HIP_Y, 0.0f }, color, "Char_LegL");
		part(LegR, meshes.Leg, m_Joints[HipR], { 0.0f, LEG_ORIGIN_Y - HIP_Y, 0.0f }, color, "Char_LegR");
		part(ArmL, meshes.Arm, m_Joints[ShoulderL], { 0.0f, ARM_ORIGIN_Y - SHOULDER_Y, 0.0f }, color, "Char_ArmL");
		part(ArmR, meshes.Arm, m_Joints[ShoulderR], { 0.0f, ARM_ORIGIN_Y - SHOULDER_Y, 0.0f }, color, "Char_ArmR");

		if (withSword && meshes.Sword)
		{
			// Grip at the right hand, under the shoulder joint so it swings with the arm, with a
			// fixed grip tilt so the blade angles out of the fist instead of lying flat against it.
			part(Sword, meshes.Sword, m_Joints[ShoulderR], { 0.0f, HAND_Y - SHOULDER_Y, 0.0f }, STEEL, "Char_Sword");
			m_Parts[Sword].GetComponent<Transform3DComponent>().Rotation = glm::angleAxis(SWORD_GRIP_TILT, X_AXIS);
		}

		m_Created = true;
	}

	void Character::Destroy()
	{
		if (!m_Created)
			return;

		if (m_Root.IsValid())
			m_Root.Destroy();

		m_Root = {};
		for (Entity& joint : m_Joints)
			joint = {};
		for (Entity& part : m_Parts)
			part = {};
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

	void Character::Update(const glm::vec3& feetPosition, float targetYaw, float walkSpeed01, float deltaTime)
	{
		if (!m_Created || !m_Root.IsValid())
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
		float rightArm = ARM_R_HOLD + s * armAmp * ARM_R_SWAY;

		// Attack: wind the weapon arm up over the shoulder, snap it down/forward, then
		// recover to the ready pose — a three-phase chop instead of one even sweep.
		float attackLean = 0.0f;
		if (m_AttackTime > 0.0f)
		{
			m_AttackTime = std::max(0.0f, m_AttackTime - deltaTime);
			const float p = 1.0f - (m_AttackTime / ATTACK_DUR); // 0 -> 1

			if (p < ANTIC_END)
			{
				const float u = p / ANTIC_END;
				rightArm = glm::mix(ARM_R_HOLD, RAISE_ANGLE, glm::smoothstep(0.0f, 1.0f, u));
			}
			else if (p < STRIKE_END)
			{
				const float u = (p - ANTIC_END) / (STRIKE_END - ANTIC_END);
				rightArm = glm::mix(RAISE_ANGLE, STRIKE_ANGLE, glm::smoothstep(0.0f, 1.0f, u));
			}
			else
			{
				const float u = (p - STRIKE_END) / (1.0f - STRIKE_END);
				rightArm = glm::mix(STRIKE_ANGLE, ARM_R_HOLD, glm::smoothstep(0.0f, 1.0f, u));
			}

			// Body leans forward into the swing, peaking around the strike.
			attackLean = ATTACK_LUNGE * std::sin(p * glm::pi<float>());
		}

		m_Joints[HipL].GetComponent<Transform3DComponent>().Rotation = glm::angleAxis(s * legAmp, X_AXIS);
		m_Joints[HipR].GetComponent<Transform3DComponent>().Rotation = glm::angleAxis(-s * legAmp, X_AXIS);
		m_Joints[ShoulderL].GetComponent<Transform3DComponent>().Rotation = glm::angleAxis(-s * armAmp, X_AXIS);
		m_Joints[ShoulderR].GetComponent<Transform3DComponent>().Rotation = glm::angleAxis(rightArm, X_AXIS);

		// A little vertical bounce: a step bob while walking, a gentle breath while idle.
		const float walkBob = 0.030f * walkSpeed01 * std::abs(std::sin(m_WalkPhase));
		const float idleBob = 0.010f * (1.0f - walkSpeed01) * std::sin(m_WalkPhase * 0.5f);

		// Hit reaction: the whole body recoils backward about the feet, decaying fast.
		if (m_HitTime > 0.0f)
			m_HitTime = std::max(0.0f, m_HitTime - deltaTime);
		const float hitLean = HIT_LEAN * (m_HitTime / HIT_DUR); // peak -> 0 over the window

		// The root stands at the feet: facing, then the body lean (attack lunge forward + hit
		// recoil backward) in the character's local frame.
		Transform3DComponent& root = m_Root.GetComponent<Transform3DComponent>();
		root.Position = feetPosition + glm::vec3(0.0f, walkBob + idleBob, 0.0f);
		root.Rotation = glm::angleAxis(m_Yaw, Y_AXIS) * glm::angleAxis(attackLean + hitLean, X_AXIS);

		for (int i = 0; i < PartCount; ++i)
		{
			if (m_Parts[i].IsValid())
				m_Parts[i].GetComponent<MeshRendererComponent>().Color = glm::mix(m_BaseColors[i], m_FlashColor, m_FlashAmount);
		}
	}
}
