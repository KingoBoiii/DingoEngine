#include "GameScripts.h"
#include "GameTuning.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <format>

namespace
{
	using namespace Dingo;

	// Emissive material for orbs / sentry eyes. Reads the engine scene UBO at binding 0
	// (view-projection) and its own emissive params at binding 1, then adds the emissive
	// term on top of the vertex colour — the game-side counterpart to the engine's built-in
	// emissive channel (MaterialParams::SetEmissiveColor/Strength), used here so only these
	// meshes glow rather than every default-material mesh.
	constexpr const char* k_EmissiveShaderSource = R"(
#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;

layout(std140, binding = 0) uniform CameraData
{
	mat4 ViewProjection;
	vec4 LightDirection;
	vec4 Ambient;
};

layout(location = 0) out vec4 v_Color;
layout(location = 1) out vec3 v_Normal;

void main()
{
	gl_Position = ViewProjection * vec4(a_Position, 1.0);
	v_Color = a_Color;
	v_Normal = a_Normal; // consumed so the shared vertex layout's normal isn't flagged unused
}

#type fragment
#version 450

layout(location = 0) in vec4 v_Color;
layout(location = 1) in vec3 v_Normal;

layout(std140, binding = 1) uniform EmissiveParams
{
	vec4 EmissiveColor; // rgb
	vec4 Params;        // x = strength
};

layout(location = 0) out vec4 o_Color;

void main()
{
	// A touch of facet shading from the normal keeps the geometry readable and consumes
	// the interpolated normal (so the shared layout's attribute is used).
	float facet = 0.85 + 0.15 * (normalize(v_Normal).y * 0.5 + 0.5);
	vec3 color = v_Color.rgb * facet + EmissiveColor.rgb * Params.x;
	o_Color = vec4(color, v_Color.a);
}
)";

	// CPU mirror of the binding-1 UBO (std140: two vec4s).
	struct EmissiveParams
	{
		glm::vec4 EmissiveColor{ 1.0f };
		glm::vec4 Params{ 0.0f }; // x = strength
	};

	glm::quat YawQuat(float yawRadians)
	{
		return glm::angleAxis(yawRadians, glm::vec3(0.0f, 1.0f, 0.0f));
	}

	constexpr const char* k_UiFontPath = "assets/fonts/arialbd.ttf";
	constexpr float k_MenuOrthoSize = 11.0f;

	// Shared UI glue for the Menu/Win/Hud overlay scripts: font load, text entity creation
	// and the orthographic overlay camera all follow the same pattern in each of them.
	Font* LoadUiFont(const char* who)
	{
		Font* font = Font::Create(k_UiFontPath);
		if (!font)
			DE_ERROR("EchoVault: failed to load {} font '{}'", who, k_UiFontPath);
		return font;
	}

	Entity MakeText(Scene& scene, Font* font, const char* name, float size, const glm::vec4& color, bool centered)
	{
		Entity entity = scene.CreateEntity(name);
		auto& text = entity.AddComponent<TextComponent>();
		text.Font = font;
		text.Size = size;
		text.Color = color;
		text.Centered = centered;
		return entity;
	}

	Entity MakeOverlayCamera(Scene& scene, const char* name, float orthoSize)
	{
		Entity entity = scene.CreateEntity(name);
		auto& camera = entity.AddComponent<CameraComponent>();
		camera.Type = CameraComponent::ProjectionType::Orthographic;
		camera.OrthographicSize = orthoSize;
		camera.Primary = true;
		return entity;
	}
}

namespace Dingo
{
	// ======================================================================
	// PingPongPath
	// ======================================================================

	glm::vec3 PingPongPath::Advance(float deltaTime)
	{
		const float length = glm::length(B - A);
		if (length > 0.0001f)
		{
			T += Dir * (Speed / length) * deltaTime;
			if (T >= 1.0f) { T = 1.0f; Dir = -1; }
			else if (T <= 0.0f) { T = 0.0f; Dir = 1; }
		}
		return glm::mix(A, B, T);
	}

	// ======================================================================
	// CourseControllerScript
	// ======================================================================

	void CourseControllerScript::OnStart()
	{
		Renderer3D& renderer3D = Application::Get().GetRenderer3D();
		m_Context.BoxMesh = renderer3D.GetBoxMesh();
		m_Context.SphereMesh = renderer3D.GetSphereMesh();

		GetScene().SetClearColor(COLOR_BG);
		GetScene().SetGravity(GRAVITY);

		LoadAudio();

		m_EmissiveShader = Shader::CreateFromSource("EchoVaultEmissive", k_EmissiveShaderSource);

		m_OrbMaterial = Material::Create(MaterialParams()
			.SetDebugName("OrbEmissive")
			.SetShader(m_EmissiveShader)
			.SetCullMode(CullMode::None));
		m_OrbMaterial->SetUniform(EmissiveParams{ glm::vec4(glm::vec3(COLOR_ORB), 1.0f), glm::vec4(ORB_EMISSIVE, 0.0f, 0.0f, 0.0f) });

		m_SentryEyeMaterial = Material::Create(MaterialParams()
			.SetDebugName("SentryEyeEmissive")
			.SetShader(m_EmissiveShader)
			.SetCullMode(CullMode::None));
		m_SentryEyeMaterial->SetUniform(EmissiveParams{ glm::vec4(glm::vec3(COLOR_SENTRY_EYE), 1.0f), glm::vec4(SENTRY_EMISSIVE, 0.0f, 0.0f, 0.0f) });

		SetupCameraAndLight();
		BuildCourse();

		GetScene().CreateEntity("Hud").AddScript<HudScript>(&m_Context);

		// Ambient pad: non-spatialized looping bed. One-shot Play (2D) via AudioEngine so it
		// isn't tied to an entity's position.
		if (m_Context.AmbientClip)
		{
			SoundPlayParams params;
			params.Looping = true;
			params.Volume = 0.5f;
			params.Spatialized = false;
			Application::Get().GetAudioEngine().Play(m_Context.AmbientClip, params);
		}

		UpdateCamera(0.0f);
		m_CameraInitialized = true;
	}

	void CourseControllerScript::LoadAudio()
	{
		AudioEngine& audio = Application::Get().GetAudioEngine();

		auto load = [&](const char* path) -> std::shared_ptr<AudioClip>
		{
			std::shared_ptr<AudioClip> clip = audio.LoadClip(path);
			if (!clip)
				DE_ERROR("EchoVault: failed to load audio clip '{}'", path);
			return clip;
		};

		m_Context.OrbClip = load(AUDIO_ORB);
		m_Context.HumClip = load(AUDIO_HUM);
		m_Context.AmbientClip = load(AUDIO_AMBIENT);
		m_Context.FootstepClip = load(AUDIO_FOOTSTEP);
		m_Context.PickupClip = load(AUDIO_PICKUP);
		m_Context.JumpClip = load(AUDIO_JUMP);
		m_Context.DetectionClip = load(AUDIO_DETECTION);
	}

	Entity CourseControllerScript::SpawnPlatform(const glm::vec3& center, const glm::vec3& size, const glm::vec4& color)
	{
		Entity entity = GetScene().CreateEntity("Platform");
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = center;
		transform.Scale = size;

		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Context.BoxMesh, color));
		entity.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
		entity.AddComponent<BoxCollider3DComponent>();
		return entity;
	}

	Entity CourseControllerScript::SpawnMovingPlatform(const glm::vec3& a, const glm::vec3& b, const glm::vec3& size, float speed)
	{
		Entity entity = GetScene().CreateEntity("MovingPlatform");
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = a;
		transform.Scale = size;

		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Context.BoxMesh, COLOR_MOVING));
		entity.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Kinematic));
		entity.AddComponent<BoxCollider3DComponent>();

		// A humming spatialized loop so you can hear a platform approach.
		if (m_Context.HumClip)
		{
			auto& source = entity.AddComponent<AudioSourceComponent>();
			source.Clip = m_Context.HumClip;
			source.Looping = true;
			source.Spatialized = true;
			source.Volume = 0.7f;
			source.PlayOnStart = true;
		}

		entity.AddScript<MovingPlatformScript>(&m_Context, a, b, speed);
		return entity;
	}

	Entity CourseControllerScript::SpawnPlayer(const glm::vec3& feetPos)
	{
		Entity entity = GetScene().CreateEntity("Player");
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = feetPos;
		transform.Scale = glm::vec3(1.0f);

		auto& cc = entity.AddComponent<CharacterController3DComponent>();
		cc.Radius = PLAYER_RADIUS;
		cc.Height = PLAYER_HEIGHT;
		cc.StepHeight = PLAYER_STEP;
		cc.MaxSlopeAngle = PLAYER_SLOPE;

		// Visible body: a capsule-ish stack (sphere head + box torso) drawn at the feet.
		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Context.BoxMesh, COLOR_PLAYER));

		entity.AddScript<PlayerScript>(&m_Context);
		return entity;
	}

	Entity CourseControllerScript::SpawnOrb(const glm::vec3& position)
	{
		Entity entity = GetScene().CreateEntity("Orb");
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = position;
		transform.Scale = glm::vec3(0.6f);

		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Context.SphereMesh, COLOR_ORB)).Material = m_OrbMaterial;

		if (m_Context.OrbClip)
		{
			auto& source = entity.AddComponent<AudioSourceComponent>();
			source.Clip = m_Context.OrbClip;
			source.Looping = true;
			source.Spatialized = true;
			source.Volume = 0.9f;
			source.PlayOnStart = true;
		}

		entity.AddScript<OrbScript>(&m_Context, static_cast<float>(m_Context.TotalOrbs));
		m_Context.TotalOrbs++;
		return entity;
	}

	Entity CourseControllerScript::SpawnSentry(const glm::vec3& a, const glm::vec3& b, float speed)
	{
		Entity entity = GetScene().CreateEntity("Sentry");
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = a;
		transform.Scale = { 0.9f, 1.6f, 0.9f };

		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Context.BoxMesh, COLOR_SENTRY));
		entity.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Kinematic));
		entity.AddComponent<BoxCollider3DComponent>();

		// A glowing "eye" child (separate entity) that renders emissive in front of the sentry.
		Entity eye = GetScene().CreateEntity("SentryEye");
		auto& eyeTransform = eye.AddComponent<Transform3DComponent>();
		eyeTransform.Position = a + glm::vec3(0.0f, 0.4f, 0.0f);
		eyeTransform.Scale = glm::vec3(0.35f);
		eye.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Context.SphereMesh, COLOR_SENTRY_EYE)).Material = m_SentryEyeMaterial;

		entity.AddScript<SentryScript>(&m_Context, a, b, speed);
		return entity;
	}

	Entity CourseControllerScript::SpawnGoalPad(const glm::vec3& center)
	{
		// Purely visual marker for the final platform (a bright pad you can see is the end).
		Entity entity = GetScene().CreateEntity("GoalPad");
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = center;
		transform.Scale = { 3.0f, 0.15f, 3.0f };
		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Context.BoxMesh, COLOR_GOAL));
		return entity;
	}

	void CourseControllerScript::BuildCourse()
	{
		m_Context.Collected = 0;
		m_Context.TotalOrbs = 0;
		m_Context.ElapsedTime = 0.0f;

		const float t = PLATFORM_THICKNESS;

		// Handcrafted floating course. Platforms are laid roughly along +/-X and stepping in
		// Z, with height changes to exercise jumps, steps and slopes.
		SpawnPlatform({ 0.0f, 0.0f, 0.0f }, { 6.0f, t, 6.0f }, COLOR_PLATFORM);       // start
		SpawnPlatform({ 0.0f, 0.6f, -6.5f }, { 4.0f, t, 4.0f }, COLOR_PLATFORM2);     // small step up
		SpawnPlatform({ 6.0f, 1.2f, -10.0f }, { 4.0f, t, 4.0f }, COLOR_PLATFORM);     // jump gap right
		SpawnPlatform({ 6.0f, 1.2f, -17.0f }, { 5.0f, t, 4.0f }, COLOR_PLATFORM2);    // sentry lane
		SpawnPlatform({ -1.0f, 2.0f, -22.0f }, { 5.0f, t, 4.0f }, COLOR_PLATFORM);    // higher
		SpawnPlatform({ -1.0f, 2.6f, -30.0f }, { 6.0f, t, 6.0f }, COLOR_PLATFORM2);   // goal platform

		// A ramp (slope) linking the start to the second-step platform edge.
		Entity ramp = SpawnPlatform({ 0.0f, 0.3f, -3.4f }, { 3.0f, t, 3.0f }, COLOR_PLATFORM);
		ramp.GetComponent<Transform3DComponent>().SetRotationEuler({ -18.0f, 0.0f, 0.0f });

		// Moving kinematic platforms bridging the gaps.
		SpawnMovingPlatform({ 3.0f, 1.0f, -8.0f }, { 6.0f, 1.2f, -8.0f }, { 2.6f, t, 2.6f }, 2.5f); // start-right lateral
		SpawnMovingPlatform({ 6.0f, 1.6f, -19.5f }, { 2.5f, 2.0f, -21.0f }, { 2.6f, t, 2.6f }, 2.0f); // to the higher one
		SpawnMovingPlatform({ -1.0f, 2.3f, -26.0f }, { -1.0f, 2.6f, -30.0f }, { 2.4f, t, 2.4f }, 1.8f); // to goal

		m_Context.SpawnPoint = { 0.0f, t * 0.5f + 0.05f, 0.0f };
		m_Context.Player = SpawnPlayer(m_Context.SpawnPoint);

		// Orbs scattered so several are only reachable across the moving platforms; positioned
		// a little above each platform so they float and chime.
		SpawnOrb({ 2.0f, 1.4f, -1.5f });   // off the spawn point so it isn't auto-collected
		SpawnOrb({ 0.0f, 1.8f, -6.5f });
		SpawnOrb({ 6.0f, 2.4f, -10.0f });
		SpawnOrb({ 6.0f, 2.4f, -17.0f });
		SpawnOrb({ -1.0f, 3.2f, -22.0f });
		SpawnOrb({ 4.6f, 2.6f, -19.5f });   // reachable via the second moving platform
		SpawnOrb({ -1.0f, 3.8f, -30.0f });  // on the goal platform

		// Two sentries guarding the mid-course lanes.
		SpawnSentry({ 6.0f, 1.8f, -15.5f }, { 6.0f, 1.8f, -18.5f }, 2.2f);
		SpawnSentry({ -3.0f, 2.6f, -22.0f }, { 1.0f, 2.6f, -22.0f }, 2.6f);

		SpawnGoalPad({ -1.0f, 2.6f + PLATFORM_THICKNESS * 0.5f + 0.08f, -30.0f });
	}

	void CourseControllerScript::SetupCameraAndLight()
	{
		m_CameraEntity = GetScene().CreateEntity("Camera");
		auto& camera = m_CameraEntity.AddComponent<CameraComponent>();
		camera.Type = CameraComponent::ProjectionType::Perspective;
		camera.FOV = 55.0f;
		camera.PerspNear = 0.1f;
		camera.PerspFar = 500.0f;
		camera.Primary = true;
		m_CameraEntity.AddComponent<Transform3DComponent>();

		// The listener rides the camera so spatialized audio is heard from the player's view.
		m_CameraEntity.AddComponent<AudioListenerComponent>();

		GetScene().CreateEntity("Sun").AddComponent<DirectionalLightComponent>();
	}

	void CourseControllerScript::UpdateCamera(float deltaTime)
	{
		if (!m_CameraEntity.IsValid())
			return;

		const glm::vec3 focus = m_Context.Player.IsValid()
			? m_Context.Player.GetComponent<Transform3DComponent>().Position
			: glm::vec3(0.0f);

		const glm::vec3 desiredEye = focus + CAMERA_OFFSET;
		const glm::vec3 target = focus + glm::vec3(0.0f, 1.0f, 0.0f);

		auto& transform = m_CameraEntity.GetComponent<Transform3DComponent>();

		// Smoothly chase the desired eye so the follow feels less rigid than a hard snap.
		glm::vec3 eye = transform.Position;
		if (!m_CameraInitialized || deltaTime <= 0.0f)
			eye = desiredEye;
		else
			eye = glm::mix(eye, desiredEye, std::min(1.0f, CAMERA_LAG * deltaTime));

		transform.Position = eye;
		transform.Rotation = glm::quat_cast(glm::inverse(glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f))));
	}

	void CourseControllerScript::OnUpdate(float deltaTime)
	{
		m_Context.ElapsedTime += deltaTime;

		UpdateCamera(deltaTime);

		// Pulse the shared emissive materials.
		const float pulse = 0.75f + 0.25f * std::sin(m_Context.ElapsedTime * 3.0f);
		if (m_OrbMaterial)
			m_OrbMaterial->SetUniform(EmissiveParams{ glm::vec4(glm::vec3(COLOR_ORB), 1.0f), glm::vec4(ORB_EMISSIVE * pulse, 0.0f, 0.0f, 0.0f) });
		if (m_SentryEyeMaterial)
			m_SentryEyeMaterial->SetUniform(EmissiveParams{ glm::vec4(glm::vec3(COLOR_SENTRY_EYE), 1.0f), glm::vec4(SENTRY_EMISSIVE * pulse, 0.0f, 0.0f, 0.0f) });

		if (m_Context.TotalOrbs > 0 && m_Context.Collected >= m_Context.TotalOrbs)
			RequestSceneTransition("Win"); // script-driven Game -> Win transition
	}

	void CourseControllerScript::OnDestroy()
	{
		if (m_OrbMaterial)       { m_OrbMaterial->Destroy();       delete m_OrbMaterial;       m_OrbMaterial = nullptr; }
		if (m_SentryEyeMaterial) { m_SentryEyeMaterial->Destroy(); delete m_SentryEyeMaterial; m_SentryEyeMaterial = nullptr; }
		if (m_EmissiveShader)    { m_EmissiveShader->Destroy();    m_EmissiveShader = nullptr; }
	}

	// ======================================================================
	// PlayerScript
	// ======================================================================

	void PlayerScript::OnStart()
	{
		m_VerticalVelocity = 0.0f;
	}

	void PlayerScript::ApplyKnockback(const glm::vec3& horizontalDir)
	{
		m_PendingKnockback = horizontalDir;
	}

	void PlayerScript::OnUpdate(float deltaTime)
	{
		if (!m_Controller)
			m_Controller = GetScene().GetCharacterController(GetEntity());
		CharacterController3D* controller = m_Controller;
		if (!controller)
			return;

		// Respawn if we've fallen off the course.
		if (controller->GetPosition().y < RESPAWN_Y)
		{
			controller->SetPosition(m_Context->SpawnPoint); // teleport
			controller->SetLinearVelocity(glm::vec3(0.0f));
			m_VerticalVelocity = 0.0f;
			return;
		}

		const bool grounded = controller->IsGrounded();

		// Horizontal input: keyboard, d-pad, or analog left stick.
		glm::vec3 move(0.0f);
		if (Input::IsKeyDown(Key::W) || Input::IsKeyDown(Key::Up)    || Input::IsGamepadButtonDown(GamepadButton::DPadUp))    move.z -= 1.0f;
		if (Input::IsKeyDown(Key::S) || Input::IsKeyDown(Key::Down)  || Input::IsGamepadButtonDown(GamepadButton::DPadDown))  move.z += 1.0f;
		if (Input::IsKeyDown(Key::A) || Input::IsKeyDown(Key::Left)  || Input::IsGamepadButtonDown(GamepadButton::DPadLeft))  move.x -= 1.0f;
		if (Input::IsKeyDown(Key::D) || Input::IsKeyDown(Key::Right) || Input::IsGamepadButtonDown(GamepadButton::DPadRight)) move.x += 1.0f;

		const glm::vec2 stick = Input::GetGamepadLeftStick(); // +Y = down = toward the camera = +Z
		move.x += stick.x;
		move.z += stick.y;

		if (glm::length(move) > 1.0f)
			move = glm::normalize(move);
		glm::vec3 horizontal = move * PLAYER_SPEED; // stick magnitude scales walk speed

		// Ride moving platforms: add the ground's velocity while standing on it.
		if (grounded)
		{
			const glm::vec3 groundVel = controller->GetGroundVelocity();
			horizontal.x += groundVel.x;
			horizontal.z += groundVel.z;
		}

		// Vertical: reset fall speed on the ground, integrate gravity while airborne.
		if (grounded && m_VerticalVelocity <= 0.0f)
			m_VerticalVelocity = 0.0f;
		else
			m_VerticalVelocity += GRAVITY.y * deltaTime;

		// Jump on the grounded rising edge.
		if (grounded && (Input::IsKeyPressed(Key::Space) || Input::IsGamepadButtonPressed(GamepadButton::A)))
		{
			m_VerticalVelocity = JUMP_SPEED;
			if (m_Context->JumpClip)
				Application::Get().GetAudioEngine().PlayOneShot(m_Context->JumpClip, 0.7f);
		}

		// Knockback overrides horizontal this frame (applied by a sentry).
		if (m_PendingKnockback.has_value())
		{
			horizontal = m_PendingKnockback.value() * SENTRY_KNOCKBACK;
			m_VerticalVelocity = SENTRY_KNOCK_UP;
			m_PendingKnockback.reset();
		}

		glm::vec3 velocity = horizontal;
		velocity.y = m_VerticalVelocity;
		controller->SetLinearVelocity(velocity); // the Scene calls controller->Update() after scripts

		// Footsteps: timed spatialized one-shots while moving on the ground.
		const float horizSpeed = glm::length(glm::vec2(horizontal.x, horizontal.z));
		if (grounded && horizSpeed > 0.5f)
		{
			m_FootstepTimer -= deltaTime;
			if (m_FootstepTimer <= 0.0f)
			{
				if (m_Context->FootstepClip)
					Application::Get().GetAudioEngine().PlayOneShot(m_Context->FootstepClip, controller->GetPosition(), 0.5f);
				m_FootstepTimer = 0.35f;
			}
		}
		else
		{
			m_FootstepTimer = 0.0f;
		}

		// Face the way we move (the visible box body).
		if (horizSpeed > 0.1f)
		{
			const float yaw = std::atan2(horizontal.x, horizontal.z);
			GetComponent<Transform3DComponent>().Rotation = YawQuat(yaw);
		}
	}

	// ======================================================================
	// MovingPlatformScript
	// ======================================================================

	void MovingPlatformScript::OnUpdate(float deltaTime)
	{
		if (deltaTime <= 0.0f)
			return;

		Physics3D* physics = GetScene().GetPhysics3D();
		if (!physics)
			return;

		const std::uint32_t bodyId = GetComponent<RigidBody3DComponent>().RuntimeBody;
		if (bodyId == k_InvalidBody3D)
			return;

		const glm::vec3 target = m_Path.Advance(deltaTime);

		// MoveKinematic drives the body with a velocity that reaches the target in one step,
		// so resting bodies (and the character controller's ground query) are carried along.
		physics->MoveKinematic(bodyId, target, GetComponent<Transform3DComponent>().Rotation, deltaTime);
	}

	// ======================================================================
	// OrbScript
	// ======================================================================

	void OrbScript::OnUpdate(float deltaTime)
	{
		auto& transform = GetComponent<Transform3DComponent>();
		if (!m_BaseYSet)
		{
			m_BaseY = transform.Position.y;
			m_BaseYSet = true;
		}

		transform.SetRotationEuler({ 0.0f, m_Context->ElapsedTime * ORB_SPIN_DEG, 0.0f });
		transform.Position.y = m_BaseY + std::sin(m_Context->ElapsedTime * 2.5f + m_Phase) * ORB_BOB;

		if (!m_Context->Player.IsValid())
			return;

		const glm::vec3 playerPos = m_Context->Player.GetComponent<Transform3DComponent>().Position;
		const glm::vec3 orbPos = transform.Position;
		const float dx = playerPos.x - orbPos.x;
		const float dy = playerPos.y - orbPos.y;
		const float dz = playerPos.z - orbPos.z;
		constexpr float k_CollectRadiusSq = ORB_COLLECT_RADIUS * ORB_COLLECT_RADIUS;
		if (dx * dx + dy * dy + dz * dz < k_CollectRadiusSq)
		{
			m_Context->Collected++;
			if (m_Context->PickupClip)
				Application::Get().GetAudioEngine().PlayOneShot(m_Context->PickupClip, 0.8f);
			GetScene().StopAudioSource(GetEntity()); // silence its chime before it vanishes
			GetEntity().Destroy();
		}
	}

	// ======================================================================
	// SentryScript
	// ======================================================================

	void SentryScript::OnStart()
	{
		m_Facing = glm::normalize(m_Path.B - m_Path.A);
		if (glm::length(m_Facing) < 0.0001f)
			m_Facing = { 0.0f, 0.0f, 1.0f };
	}

	bool SentryScript::HasLineOfSight(const glm::vec3& eye, const glm::vec3& target) const
	{
		Physics3D* physics = GetScene().GetPhysics3D();
		if (!physics)
			return false;

		glm::vec3 toTarget = target - eye;
		const float dist = glm::length(toTarget);
		if (dist < 0.0001f)
			return true;
		toTarget /= dist;

		// The character controller is NOT a rigid body, so the ray can't hit the player
		// directly. Instead: line of sight is clear if nothing (a platform/wall/sentry body)
		// blocks the ray before it reaches the player's distance.
		RayCastHit3D hit;
		if (physics->RayCast(Ray(eye, toTarget), dist, hit))
			return hit.Fraction * dist > dist - 0.5f; // first hit is at/behind the player
		return true; // nothing in the way
	}

	void SentryScript::OnUpdate(float deltaTime)
	{
		if (m_Cooldown > 0.0f)
			m_Cooldown -= deltaTime;

		Physics3D* physics = GetScene().GetPhysics3D();
		const std::uint32_t bodyId = GetComponent<RigidBody3DComponent>().RuntimeBody;

		// Patrol (kinematic ping-pong).
		if (physics && bodyId != k_InvalidBody3D && deltaTime > 0.0f)
		{
			const int dirBefore = m_Path.Dir;
			const glm::vec3 target = m_Path.Advance(deltaTime);
			if (m_Path.Dir != dirBefore)
				m_Facing = (m_Path.Dir < 0) ? glm::normalize(m_Path.A - m_Path.B) : glm::normalize(m_Path.B - m_Path.A);
			physics->MoveKinematic(bodyId, target, YawQuat(std::atan2(m_Facing.x, m_Facing.z)), deltaTime);
		}

		if (!m_Context->Player.IsValid() || m_Cooldown > 0.0f)
			return;

		const glm::vec3 sentryPos = GetComponent<Transform3DComponent>().Position;
		const glm::vec3 eye = sentryPos + glm::vec3(0.0f, 0.4f, 0.0f);
		const glm::vec3 playerPos = m_Context->Player.GetComponent<Transform3DComponent>().Position;
		const glm::vec3 playerCenter = playerPos + glm::vec3(0.0f, PLAYER_HEIGHT * 0.5f, 0.0f);

		glm::vec3 toPlayer = playerCenter - eye;
		const float dist = glm::length(toPlayer);
		if (dist > SENTRY_VIEW_RANGE || dist < 0.0001f)
			return;
		toPlayer /= dist;

		// View cone: is the player within the sentry's facing cone?
		if (glm::dot(glm::normalize(glm::vec3(m_Facing.x, 0.0f, m_Facing.z)), glm::vec3(toPlayer.x, 0.0f, toPlayer.z)) < SENTRY_VIEW_CONE)
			return;

		if (!HasLineOfSight(eye, playerCenter))
			return;

		// Detected: knock the player back along the sentry->player direction and sting.
		if (PlayerScript* player = m_Context->Player.GetScript<PlayerScript>())
		{
			glm::vec3 knockDir = glm::normalize(glm::vec3(toPlayer.x, 0.0f, toPlayer.z));
			player->ApplyKnockback(knockDir);
		}
		if (m_Context->DetectionClip)
			Application::Get().GetAudioEngine().PlayOneShot(m_Context->DetectionClip, sentryPos, 0.9f);

		for (HudScript* hud : GetScene().GetScriptsOfType<HudScript>())
			hud->FlashAlert();

		m_Cooldown = SENTRY_COOLDOWN;
	}

	// ======================================================================
	// HudScript
	// ======================================================================

	void HudScript::FlashAlert()
	{
		m_Alert = 1.2f;
	}

	void HudScript::OnStart()
	{
		m_Font = LoadUiFont("HUD");

		Scene& scene = GetScene();
		MakeOverlayCamera(scene, "UICamera", m_OrthoSize);

		m_OrbText   = MakeText(scene, m_Font, "HudOrbs", 0.6f, COLOR_ORB, false);
		m_HintText  = MakeText(scene, m_Font, "HudHint", 0.4f, COLOR_TEXT_DIM, true);
		m_AlertText = MakeText(scene, m_Font, "HudAlert", 0.7f, COLOR_SENTRY, true);

		m_HintText.GetComponent<TextComponent>().Text =
			"WASD move   SPACE jump   listen for the chimes   avoid the red sentries";
	}

	void HudScript::OnUpdate(float deltaTime)
	{
		if (m_Alert > 0.0f)
			m_Alert -= deltaTime;

		const glm::vec2 viewport = Application::Get().GetRenderer2D().GetViewportSize();
		const float aspect = (viewport.y > 0.0f) ? viewport.x / viewport.y : 1.0f;
		const float halfH = m_OrthoSize * 0.5f;
		const float halfW = halfH * aspect;
		const float pad = 0.45f;

		m_OrbText.GetComponent<TransformComponent>().Position = { -halfW + pad, halfH - 0.95f, 0.0f };
		if (m_Context->Collected != m_LastCollected || m_Context->TotalOrbs != m_LastTotalOrbs)
		{
			m_OrbText.GetComponent<TextComponent>().Text = std::format("ORBS  {}/{}", m_Context->Collected, m_Context->TotalOrbs);
			m_LastCollected = m_Context->Collected;
			m_LastTotalOrbs = m_Context->TotalOrbs;
		}

		m_HintText.GetComponent<TransformComponent>().Position = { 0.0f, -halfH + 0.45f, 0.0f };

		auto& alert = m_AlertText.GetComponent<TextComponent>();
		m_AlertText.GetComponent<TransformComponent>().Position = { 0.0f, 0.6f, 0.0f };
		if (m_Alert > 0.0f)
		{
			if (!m_AlertActive)
			{
				alert.Text = "SPOTTED!";
				m_AlertActive = true;
			}
			alert.Color = { COLOR_SENTRY.r, COLOR_SENTRY.g, COLOR_SENTRY.b, std::min(1.0f, m_Alert) };
		}
		else if (m_AlertActive)
		{
			alert.Text.clear();
			m_AlertActive = false;
		}
	}

	void HudScript::OnDestroy()
	{
		if (m_Font)
		{
			m_Font->Destroy();
			m_Font = nullptr;
		}
	}

	// ======================================================================
	// MenuControllerScript
	// ======================================================================

	void MenuControllerScript::OnStart()
	{
		m_Font = LoadUiFont("menu");

		Scene& scene = GetScene();
		MakeOverlayCamera(scene, "MenuCamera", k_MenuOrthoSize);

		auto makeText = [&](const char* name, float size, const glm::vec4& color, const glm::vec3& pos, const char* str)
		{
			Entity entity = MakeText(scene, m_Font, name, size, color, true);
			entity.GetComponent<TextComponent>().Text = str;
			entity.GetComponent<TransformComponent>().Position = pos;
		};

		makeText("Title", 1.2f, { 0.55f, 0.9f, 0.98f, 1.0f }, { 0.0f, 3.0f, 0.0f }, "ECHOVAULT");
		makeText("Prompt", 0.6f, COLOR_TEXT, { 0.0f, 0.3f, 0.0f }, "Press SPACE or (A) to begin");
		makeText("Blurb", 0.4f, COLOR_TEXT_DIM, { 0.0f, -1.4f, 0.0f },
			"Cross the floating vault by ear - collect every chiming orb to win");
		makeText("Footer", 0.34f, { 0.5f, 0.52f, 0.62f, 1.0f }, { 0.0f, -4.6f, 0.0f },
			"A Dingo Engine v0.5 example - character controller + 3D positional audio");
	}

	void MenuControllerScript::OnUpdate(float)
	{
		if (Input::IsKeyPressed(Key::Space) || Input::IsKeyPressed(Key::Enter)
			|| Input::IsGamepadButtonPressed(GamepadButton::A) || Input::IsGamepadButtonPressed(GamepadButton::Start))
			RequestSceneTransition("Game");
	}

	void MenuControllerScript::OnDestroy()
	{
		if (m_Font)
		{
			m_Font->Destroy();
			m_Font = nullptr;
		}
	}

	// ======================================================================
	// WinControllerScript
	// ======================================================================

	void WinControllerScript::OnStart()
	{
		m_Font = LoadUiFont("win");

		Scene& scene = GetScene();
		MakeOverlayCamera(scene, "WinCamera", k_MenuOrthoSize);

		auto makeText = [&](const char* name, float size, const glm::vec4& color, const glm::vec3& pos, const char* str)
		{
			Entity entity = MakeText(scene, m_Font, name, size, color, true);
			entity.GetComponent<TextComponent>().Text = str;
			entity.GetComponent<TransformComponent>().Position = pos;
		};

		makeText("WinTitle", 1.2f, COLOR_GOAL, { 0.0f, 2.5f, 0.0f }, "VAULT CLEARED!");
		makeText("WinSub", 0.55f, COLOR_TEXT, { 0.0f, 0.0f, 0.0f }, "Every orb collected.");
		makeText("WinPrompt", 0.42f, COLOR_TEXT_DIM, { 0.0f, -2.0f, 0.0f }, "Press SPACE or (A) to return to the menu");

		// Win fanfare (2D one-shot).
		std::shared_ptr<AudioClip> winClip = Application::Get().GetAudioEngine().LoadClip("assets/audio/win.wav");
		if (winClip)
			Application::Get().GetAudioEngine().PlayOneShot(winClip, 0.9f);
		else
			DE_ERROR("EchoVault: failed to load 'assets/audio/win.wav'");
	}

	void WinControllerScript::OnUpdate(float)
	{
		if (Input::IsKeyPressed(Key::Space) || Input::IsKeyPressed(Key::Enter)
			|| Input::IsGamepadButtonPressed(GamepadButton::A) || Input::IsGamepadButtonPressed(GamepadButton::Start))
			RequestSceneTransition("Menu");
	}

	void WinControllerScript::OnDestroy()
	{
		if (m_Font)
		{
			m_Font->Destroy();
			m_Font = nullptr;
		}
	}
}
