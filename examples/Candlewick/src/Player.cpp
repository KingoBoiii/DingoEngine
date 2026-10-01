#include "Player.h"
#include "GameMath.h"
#include "GameTuning.h"
#include "KeepMap.h"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace
{
	using namespace Dingo;

	constexpr float k_BodyHeight = 1.3f;
	constexpr glm::vec3 k_BodySize = { 0.55f, k_BodyHeight, 0.4f };
	constexpr float k_HeadDiameter = 0.4f;
	constexpr float k_HeadRiseFraction = 0.4f;
	constexpr float k_HeadCenter = k_BodyHeight + k_HeadDiameter * k_HeadRiseFraction;
	constexpr glm::vec3 k_VisorSize = { 0.22f, 0.1f, 0.12f };
	constexpr float k_VisorForward = 0.17f;
	constexpr float k_VisorLift = 0.02f;
	constexpr float k_FacingThreshold = 0.1f;

	Entity SpawnVisual(Scene& scene, const char* name, Mesh* mesh, const glm::vec3& size, const glm::vec4& color, Material* material)
	{
		Entity entity = scene.CreateEntity(name);
		entity.AddComponent<Transform3DComponent>().Scale = size;
		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(mesh, color)).Material = material;
		return entity;
	}
}

namespace Dingo
{

	Player::Player(Scene& scene, const KeepMap& map, const glm::ivec2& spawnTile)
		: m_Scene(scene)
	{
		Renderer3D& renderer3D = Application::Get().GetRenderer3D();
		const glm::vec3 feet = map.TileCenter(spawnTile) + glm::vec3(0.0f, PLAYER_SPAWN_LIFT, 0.0f);

		m_Entity = scene.CreateEntity("Player");
		m_Entity.AddComponent<Transform3DComponent>().Position = feet;

		auto& controller = m_Entity.AddComponent<CharacterController3DComponent>();
		controller.Radius = PLAYER_RADIUS;
		controller.Height = PLAYER_HEIGHT;
		controller.StepHeight = PLAYER_STEP;
		controller.MaxSlopeAngle = PLAYER_SLOPE;

		m_CloakMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("PlayerCloak")
			.SetRoughness(CLOAK_ROUGHNESS)
			.SetEmissiveColor(CLOAK_EMISSIVE_COLOR)
			.SetEmissiveStrength(CLOAK_EMISSIVE));

		m_FaceMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("PlayerFace")
			.SetRoughness(FACE_ROUGHNESS)
			.SetEmissiveColor(glm::vec3(COLOR_SKIN))
			.SetEmissiveStrength(FACE_EMISSIVE));

		m_Body = SpawnVisual(scene, "PlayerBody", renderer3D.GetBoxMesh(), k_BodySize, COLOR_CLOAK, m_CloakMaterial);
		m_Head = SpawnVisual(scene, "PlayerHead", renderer3D.GetSphereMesh(), glm::vec3(k_HeadDiameter), COLOR_CLOAK, m_CloakMaterial);
		m_Visor = SpawnVisual(scene, "PlayerVisor", renderer3D.GetBoxMesh(), k_VisorSize, COLOR_SKIN, m_FaceMaterial);

		m_LastFeet = feet;
		PlaceVisuals(feet);
	}

	Player::~Player()
	{
		DestroyAndDelete(m_CloakMaterial);
		DestroyAndDelete(m_FaceMaterial);
	}

	glm::vec3 Player::GetPosition() const
	{
		Entity entity = m_Entity;
		return entity.GetComponent<Transform3DComponent>().Position;
	}

	glm::quat Player::GetFacing() const
	{
		return glm::angleAxis(m_Yaw, glm::vec3(0.0f, 1.0f, 0.0f));
	}

	void Player::Update(float deltaTime)
	{
		// The controller dies with the physics world on every OnStop, so it is looked up each frame.
		CharacterController3D* controller = m_Scene.GetCharacterController(m_Entity);
		if (!controller)
			return;

		// The controller reports the velocity it was given, not the one it achieved, so the step is measured.
		const glm::vec3 feet = controller->GetPosition();
		const float speed = deltaTime > 0.0f ? glm::length(glm::vec2(feet.x - m_LastFeet.x, feet.z - m_LastFeet.z)) / deltaTime : 0.0f;
		m_LastFeet = feet;

		glm::vec3 move(0.0f);
		if (Input::IsKeyDown(Key::W) || Input::IsKeyDown(Key::Up)    || Input::IsGamepadButtonDown(GamepadButton::DPadUp))    move.z -= 1.0f;
		if (Input::IsKeyDown(Key::S) || Input::IsKeyDown(Key::Down)  || Input::IsGamepadButtonDown(GamepadButton::DPadDown))  move.z += 1.0f;
		if (Input::IsKeyDown(Key::A) || Input::IsKeyDown(Key::Left)  || Input::IsGamepadButtonDown(GamepadButton::DPadLeft))  move.x -= 1.0f;
		if (Input::IsKeyDown(Key::D) || Input::IsKeyDown(Key::Right) || Input::IsGamepadButtonDown(GamepadButton::DPadRight)) move.x += 1.0f;

		const glm::vec2 stick = Input::GetGamepadLeftStick();
		move.x += stick.x;
		move.z += stick.y;

		if (m_MovementLocked)
			move = glm::vec3(0.0f);
		else if (glm::length(move) > 1.0f)
			move = glm::normalize(move);

		m_Moving = glm::length(move) > k_FacingThreshold && controller->IsGrounded() && speed > FOOTSTEP_MIN_SPEED;

		if (controller->IsGrounded() && m_VerticalVelocity <= 0.0f)
			m_VerticalVelocity = 0.0f;
		else
			m_VerticalVelocity += GRAVITY_Y * deltaTime;

		glm::vec3 velocity = move * PLAYER_SPEED;
		velocity.y = m_VerticalVelocity;
		controller->SetLinearVelocity(velocity);

		if (glm::length(move) > k_FacingThreshold)
			m_Yaw = ApproachAngle(m_Yaw, std::atan2(move.x, move.z), PLAYER_TURN_SPEED * deltaTime);

		PlaceVisuals(controller->GetPosition());
	}

	void Player::Halt()
	{
		m_MovementLocked = true;
		m_Moving = false;
		if (CharacterController3D* controller = m_Scene.GetCharacterController(m_Entity))
			controller->SetLinearVelocity(glm::vec3(0.0f, m_VerticalVelocity, 0.0f));
	}

	void Player::Teleport(const glm::vec3& feet)
	{
		if (CharacterController3D* controller = m_Scene.GetCharacterController(m_Entity))
		{
			controller->SetPosition(feet);
			controller->SetLinearVelocity(glm::vec3(0.0f));
		}

		// The Scene writes the controller back only after its step; later subsystems this frame read the transform.
		m_Entity.GetComponent<Transform3DComponent>().Position = feet;
		m_VerticalVelocity = 0.0f;
		m_LastFeet = feet;
		PlaceVisuals(feet);
	}

	void Player::PlaceVisuals(const glm::vec3& feet)
	{
		const glm::quat facing = GetFacing();

		auto& body = m_Body.GetComponent<Transform3DComponent>();
		body.Position = feet + glm::vec3(0.0f, k_BodyHeight * 0.5f, 0.0f);
		body.Rotation = facing;

		m_Head.GetComponent<Transform3DComponent>().Position = feet + glm::vec3(0.0f, k_HeadCenter, 0.0f);

		auto& visor = m_Visor.GetComponent<Transform3DComponent>();
		visor.Position = feet + glm::vec3(0.0f, k_HeadCenter + k_VisorLift, 0.0f) + facing * glm::vec3(0.0f, 0.0f, k_VisorForward);
		visor.Rotation = facing;
	}

}
