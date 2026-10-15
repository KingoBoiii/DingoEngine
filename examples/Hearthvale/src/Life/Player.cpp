#include "Life/Player.h"
#include "GameTuning.h"
#include "Life/PlayerMath.h"
#include "Look/ArtRegistry.h"
#include "Look/FarmerAssets.h"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>

namespace Dingo
{

	namespace
	{
		constexpr float k_HeadLift = 0.1f;
		constexpr float k_FaceForward = 0.17f;
		constexpr float k_FaceLift = 0.02f;

		glm::quat YawQuat(float yaw)
		{
			return glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
		}

		void SpawnPlaceholder(Scene& scene, Entity root)
		{
			const ArtEntry* body = Art::Find(Art::FARMER);
			const ArtEntry* head = Art::Find(Art::FARMER_HEAD);
			if (!body || !head)
				return;

			const float bodyTop = body->Size.y;
			const float headCenter = bodyTop + k_HeadLift;
			const struct { std::string_view Id; const char* Name; glm::vec3 Position; } parts[] = {
				{ Art::FARMER,      "FarmerBody", glm::vec3(0.0f, 0.5f * bodyTop, 0.0f) },
				{ Art::FARMER_HEAD, "FarmerHead", glm::vec3(0.0f, headCenter, 0.0f) },
				{ Art::FARMER_FACE, "FarmerFace", glm::vec3(0.0f, headCenter + k_FaceLift, k_FaceForward) },
			};
			for (const auto& part : parts)
			{
				Entity visual = Art::Spawn(scene, part.Id, part.Name, part.Position);
				if (visual)
					visual.SetParent(root, false);
			}
		}
	}

	Player::Player(Scene& scene, const glm::vec3& feet, const FarmerAssets* assets)
		: m_Scene(scene)
	{
		m_Entity = scene.CreateEntity("Farmer");
		auto& transform = m_Entity.AddComponent<Transform3DComponent>();
		transform.Position = feet;

		auto& controller = m_Entity.AddComponent<CharacterController3DComponent>();
		controller.Radius = PLAYER_RADIUS;
		controller.Height = PLAYER_HEIGHT;
		controller.StepHeight = PLAYER_STEP_HEIGHT;
		controller.MaxSlopeAngle = PLAYER_SLOPE_DEG;

		if (assets && assets->IsUsable() && assets->GetModelHeight() > 0.0f)
		{
			m_Model = true;
			m_Idle = assets->GetIdle();
			m_Walk = assets->GetWalk();
			transform.Scale = glm::vec3(PLAYER_MODEL_HEIGHT / assets->GetModelHeight());
			m_Entity.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(assets->GetModel())).Material = assets->GetMaterial();

			AnimatorComponent animator;
			animator.PlayOnStart = false;
			m_Entity.AddComponent<AnimatorComponent>(animator);
		}
		else
		{
			SpawnPlaceholder(scene, m_Entity);
		}
	}

	glm::vec3 Player::GetFeet() const
	{
		if (CharacterController3D* controller = m_Scene.GetCharacterController(m_Entity))
			return controller->GetPosition();
		Entity entity = m_Entity;
		return entity.GetComponent<Transform3DComponent>().Position;
	}

	void Player::Step(const PlayerIntent& intent, const TileGrid& grid, float cameraYaw, float dt)
	{
		// The controller dies with the physics world on every OnStop, so it is looked up each step.
		CharacterController3D* controller = m_Scene.GetCharacterController(m_Entity);
		if (!controller)
			return;

		glm::vec2 move = intent.Move;
		if (glm::length(move) > 1.0f)
			move = glm::normalize(move);
		const glm::vec2 direction = PlayerMath::CameraRelativeMove(move, cameraYaw);

		const glm::vec3 feet = controller->GetPosition();
		const glm::vec2 allowed = PlayerMath::ResolveMove(grid, glm::vec2(feet.x, feet.z), direction * PLAYER_SPEED * PLAYER_BLOCK_LOOKAHEAD, PLAYER_RADIUS);
		const glm::vec2 horizontal = allowed / PLAYER_BLOCK_LOOKAHEAD;

		if (controller->IsGrounded() && m_VerticalVelocity <= 0.0f)
			m_VerticalVelocity = 0.0f;
		else
			m_VerticalVelocity = (std::max)(m_VerticalVelocity + PLAYER_GRAVITY_Y * dt, PLAYER_TERMINAL_Y);

		controller->SetLinearVelocity(glm::vec3(horizontal.x, m_VerticalVelocity, horizontal.y));

		if (glm::length(direction) > PLAYER_MOVE_THRESHOLD)
			m_Yaw = PlayerMath::ApproachAngle(m_Yaw, PlayerMath::YawOf(direction), PLAYER_TURN_SPEED * dt);
		const glm::quat facing = YawQuat(m_Yaw);
		controller->SetRotation(facing);
		m_Entity.GetComponent<Transform3DComponent>().Rotation = facing;

		m_Moving = glm::length(horizontal) > PLAYER_MOVE_THRESHOLD * PLAYER_SPEED;
		Animate(m_Moving);
	}

	void Player::SetYaw(float yaw)
	{
		m_Yaw = PlayerMath::WrapAngle(yaw);
		m_Entity.GetComponent<Transform3DComponent>().Rotation = YawQuat(m_Yaw);
	}

	void Player::Animate(bool moving)
	{
		if (!m_Model)
			return;

		Animator* animator = m_Scene.GetAnimator(m_Entity);
		if (!animator)
			return;

		const AnimationClip* wanted = moving ? m_Walk : m_Idle;
		if (animator->GetCurrentClip() != wanted)
			animator->Play(wanted, PLAYER_ANIM_FADE);
		m_Entity.GetComponent<AnimatorComponent>().Speed = moving ? PLAYER_WALK_ANIM_SPEED : 1.0f;
	}

	void Player::Teleport(const glm::vec3& feet)
	{
		if (CharacterController3D* controller = m_Scene.GetCharacterController(m_Entity))
		{
			controller->SetPosition(feet);
			controller->SetLinearVelocity(glm::vec3(0.0f));
		}

		m_Entity.GetComponent<Transform3DComponent>().Position = feet;
		m_VerticalVelocity = 0.0f;
	}

	Entity Player::SpawnFloor(Scene& scene, int tilesWide, int tilesDeep)
	{
		constexpr float k_Thickness = 1.0f;
		Entity floor = scene.CreateEntity("PlayerFloor");
		auto& transform = floor.AddComponent<Transform3DComponent>();
		transform.Position = glm::vec3(0.5f * static_cast<float>(tilesWide) * TILE_SIZE, GROUND_TOP - 0.5f * k_Thickness, 0.5f * static_cast<float>(tilesDeep) * TILE_SIZE);
		transform.Scale = glm::vec3(static_cast<float>(tilesWide) * TILE_SIZE, k_Thickness, static_cast<float>(tilesDeep) * TILE_SIZE);
		floor.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
		floor.AddComponent<BoxCollider3DComponent>();
		return floor;
	}

}
