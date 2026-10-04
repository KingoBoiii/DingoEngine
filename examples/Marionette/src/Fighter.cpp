#include "Fighter.h"
#include "GameMath.h"

#include <algorithm>
#include <format>
#include <string>

namespace Dingo
{

	Fighter::Fighter(const FighterContext& context, const FighterDef& def, const FighterSpawn& spawn)
		: m_Context(context), m_Def(def)
	{
		Scene& scene = context.World;
		m_Entity = scene.CreateEntity(def.Name);

		Model* model = context.Assets.GetCharacter(def);
		if (!model || !model->GetSkeleton())
		{
			DE_ERROR("Marionette: {} has no skinned model; it will not be drawn", def.Name);
			return;
		}

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

		if (def.RightWeapon)
			SpawnWeapon(def.RightWeapon, Joints::HAND_RIGHT);
		if (def.LeftWeapon)
			SpawnWeapon(def.LeftWeapon, Joints::HAND_LEFT);
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

	void Fighter::SpawnWeapon(const char* path, const char* joint)
	{
		const Model* weapon = m_Context.Assets.GetModel(path);
		if (!weapon)
		{
			DE_ERROR("Marionette: {} cannot carry '{}', it did not load", m_Def.Name, path);
			return;
		}

		Material* material = m_Context.Assets.GetMaterial(m_Def);
		const std::string name = std::format("{} {}", m_Def.Name, joint);
		size_t index = 0;
		for (const SubMesh& submesh : weapon->GetSubMeshes())
		{
			if (!submesh.MeshData)
				continue;

			Entity part = m_Context.World.CreateEntity(index == 0 ? name : std::format("{} {}", name, index));
			++index;
			part.AddComponent<Transform3DComponent>();
			part.AddComponent<MeshRendererComponent>(MeshRendererComponent(submesh.MeshData)).Material = material;
			part.SetParent(m_Entity, joint, false);
		}
	}

	void Fighter::ShowIdle(float time, bool freeze)
	{
		Animator* animator = GetAnimator();
		const AnimationClip* idle = m_Context.Assets.GetClip(Clips::IDLE);
		if (!animator || !idle)
			return;

		animator->Play(idle);
		animator->SetTime(time);
		animator->Evaluate();
		m_Entity.GetComponent<AnimatorComponent>().Enabled = !freeze;
		m_Frozen = freeze;
	}

	void Fighter::StartLocomotion(float phase)
	{
		Animator* animator = GetAnimator();
		if (!animator || !m_States.Complete)
			return;

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

		m_MoveParameter = std::max(move, 0.0f);
		animator->SetFloat(MOVE_PARAMETER, m_MoveParameter);
		animator->Play(m_States.Blend);
		animator->SetNormalizedTime(phase);
		animator->Evaluate();
		m_Entity.GetComponent<AnimatorComponent>().Enabled = false;
		m_PlayedZone = LocomotionZone::Forward;
	}

	void Fighter::Think(float deltaTime, const Fighter* opponent)
	{
		if (!m_Valid)
			return;

		// The controller reports the velocity it was given, not the one it achieved, so the step is measured.
		if (m_Placed && m_LastDelta > 0.0f)
		{
			const glm::vec2 achieved = (GetGroundPosition() - m_LastGround) / m_LastDelta;
			if (glm::length(achieved) < glm::length(m_Velocity))
				m_Velocity = achieved;
			m_GroundSpeed = std::min(glm::length(m_Velocity), glm::length(achieved));
		}

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
		}
		m_Zone = zone;

		const glm::vec2 target = direction * targetSpeed;
		const float rate = glm::length(target) > glm::length(m_Velocity) ? MOVE_ACCEL : MOVE_DECEL;
		m_Velocity = GameMath::MoveToward(m_Velocity, target, rate * deltaTime);
		m_Yaw = GameMath::ApproachAngle(m_Yaw, targetYaw, glm::radians(FACE_TURN_RATE_DEG) * deltaTime);
	}

	void Fighter::Separate(Fighter& a, Fighter& b, float deltaTime)
	{
		if (!a.m_Valid || !b.m_Valid)
			return;

		SeparationBody first{ a.GetGroundPosition(), a.m_Velocity, a.m_Radius };
		SeparationBody second{ b.GetGroundPosition(), b.m_Velocity, b.m_Radius };
		ResolveSeparation(first, second, deltaTime);
		a.m_Velocity = first.Velocity;
		b.m_Velocity = second.Velocity;
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

			controller->SetLinearVelocity(glm::vec3(m_Velocity.x, m_VerticalVelocity, m_Velocity.y));
			controller->SetRotation(GameMath::YawQuat(m_Yaw));
		}

		Animator* animator = m_Frozen ? nullptr : GetAnimator();
		if (!animator || !m_States.Complete)
			return;

		// A wall stops the body within a frame; easing the blend down keeps the pose from snapping to idle.
		m_MoveParameter = std::max(m_GroundSpeed / GetSpeedFactor(), m_MoveParameter - MOVE_PARAMETER_FALL_RATE * deltaTime);
		animator->SetFloat(MOVE_PARAMETER, m_MoveParameter);
		PlayZone(*animator, deltaTime);
	}

	void Fighter::PlayZone(Animator& animator, float deltaTime)
	{
		if (m_Zone == m_PlayedZone)
			return;

		SwitchZone(animator, m_States.For(m_PlayedZone), m_States.For(m_Zone), deltaTime * m_Def.Pace);
		m_PlayedZone = m_Zone;
	}

	void Fighter::OnAnimationEvent(const AnimationEvent& event)
	{
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
