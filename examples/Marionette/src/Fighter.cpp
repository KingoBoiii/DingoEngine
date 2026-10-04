#include "Fighter.h"
#include "GameMath.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <string>

namespace
{
	using namespace Dingo;

	constexpr float k_Infinity = std::numeric_limits<float>::infinity();

	glm::vec2 Rotate(const glm::vec2& local, float yaw)
	{
		const float c = std::cos(yaw);
		const float s = std::sin(yaw);
		return glm::vec2(local.x * c + local.y * s, -local.x * s + local.y * c);
	}
}

namespace Dingo
{

	const char* ToString(FighterState state)
	{
		switch (state)
		{
			case FighterState::Attack:   return "Attack";
			case FighterState::Block:    return "Block";
			case FighterState::Dodge:    return "Dodge";
			case FighterState::HitReact: return "HitReact";
			case FighterState::Stagger:  return "Stagger";
			case FighterState::Taunt:    return "Taunt";
			case FighterState::Dead:     return "Dead";
			default:                     return "Locomotion";
		}
	}

	Fighter::Fighter(const FighterContext& context, const FighterDef& def, const FighterSpawn& spawn)
		: m_Context(context), m_Def(def)
	{
		Scene& scene = context.World;
		m_Entity = scene.CreateEntity(def.Name);
		m_Health = def.Health;
		m_AnimationRate = def.Pace;

		Model* model = context.Assets.GetCharacter(def);
		if (!model || !model->GetSkeleton())
		{
			DE_ERROR("Marionette: {} has no skinned model; it will not be drawn", def.Name);
			return;
		}

		m_Skeleton = model->GetSkeleton();
		m_Yaw = glm::radians(spawn.YawDegrees);
		auto& transform = m_Entity.AddComponent<Transform3DComponent>();
		transform.Position = spawn.Position;
		transform.Rotation = GameMath::YawQuat(m_Yaw);
		transform.Scale = glm::vec3(def.Scale);

		m_Entity.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(model)).Material = context.Assets.GetMaterial(def);

		AnimatorComponent animator;
		animator.PlayOnStart = false;
		animator.Speed = def.Pace;
		m_Entity.AddComponent<AnimatorComponent>(animator);

		float top = 0.0f;
		for (const SubMesh& submesh : model->GetSubMeshes())
		{
			if (!submesh.MeshData)
				continue;
			for (const MeshVertex& vertex : submesh.MeshData->GetVertices())
				top = std::max(top, vertex.Position.y);
		}
		m_ModelHeight = top;
		m_Radius = FIGHTER_RADIUS * def.Scale;

		if (spawn.Controlled)
		{
			auto& controller = m_Entity.AddComponent<CharacterController3DComponent>();
			controller.Radius = m_Radius;
			controller.Height = std::max(m_ModelHeight * def.Scale, 2.0f * m_Radius + FIGHTER_MIN_CAPSULE_BODY);
			controller.StepHeight = FIGHTER_STEP_HEIGHT * def.Scale;
			controller.MaxSlopeAngle = FIGHTER_SLOPE;
			m_Entity.AddScript<FighterScript>(this);
		}

		m_States = MakeLocomotionStates(context.Assets.GetClips());
		m_LastGround = glm::vec2(spawn.Position.x, spawn.Position.z);
		m_Valid = true;

		Entity rightHand;
		if (def.RightWeapon)
			rightHand = SpawnWeapon(def.RightWeapon, Joints::HAND_RIGHT);
		if (def.LeftWeapon)
			SpawnWeapon(def.LeftWeapon, Joints::HAND_LEFT);

		if (spawn.Controlled)
			BuildHitRig(def.RightWeapon ? context.Assets.GetModel(def.RightWeapon) : nullptr, rightHand);
	}

	Animator* Fighter::GetAnimator() const
	{
		return m_Context.World.GetAnimator(m_Entity);
	}

	glm::vec3 Fighter::GetPosition() const
	{
		if (!m_Valid)
			return glm::vec3(0.0f);

		Entity entity = m_Entity;
		return entity.GetComponent<Transform3DComponent>().Position;
	}

	glm::vec2 Fighter::GetGroundPosition() const
	{
		const glm::vec3 position = GetPosition();
		return glm::vec2(position.x, position.z);
	}

	glm::vec2 Fighter::GetFacing() const
	{
		return glm::vec2(std::sin(m_Yaw), std::cos(m_Yaw));
	}

	Entity Fighter::SpawnWeapon(const char* path, const char* joint)
	{
		const Model* weapon = m_Context.Assets.GetModel(path);
		if (!weapon)
		{
			DE_ERROR("Marionette: {} cannot carry '{}', it did not load", m_Def.Name, path);
			return Entity();
		}

		Material* material = m_Context.Assets.GetMaterial(m_Def);
		const std::string name = std::format("{} {}", m_Def.Name, joint);
		size_t index = 0;
		Entity first;
		for (const SubMesh& submesh : weapon->GetSubMeshes())
		{
			if (!submesh.MeshData)
				continue;

			Entity part = m_Context.World.CreateEntity(index == 0 ? name : std::format("{} {}", name, index));
			part.AddComponent<Transform3DComponent>();
			part.AddComponent<MeshRendererComponent>(MeshRendererComponent(submesh.MeshData)).Material = material;
			part.SetParent(m_Entity, joint, false);
			if (index == 0)
				first = part;
			++index;
		}
		return first;
	}

	Fighter::RigSphere Fighter::SpawnRigSphere(const std::string& name, Entity parent, const char* joint, const glm::vec3& offset, float radius)
	{
		Entity node = m_Context.World.CreateEntity(name);
		auto& transform = node.AddComponent<Transform3DComponent>();
		transform.Position = offset;
		transform.Scale = glm::vec3(radius / HIT_SPHERE_MESH_RADIUS);
		node.SetParent(parent, joint ? joint : "", false);

		if (m_Context.Debug)
		{
			node.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Context.Debug->GetMesh())).Material = m_Context.Debug->GetMaterial(DebugTint::Idle);
		}

		RigSphere sphere;
		sphere.Node = node;
		sphere.Radius = radius;
		return sphere;
	}

	void Fighter::BuildHitRig(const Model* weapon, Entity weaponPart)
	{
		for (const HurtSphereDef& def : HURT_SPHERES)
			m_Hurt.push_back(SpawnRigSphere(std::format("{} hurt {}", m_Def.Name, def.Joint), m_Entity, def.Joint, def.Offset, def.Radius));

		if (!weapon || !weaponPart)
			return;

		const BladeAxis blade = MeasureBlade(*weapon);
		if (!(blade.Length > 0.0f))
			return;

		const float radius = WeaponSphereRadius(blade);
		for (size_t i = 0; i < m_WeaponSpheres.size(); ++i)
		{
			m_WeaponSpheres[i] = SpawnRigSphere(std::format("{} blade {}", m_Def.Name, i), weaponPart, nullptr,
				blade.Direction * (blade.Length * WEAPON_SPHERE_FRACTIONS[i]), radius);
		}
		m_HasWeaponSpheres = true;
		if (m_Context.LogCombat)
			DE_INFO("[Rig] {}: blade {:.2f} m, half-width {:.3f} m, weapon spheres radius {:.3f} m", m_Def.Name, blade.Length, blade.HalfWidth, radius);
	}

	void Fighter::SetupLayers(Animator& animator) const
	{
		animator.SetLayer(BLOCK_LAYER, AnimationLayer().SetMask(Joints::SPINE).SetWeight(1.0f));
	}

	void Fighter::ShowIdle(float time, bool freeze)
	{
		ShowClip(m_Context.Assets.GetClip(Clips::IDLE), time, freeze);
	}

	void Fighter::ShowClip(const AnimationClip* clip, float time, bool freeze)
	{
		Animator* animator = GetAnimator();
		if (!animator || !clip)
			return;

		animator->Play(clip);
		animator->SetTime(time);
		animator->Evaluate();
		m_Entity.GetComponent<AnimatorComponent>().Enabled = !freeze;
		m_Frozen = freeze;
	}

	void Fighter::OnEventsChanged()
	{
		Animator* animator = m_Valid && m_PoseClip ? GetAnimator() : nullptr;
		if (!animator)
			return;

		animator->SetTime(m_PoseTime);
		animator->Evaluate();
	}

	void Fighter::StartLocomotion(float phase)
	{
		Animator* animator = GetAnimator();
		if (!animator || !m_States.Complete)
			return;

		SetupLayers(*animator);
		animator->SetFloat(MOVE_PARAMETER, 0.0f);
		animator->Play(m_States.Blend);
		animator->SetNormalizedTime(phase);
		animator->Evaluate();
		m_PlayedZone = LocomotionZone::Forward;
	}

	void Fighter::Freeze(float move, float phase)
	{
		m_Frozen = true;

		Animator* animator = GetAnimator();
		if (!animator || !m_States.Complete)
			return;

		if (phase < 0.0f)
		{
			const AnimationClip* idle = m_Context.Assets.GetClip(Clips::IDLE);
			phase = idle && idle->GetDuration() > 0.0f ? FREEZE_POSE_TIME / idle->GetDuration() : FREEZE_FALLBACK_PHASE;
		}

		SetupLayers(*animator);
		m_MoveParameter = std::max(move, 0.0f);
		animator->SetFloat(MOVE_PARAMETER, m_MoveParameter);
		animator->Play(m_States.Blend);
		animator->SetNormalizedTime(phase);
		animator->Evaluate();
		m_Entity.GetComponent<AnimatorComponent>().Enabled = false;
		m_PlayedZone = LocomotionZone::Forward;
	}

	void Fighter::FreezePose(const AnimationClip& clip, float seconds)
	{
		m_Frozen = true;

		Animator* animator = GetAnimator();
		if (!animator)
			return;

		SetupLayers(*animator);
		animator->Play(&clip);
		animator->SetTime(seconds);
		animator->Evaluate();
		m_Entity.GetComponent<AnimatorComponent>().Enabled = false;
		m_PoseClip = &clip;
		m_PoseTime = seconds;
	}

	void Fighter::Update(float deltaTime, const Fighter* opponent)
	{
		if (!m_Valid)
			return;

		UpdateHitStop(deltaTime);

		if (!m_Frozen && m_States.Complete)
		{
			if (Animator* animator = GetAnimator())
				Act(*animator, deltaTime);
		}

		// The controller reports the velocity it was given, not the one it achieved, so the step is measured.
		// The extra velocity went with it and is not the fighter's own to lose.
		if (m_Placed && m_LastDelta > 0.0f)
		{
			const glm::vec2 achieved = (GetGroundPosition() - m_LastGround) / m_LastDelta;
			const glm::vec2 own = achieved - m_Extra;
			if (glm::length(own) < glm::length(m_Velocity))
				m_Velocity = own;
			m_GroundSpeed = std::min(glm::length(m_Velocity), glm::length(achieved));
		}

		switch (m_State)
		{
			case FighterState::Locomotion:
			case FighterState::Block:    ThinkMove(deltaTime, opponent); break;
			case FighterState::Attack:   ThinkAttack(deltaTime, opponent); break;
			default:                     ThinkStill(deltaTime); break;
		}
		UpdateExtra(deltaTime);
	}

	void Fighter::UpdateHitStop(float deltaTime)
	{
		const float stopped = std::min(m_HitStopLeft, deltaTime);
		m_HitStopLeft -= stopped;
		m_AnimationRate = glm::mix(m_Def.Pace, HITSTOP_SPEED, deltaTime > 0.0f ? stopped / deltaTime : 0.0f);
		m_Entity.GetComponent<AnimatorComponent>().Speed = m_AnimationRate;
	}

	void Fighter::StartHitStop(float seconds)
	{
		m_HitStopLeft = seconds;
	}

	void Fighter::Act(Animator& animator, float deltaTime)
	{
		const float ownTime = deltaTime * m_AnimationRate / m_Def.Pace;
		m_LightBuffer = std::max(m_LightBuffer - ownTime, 0.0f);
		m_RiposteLeft = std::max(m_RiposteLeft - ownTime, 0.0f);
		if (m_Intent.Light)
			m_LightBuffer = INPUT_BUFFER;

		switch (m_State)
		{
			case FighterState::Locomotion: ActLocomotion(animator); break;
			case FighterState::Block:      ActBlock(animator); break;
			case FighterState::Attack:     ActAttack(animator); break;
			case FighterState::Dodge:
			case FighterState::HitReact:
			case FighterState::Stagger:
			case FighterState::Taunt:
				if (!animator.IsOneShotPlaying(0))
					EnterLocomotion();
				break;
			default:
				break;
		}
	}

	void Fighter::ActLocomotion(Animator& animator)
	{
		if (m_Intent.Dodge)
		{
			StartDodge(animator);
			return;
		}
		if (m_Intent.Heavy)
		{
			if (const MoveDef* heavy = GetHeavyMove(m_Def))
			{
				StartAttack(animator, *heavy, -1);
				return;
			}
		}
		if (m_Intent.Light && !m_Def.LightChain.empty())
		{
			if (const MoveDef* first = FindMove(m_Def.LightChain[0]))
			{
				StartAttack(animator, *first, 0);
				return;
			}
		}
		if (m_Intent.Block)
			RaiseBlock(animator);
	}

	void Fighter::ActBlock(Animator& animator)
	{
		if (m_Intent.Dodge)
		{
			LowerBlock(animator, 0.0f);
			StartDodge(animator);
			return;
		}
		if (m_Intent.Light)
		{
			const bool riposte = CanRiposte();
			const MoveDef* move = riposte ? &GetRiposteMove() : (m_Def.LightChain.empty() ? nullptr : FindMove(m_Def.LightChain[0]));
			if (move)
			{
				StartAttack(animator, *move, riposte ? -1 : 0);
				return;
			}
		}
		if (m_Intent.Heavy)
		{
			if (const MoveDef* heavy = GetHeavyMove(m_Def))
			{
				StartAttack(animator, *heavy, -1);
				return;
			}
		}
		if (!m_Intent.Block)
		{
			LowerBlock(animator, BLOCK_LOWER_FADE);
			EnterLocomotion();
			return;
		}

		if (m_BlockRaising && !animator.IsOneShotPlaying(BLOCK_LAYER) && animator.IsFinished(BLOCK_LAYER))
		{
			animator.Play(m_Context.Assets.GetClip(Clips::BLOCKING), 0.0f, BLOCK_LAYER);
			m_BlockRaising = false;
		}
	}

	void Fighter::ActAttack(Animator& animator)
	{
		if (!animator.IsOneShotPlaying(0))
		{
			EnterLocomotion();
			return;
		}

		if (!m_Move || m_Move->Kind != MoveKind::Light)
			return;

		const MoveDef* next = GetNextInChain(m_Def, m_Move->Clip);
		if (next && IsComboOpen(animator) && m_LightBuffer > 0.0f)
			StartAttack(animator, *next, m_ChainIndex + 1);
	}

	bool Fighter::IsComboOpen(const Animator& animator) const
	{
		if (!m_MoveClip)
			return false;

		const std::optional<ClipRange> combo = FindRange(*m_MoveClip, Events::COMBO);
		return combo && animator.IsEventActive(Events::COMBO) && animator.GetCurrentClip(0) == m_MoveClip && animator.GetTime(0) >= combo->Begin;
	}

	void Fighter::EnterLocomotion()
	{
		m_State = FighterState::Locomotion;
		m_Move = nullptr;
		m_MoveClip = nullptr;
		m_ChainIndex = -1;
		m_BlockRaising = false;
	}

	void Fighter::StartAttack(Animator& animator, const MoveDef& move, int chainIndex)
	{
		const AnimationClip* clip = m_Context.Assets.GetClip(move.Clip);
		if (!clip)
			return;

		LowerBlock(animator, 0.0f);
		m_State = FighterState::Attack;
		m_Move = &move;
		m_MoveClip = clip;
		m_ChainIndex = chainIndex;
		++m_SwingId;
		m_SwingResolved = false;
		m_HitboxPulse = false;
		m_LightBuffer = 0.0f;

		ReleaseTravel(animator, move.FadeIn);
		animator.PlayOneShot(clip, move.FadeIn, move.FadeOut, 0);
		BeginTravel(*clip, move.FadeOut, 0.0f);
		if (m_Context.LogCombat)
		{
			const glm::vec2 net = m_Travel.GetReturnStep() / m_Def.Scale;
			DE_INFO("[Attack] {} {} chain={} swing={} t={:.3f} net hips ({:.3f}, {:.3f}) m", m_Def.Name, move.Clip, chainIndex, m_SwingId, m_Context.Time, net.x, net.y);
		}
	}

	const char* Fighter::PickDodgeClip() const
	{
		const float length = glm::length(m_Intent.Move);
		if (length <= MOVE_DEADZONE)
			return Clips::DODGE_BACKWARD;

		const float relative = GameMath::WrapAngle(GameMath::YawOf(m_Intent.Move / length) - m_Yaw);
		const float angle = std::abs(relative);
		if (angle <= glm::radians(DODGE_FORWARD_ARC_DEG))
			return Clips::DODGE_FORWARD;
		if (angle >= glm::radians(DODGE_BACKWARD_ARC_DEG))
			return Clips::DODGE_BACKWARD;
		return relative > 0.0f ? Clips::DODGE_LEFT : Clips::DODGE_RIGHT;
	}

	void Fighter::StartDodge(Animator& animator)
	{
		const AnimationClip* clip = m_Context.Assets.GetClip(PickDodgeClip());
		if (!clip)
			return;

		LowerBlock(animator, 0.0f);
		m_State = FighterState::Dodge;
		m_Move = nullptr;
		m_MoveClip = nullptr;

		ReleaseTravel(animator, DODGE_FADE_IN);
		animator.PlayOneShot(clip, DODGE_FADE_IN, DODGE_FADE_OUT, 0);
		BeginTravel(*clip, DODGE_FADE_OUT, DODGE_EXTRA_DISTANCE);
		if (m_Context.LogCombat)
		{
			const glm::vec2 net = m_Travel.GetReturnStep() / m_Def.Scale;
			DE_INFO("[Dodge] {} {} t={:.3f} net hips ({:.3f}, {:.3f}) m, extra {:.2f} m", m_Def.Name, clip->GetName(), m_Context.Time, net.x, net.y,
				glm::length(m_Travel.GetDashStep()));
		}
	}

	void Fighter::BeginTravel(const AnimationClip& clip, float fadeOut, float dashDistance)
	{
		m_Travel.Begin(m_Skeleton, clip, m_Def.Scale, fadeOut, dashDistance);
	}

	void Fighter::ReleaseTravel(const Animator& animator, float fadeIn, bool late)
	{
		// The move's own clip is the pose's clock until the one-shot starts returning; after that the animator
		// shows the way back and the travel's clock says how much of it has been paid.
		const AnimationClip* clip = m_Travel.GetClip();
		const float clipTime = clip && animator.GetCurrentClip(0) == clip ? animator.GetTime(0) : m_Travel.GetClock();
		m_Travel.Release(clipTime, fadeIn);

		// The combat pass runs after the velocity went to the controller, and the animator's next update starts
		// the cross-fade this frame, so the carry's first share can't wait for the next one.
		if (late && m_LastDelta > 0.0f)
		{
			const glm::vec2 first = m_Travel.Step(m_LastDelta * m_AnimationRate);
			m_Extra += Rotate(first, m_Yaw) / m_LastDelta;
			if (CharacterController3D* controller = m_Context.World.GetCharacterController(m_Entity))
				PushVelocity(*controller);
		}

		const glm::vec2 carry = m_Travel.GetCarry();
		if (m_Context.LogCombat && glm::length(carry) > 1.0e-3f)
			DE_INFO("[Carry] {} t={:.3f} owes ({:.3f}, {:.3f}) m, paid over {:.2f} s", m_Def.Name, m_Context.Time, carry.x / m_Def.Scale, carry.y / m_Def.Scale, fadeIn);
	}

	glm::vec2 Fighter::StepTravel(float deltaTime)
	{
		if (!(deltaTime > 0.0f))
			return glm::vec2(0.0f);

		return Rotate(m_Travel.Step(deltaTime * m_AnimationRate), m_Yaw) / deltaTime;
	}

	void Fighter::PushVelocity(CharacterController3D& controller) const
	{
		const glm::vec2 total = m_Velocity + m_Extra;
		controller.SetLinearVelocity(glm::vec3(total.x, m_VerticalVelocity, total.y));
	}

	void Fighter::UpdateExtra(float deltaTime)
	{
		const float poseRate = m_AnimationRate / m_Def.Pace;
		m_Extra = m_Knockback * poseRate + StepTravel(deltaTime);
		m_Knockback = GameMath::MoveToward(m_Knockback, glm::vec2(0.0f), BLOCK_PUSH_DECEL * deltaTime * poseRate);
	}

	void Fighter::RaiseBlock(Animator& animator)
	{
		const AnimationClip* raise = m_Context.Assets.GetClip(Clips::BLOCK_RAISE);
		if (!raise)
			return;

		animator.Play(AnimationState::Clip(raise).SetLoop(false), BLOCK_RAISE_FADE, BLOCK_LAYER);
		m_State = FighterState::Block;
		m_BlockUp = true;
		m_BlockRaising = true;
		m_ParryBegun = false;
		if (m_Context.LogCombat)
			DE_INFO("[Block] {} raised t={:.3f}", m_Def.Name, m_Context.Time);
	}

	void Fighter::LowerBlock(Animator& animator, float fadeSeconds)
	{
		if (!m_BlockUp)
			return;

		animator.Stop(fadeSeconds, BLOCK_LAYER);
		m_BlockUp = false;
		m_BlockRaising = false;
		m_RiposteLeft = 0.0f;
	}

	void Fighter::Interrupt(const char* clipName, FighterState state, float fadeIn, float fadeOut, bool late)
	{
		Animator* animator = GetAnimator();
		const AnimationClip* clip = m_Context.Assets.GetClip(clipName);
		if (!animator || !clip)
			return;

		LowerBlock(*animator, 0.0f);
		m_State = state;
		m_Move = nullptr;
		m_MoveClip = nullptr;
		m_ChainIndex = -1;
		m_BlockRaising = false;
		ReleaseTravel(*animator, fadeIn, late);
		animator->PlayOneShot(clip, fadeIn, fadeOut, 0);
	}

	void Fighter::TakeHit(float damage)
	{
		if (!m_Valid || IsDead())
			return;

		m_Health = std::max(m_Health - damage, 0.0f);
		if (m_Health <= 0.0f)
			Die();
		else
			Interrupt(Clips::HIT_REACT, FighterState::HitReact, HIT_REACT_FADE_IN, HIT_REACT_FADE_OUT, true);
	}

	void Fighter::Stagger()
	{
		if (m_Valid && !IsDead())
			Interrupt(Clips::STAGGER, FighterState::Stagger, STAGGER_FADE_IN, STAGGER_FADE_OUT, true);
	}

	void Fighter::Taunt(const char* clipName)
	{
		if (m_Valid && !IsDead() && clipName)
			Interrupt(clipName, FighterState::Taunt, TAUNT_FADE_IN, TAUNT_FADE_OUT, false);
	}

	void Fighter::Die()
	{
		Animator* animator = GetAnimator();
		const AnimationClip* death = m_Context.Assets.GetClip(m_Def.Death);
		if (!animator || !death)
			return;

		LowerBlock(*animator, 0.0f);
		m_State = FighterState::Dead;
		m_Move = nullptr;
		m_MoveClip = nullptr;
		m_ChainIndex = -1;
		ReleaseTravel(*animator, DEATH_FADE, true);
		animator->Play(AnimationState::Clip(death).SetLoop(false), DEATH_FADE);
		if (m_Context.LogCombat)
			DE_INFO("[Dead] {} {} t={:.3f}", m_Def.Name, death->GetName(), m_Context.Time);
	}

	void Fighter::TakeBlock(float chip, const glm::vec2& awayDirection)
	{
		if (!m_Valid || IsDead())
			return;

		m_Health = std::max(m_Health - chip, std::min(m_Health, 1.0f));
		Animator* animator = GetAnimator();
		if (const AnimationClip* hit = m_Context.Assets.GetClip(Clips::BLOCK_HIT); animator && hit)
			animator->PlayOneShot(hit, BLOCK_HIT_FADE_IN, BLOCK_HIT_FADE_OUT, BLOCK_LAYER);
		m_Knockback = awayDirection * BLOCK_PUSH_SPEED;
	}

	void Fighter::OpenRiposteWindow()
	{
		m_RiposteLeft = RIPOSTE_WINDOW;
	}

	bool Fighter::CanRiposte() const
	{
		return m_State == FighterState::Block && m_RiposteLeft > 0.0f;
	}

	bool Fighter::IsParryOpen() const
	{
		return IsWindowActive(Events::PARRY) || (m_BlockRaising && !m_ParryBegun);
	}

	bool Fighter::IsWindowActive(std::string_view name) const
	{
		if (m_PoseClip)
		{
			const std::optional<ClipRange> range = FindRange(*m_PoseClip, name);
			return range && range->Contains(m_PoseTime);
		}

		const Animator* animator = GetAnimator();
		return animator && animator->IsEventActive(name);
	}

	bool Fighter::IsHitboxLive() const
	{
		if (m_State != FighterState::Attack || !m_MoveClip)
			return false;

		const Animator* animator = GetAnimator();
		return animator && animator->GetCurrentClip(0) == m_MoveClip && (animator->IsEventActive(Events::HITBOX) || m_HitboxPulse);
	}

	float Fighter::GetSecondsToHitbox() const
	{
		if (m_State != FighterState::Attack || !m_MoveClip)
			return k_Infinity;

		const std::optional<ClipRange> hitbox = FindRange(*m_MoveClip, Events::HITBOX);
		const Animator* animator = GetAnimator();
		if (!hitbox || !animator || animator->GetCurrentClip(0) != m_MoveClip)
			return k_Infinity;
		return (hitbox->Begin - animator->GetTime(0)) / m_Def.Pace;
	}

	float Fighter::GetHitboxTime() const
	{
		const float time = GetLayerTime(0);
		const Animator* animator = GetAnimator();
		if (!m_MoveClip || !animator || animator->IsEventActive(Events::HITBOX))
			return time;

		const std::optional<ClipRange> hitbox = FindRange(*m_MoveClip, Events::HITBOX);
		return hitbox ? std::clamp(time, hitbox->Begin, hitbox->End) : time;
	}

	const AnimationClip* Fighter::GetLayerClip(uint32_t layer) const
	{
		const Animator* animator = GetAnimator();
		return animator ? animator->GetCurrentClip(layer) : nullptr;
	}

	float Fighter::GetLayerTime(uint32_t layer) const
	{
		const Animator* animator = GetAnimator();
		return animator ? animator->GetTime(layer) : 0.0f;
	}

	void Fighter::CollectHurtSpheres(std::vector<WorldSphere>& out) const
	{
		for (const RigSphere& sphere : m_Hurt)
			out.push_back({ sphere.Node.GetWorldPosition(), sphere.Radius * m_Def.Scale });
	}

	void Fighter::AdvanceWeaponTrail()
	{
		if (!m_HasWeaponSpheres)
			return;

		m_WeaponPrevious = m_WeaponCurrent;
		for (size_t i = 0; i < m_WeaponSpheres.size(); ++i)
			m_WeaponCurrent[i] = m_WeaponSpheres[i].Node.GetWorldPosition();
		if (!m_TrailValid)
		{
			m_WeaponPrevious = m_WeaponCurrent;
			m_TrailValid = true;
		}
	}

	void Fighter::CollectSwingSpheres(std::vector<SweptSphere>& out) const
	{
		if (!m_HasWeaponSpheres)
			return;

		for (size_t i = 0; i < m_WeaponSpheres.size(); ++i)
			out.push_back({ m_WeaponPrevious[i], m_WeaponCurrent[i], m_WeaponSpheres[i].Radius * m_Def.Scale });
	}

	void Fighter::UpdateDebugTint()
	{
		if (!m_Context.Debug || !m_Valid)
			return;

		const bool hitbox = m_PoseClip ? IsWindowActive(Events::HITBOX) : IsHitboxLive();
		DebugTint hurt = DebugTint::Idle;
		if (IsWindowActive(Events::IFRAMES))
			hurt = DebugTint::Iframes;
		else if (IsParryOpen())
			hurt = DebugTint::Parry;

		auto tint = [&](RigSphere& sphere, DebugTint to)
		{
			if (sphere.Node && sphere.Node.HasComponent<MeshRendererComponent>())
				sphere.Node.GetComponent<MeshRendererComponent>().Material = m_Context.Debug->GetMaterial(to);
		};
		for (RigSphere& sphere : m_Hurt)
			tint(sphere, hurt);
		if (m_HasWeaponSpheres)
		{
			for (RigSphere& sphere : m_WeaponSpheres)
				tint(sphere, hitbox ? DebugTint::Hitbox : DebugTint::Idle);
		}
	}

	void Fighter::ThinkMove(float deltaTime, const Fighter* opponent)
	{
		const float intentLength = m_Frozen ? 0.0f : glm::length(m_Intent.Move);
		const bool moving = intentLength > MOVE_DEADZONE;
		const glm::vec2 direction = moving ? m_Intent.Move / intentLength : glm::vec2(0.0f);

		float faceYaw = m_Yaw;
		bool facingOpponent = false;
		if (opponent && m_Intent.FaceOpponent)
		{
			const glm::vec2 toOpponent = opponent->GetGroundPosition() - GetGroundPosition();
			const float distance = glm::length(toOpponent);
			if (distance > 1.0e-3f && distance <= (m_FacingOpponent ? FACE_RANGE_LEAVE : FACE_RANGE_ENTER))
			{
				faceYaw = GameMath::YawOf(toOpponent);
				facingOpponent = true;
			}
		}
		m_FacingOpponent = facingOpponent;
		if (!facingOpponent && moving)
			faceYaw = GameMath::YawOf(direction);

		LocomotionZone zone = LocomotionZone::Forward;
		float targetYaw = faceYaw;
		float targetSpeed = 0.0f;
		if (moving)
		{
			const float travel = GameMath::WrapAngle(GameMath::YawOf(direction) - faceYaw);
			zone = PickZone(m_Zone, travel);

			const float align = glm::radians(ZONE_ALIGN_MAX_DEG);
			targetYaw = faceYaw + std::clamp(GameMath::WrapAngle(travel - ZoneAngle(zone, travel)), -align, align);

			switch (zone)
			{
				case LocomotionZone::StrafeLeft:
				case LocomotionZone::StrafeRight: targetSpeed = STRAFE_SPEED; break;
				case LocomotionZone::Backward:    targetSpeed = BACKPEDAL_SPEED; break;
				default:                          targetSpeed = RUN_SPEED; break;
			}
			targetSpeed *= std::min(intentLength, 1.0f) * GetSpeedFactor();
			if (m_State == FighterState::Block)
				targetSpeed = std::min(targetSpeed, BLOCK_MOVE_SPEED * GetSpeedFactor());
		}
		m_Zone = zone;

		const glm::vec2 target = direction * targetSpeed;
		const float rate = glm::length(target) > glm::length(m_Velocity) ? MOVE_ACCEL : MOVE_DECEL;
		m_Velocity = GameMath::MoveToward(m_Velocity, target, rate * deltaTime);
		m_Yaw = GameMath::ApproachAngle(m_Yaw, targetYaw, glm::radians(FACE_TURN_RATE_DEG) * deltaTime);
	}

	void Fighter::ThinkAttack(float deltaTime, const Fighter* opponent)
	{
		float targetYaw = m_Yaw;
		if (opponent && IsWindowActive(Events::WINDUP))
		{
			const glm::vec2 toOpponent = opponent->GetGroundPosition() - GetGroundPosition();
			const float distance = glm::length(toOpponent);
			if (distance > 1.0e-3f && distance <= FACE_RANGE_LEAVE)
				targetYaw = GameMath::YawOf(toOpponent) + (m_Move ? glm::radians(m_Move->AimDegrees) : 0.0f);
		}
		m_Yaw = GameMath::ApproachAngle(m_Yaw, targetYaw, glm::radians(ATTACK_TURN_RATE_DEG) * deltaTime);
		m_Velocity = GameMath::MoveToward(m_Velocity, glm::vec2(0.0f), MOVE_DECEL * deltaTime);
	}

	void Fighter::ThinkStill(float deltaTime)
	{
		m_Velocity = GameMath::MoveToward(m_Velocity, glm::vec2(0.0f), MOVE_DECEL * deltaTime);
	}

	void Fighter::Separate(Fighter& a, Fighter& b, float deltaTime)
	{
		if (!a.m_Valid || !b.m_Valid)
			return;

		SeparationBody first{ a.GetGroundPosition(), a.m_Velocity + a.m_Extra, a.m_Radius };
		SeparationBody second{ b.GetGroundPosition(), b.m_Velocity + b.m_Extra, b.m_Radius };
		ResolveSeparation(first, second, deltaTime);
		a.m_Extra = first.Velocity - a.m_Velocity;
		b.m_Extra = second.Velocity - b.m_Velocity;
	}

	void Fighter::Apply(float deltaTime)
	{
		if (!m_Valid)
			return;

		m_LastGround = GetGroundPosition();
		m_LastDelta = deltaTime;
		m_Placed = true;

		// The controller dies with the physics world on every OnStop, so it is looked up each frame.
		if (CharacterController3D* controller = m_Context.World.GetCharacterController(m_Entity))
		{
			if (controller->IsGrounded() && m_VerticalVelocity <= 0.0f)
				m_VerticalVelocity = 0.0f;
			else
				m_VerticalVelocity += GRAVITY_Y * deltaTime;

			PushVelocity(*controller);
			controller->SetRotation(GameMath::YawQuat(m_Yaw));
		}

		Animator* animator = m_Frozen ? nullptr : GetAnimator();
		if (!animator || !m_States.Complete)
			return;

		if (m_State == FighterState::Locomotion || m_State == FighterState::Block)
		{
			// A wall stops the body within a frame; easing the blend down keeps the pose from snapping to idle.
			m_MoveParameter = std::max(m_GroundSpeed / GetSpeedFactor(), m_MoveParameter - MOVE_PARAMETER_FALL_RATE * deltaTime);
			animator->SetFloat(MOVE_PARAMETER, m_MoveParameter);
			PlayZone(*animator, deltaTime);
		}
		else
		{
			m_MoveParameter = 0.0f;
			animator->SetFloat(MOVE_PARAMETER, 0.0f);
		}
	}

	void Fighter::PlayZone(Animator& animator, float deltaTime)
	{
		if (m_Zone == m_PlayedZone)
			return;

		SwitchZone(animator, m_States.For(m_PlayedZone), m_States.For(m_Zone), deltaTime * m_AnimationRate);
		m_PlayedZone = m_Zone;
	}

	void Fighter::OnAnimationEvent(const AnimationEvent& event)
	{
		if (event.Name == Events::HITBOX)
		{
			if (event.Type == AnimationEventType::RangeBegin)
			{
				m_HitboxPulse = true;
				if (m_State == FighterState::Attack)
					m_Context.Audio.PlayCombat(CombatSound::Swing, GetPosition(), m_Def.Scale);
			}
			return;
		}

		if (event.Name == Events::PARRY)
		{
			if (event.Type == AnimationEventType::RangeBegin)
				m_ParryBegun = true;
			return;
		}

		if (event.Type != AnimationEventType::Instant)
			return;

		const bool left = event.Name == Events::STEP_LEFT;
		if (!left && event.Name != Events::STEP_RIGHT)
			return;

		m_LastStepTime = m_Context.Time;
		if (m_Context.LogSteps)
			DE_INFO("[Step] {} {} t={:.3f}", m_Def.Name, left ? "l" : "r", m_LastStepTime);

		const float intensity = m_GroundSpeed / (RUN_SPEED * GetSpeedFactor());
		m_Context.Audio.PlayStep(GetPosition(), left, intensity, m_Def.Scale);
	}

}
