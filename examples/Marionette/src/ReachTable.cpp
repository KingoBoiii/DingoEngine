#include "ReachTable.h"
#include "GameAssets.h"
#include "GameTuning.h"
#include "HitGeometry.h"

#include <DingoEngine.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>

namespace
{
	using namespace Dingo;

	constexpr size_t k_FighterCount = 4;

	glm::vec2 Rotate(const glm::vec2& local, float yaw)
	{
		const float c = std::cos(yaw);
		const float s = std::sin(yaw);
		return glm::vec2(local.x * c + local.y * s, -local.x * s + local.y * c);
	}

	// A socket carries a joint's position and rotation, not its scale.
	glm::mat3 JointBasis(const glm::mat4& joint)
	{
		return glm::mat3(glm::normalize(glm::vec3(joint[0])), glm::normalize(glm::vec3(joint[1])), glm::normalize(glm::vec3(joint[2])));
	}

	glm::vec3 ToWorld(const glm::vec3& model, float scale, float yaw, const glm::vec2& origin)
	{
		const glm::vec2 ground = Rotate(glm::vec2(model.x, model.z) * scale, yaw) + origin;
		return glm::vec3(ground.x, model.y * scale, ground.y);
	}
}

namespace Dingo
{

	float MoveReach::DelayAt(float distance) const
	{
		if (ContactDelay.empty())
			return 0.0f;

		const int last = static_cast<int>(ContactDelay.size()) - 1;
		const int index = std::clamp(static_cast<int>(std::lround((distance - REACH_SCAN_MIN) / REACH_SCAN_STEP)), 0, last);
		return std::max(ContactDelay[static_cast<size_t>(index)], 0.0f);
	}

	ReachTable::ReachTable(const GameAssets& assets)
		: m_Assets(assets), m_MoveCount(GetMoves().size())
	{
		m_Sweeps.resize(k_FighterCount * m_MoveCount);
		m_Reach.resize(k_FighterCount * m_MoveCount * k_FighterCount);
		m_ReachDone.assign(m_Reach.size(), false);
		m_Logged.assign(k_FighterCount * k_FighterCount, false);
	}

	size_t ReachTable::SweepIndex(const FighterDef& attacker, size_t move) const
	{
		return static_cast<size_t>(attacker.Id) * m_MoveCount + move;
	}

	size_t ReachTable::ReachIndex(const FighterDef& attacker, size_t move, const FighterDef& target) const
	{
		return SweepIndex(attacker, move) * k_FighterCount + static_cast<size_t>(target.Id);
	}

	const ReachTable::Sweep& ReachTable::GetSweep(const FighterDef& attacker, size_t moveIndex) const
	{
		Sweep& sweep = m_Sweeps[SweepIndex(attacker, moveIndex)];
		if (sweep.Computed)
			return sweep;
		sweep.Computed = true;

		const MoveDef& move = GetMoves()[moveIndex];
		const AnimationClip* clip = m_Assets.GetClip(move.Clip);
		const Model* character = m_Assets.GetCharacter(attacker);
		const Skeleton* skeleton = character ? character->GetSkeleton() : nullptr;
		const Model* weapon = attacker.RightWeapon ? m_Assets.GetModel(attacker.RightWeapon) : nullptr;
		if (!clip || !skeleton || !weapon)
			return sweep;

		const std::optional<ClipRange> hitbox = FindRange(*clip, Events::HITBOX);
		const int32_t hand = skeleton->FindJoint(Joints::HAND_RIGHT);
		const BladeAxis blade = MeasureBlade(*weapon);
		if (!hitbox || hand == Skeleton::k_InvalidJoint || !(blade.Length > 0.0f))
			return sweep;

		Animator animator(skeleton);
		animator.Play(clip);
		const float last = std::max(clip->GetDuration() - 1.0e-4f, 0.0f);
		for (float time = std::max(hitbox->Begin - REACH_SAMPLE_STEP, 0.0f);; time += REACH_SAMPLE_STEP)
		{
			const float at = std::min(time, hitbox->End);
			animator.SetTime(std::min(at, last));
			animator.Evaluate();

			const glm::mat4 joint = animator.GetJointTransform(hand);
			const glm::mat3 basis = JointBasis(joint);
			std::array<glm::vec3, 3> centers;
			for (size_t i = 0; i < centers.size(); ++i)
				centers[i] = glm::vec3(joint[3]) + basis * (blade.Direction * (blade.Length * WEAPON_SPHERE_FRACTIONS[i]));
			sweep.Centers.push_back(centers);
			sweep.Delays.push_back(std::max(at - hitbox->Begin, 0.0f));

			if (at >= hitbox->End)
				break;
		}
		sweep.Radius = WeaponSphereRadius(blade);
		sweep.Valid = true;
		return sweep;
	}

	std::vector<ReachTable::Hurt> ReachTable::GetHurt(const FighterDef& target) const
	{
		std::vector<Hurt> hurt;
		const Model* model = m_Assets.GetCharacter(target);
		const Skeleton* skeleton = model ? model->GetSkeleton() : nullptr;
		const AnimationClip* idle = m_Assets.GetClip(Clips::IDLE);
		if (!skeleton || !idle)
			return hurt;

		Animator pose(skeleton);
		pose.Play(idle);
		pose.SetTime(FREEZE_POSE_TIME);
		pose.Evaluate();
		for (const HurtSphereDef& def : HURT_SPHERES)
		{
			const int32_t joint = skeleton->FindJoint(def.Joint);
			if (joint == Skeleton::k_InvalidJoint)
				continue;

			const glm::mat4 transform = pose.GetJointTransform(joint);
			hurt.push_back({ glm::vec3(transform[3]) + JointBasis(transform) * def.Offset, def.Radius });
		}
		return hurt;
	}

	MoveReach ReachTable::Scan(const Sweep& sweep, const FighterDef& attacker, const MoveDef& move, const FighterDef& target) const
	{
		MoveReach reach;
		const std::vector<Hurt> hurt = GetHurt(target);
		if (!sweep.Valid || hurt.empty())
			return reach;

		// The attacker turns AimDegrees past the line to its target in the windup (Fighter::ThinkAttack), and
		// the target faces back at it.
		const float attackerYaw = glm::radians(move.AimDegrees);
		constexpr float targetYaw = std::numbers::pi_v<float>;
		const glm::vec2 attackerAt(0.0f);

		std::vector<std::array<glm::vec3, 3>> swept;
		swept.reserve(sweep.Centers.size());
		for (const std::array<glm::vec3, 3>& centers : sweep.Centers)
		{
			std::array<glm::vec3, 3> world;
			for (size_t i = 0; i < world.size(); ++i)
				world[i] = ToWorld(centers[i], attacker.Scale, attackerYaw, attackerAt);
			swept.push_back(world);
		}

		const float weaponRadius = sweep.Radius * attacker.Scale;
		std::vector<WorldSphere> targets(hurt.size());
		for (int step = 0;; ++step)
		{
			const float distance = REACH_SCAN_MIN + static_cast<float>(step) * REACH_SCAN_STEP;
			if (distance > REACH_SCAN_MAX + 1.0e-4f)
				break;

			for (size_t j = 0; j < hurt.size(); ++j)
				targets[j] = { ToWorld(hurt[j].Center, target.Scale, targetYaw, glm::vec2(0.0f, distance)), hurt[j].Radius * target.Scale };

			size_t first = swept.size();
			for (size_t s = 0; s < swept.size() && first == swept.size(); ++s)
			{
				const std::array<glm::vec3, 3>& previous = swept[s > 0 ? s - 1 : 0];
				for (size_t i = 0; i < swept[s].size() && first == swept.size(); ++i)
				{
					const SweptSphere sphere{ previous[i], swept[s][i], weaponRadius };
					if (std::any_of(targets.begin(), targets.end(), [&](const WorldSphere& hurtSphere) { return Touches(sphere, hurtSphere); }))
						first = s;
				}
			}

			if (first == swept.size())
			{
				reach.ContactDelay.push_back(-1.0f);
				continue;
			}

			reach.ContactDelay.push_back(sweep.Delays[first]);
			if (!reach.Valid)
				reach.Min = distance;
			reach.Max = distance;
			reach.Valid = true;
		}
		return reach;
	}

	const MoveReach& ReachTable::Get(const FighterDef& attacker, const MoveDef& move, const FighterDef& target) const
	{
		static const MoveReach s_None;
		const std::ptrdiff_t index = &move - GetMoves().data();
		if (index < 0 || static_cast<size_t>(index) >= m_MoveCount)
			return s_None;

		const size_t at = ReachIndex(attacker, static_cast<size_t>(index), target);
		if (!m_ReachDone[at])
		{
			m_ReachDone[at] = true;
			m_Reach[at] = Scan(GetSweep(attacker, static_cast<size_t>(index)), attacker, move, target);
		}
		return m_Reach[at];
	}

	float ReachTable::GetOpeningMax(const FighterDef& attacker, const FighterDef& target) const
	{
		float farthest = 0.0f;
		if (!attacker.LightChain.empty())
		{
			if (const MoveDef* light = FindMove(attacker.LightChain[0]))
				farthest = std::max(farthest, Get(attacker, *light, target).Max);
		}
		if (const MoveDef* heavy = GetHeavyMove(attacker))
			farthest = std::max(farthest, Get(attacker, *heavy, target).Max);
		return farthest;
	}

	void ReachTable::Log(const FighterDef& attacker, const FighterDef& target) const
	{
		const size_t pair = static_cast<size_t>(attacker.Id) * k_FighterCount + static_cast<size_t>(target.Id);
		if (m_Logged[pair])
			return;
		m_Logged[pair] = true;

		std::vector<const MoveDef*> moves;
		for (const char* clip : attacker.LightChain)
			moves.push_back(FindMove(clip));
		moves.push_back(GetHeavyMove(attacker));
		moves.push_back(&GetRiposteMove());
		for (const MoveDef* move : moves)
		{
			if (!move)
				continue;

			const MoveReach& reach = Get(attacker, *move, target);
			if (reach.Valid)
			{
				DE_INFO("[Reach] {} {} against {}: touches from {:.2f} to {:.2f} m apart", attacker.Name, move->Clip, target.Name, reach.Min, reach.Max);
			}
			else
			{
				DE_WARN("[Reach] {} {} against {}: never touches between {:.1f} and {:.1f} m; the AI falls back to {:.1f} m", attacker.Name, move->Clip, target.Name,
					REACH_SCAN_MIN, REACH_SCAN_MAX, AI_FALLBACK_REACH);
			}
		}
	}

}
