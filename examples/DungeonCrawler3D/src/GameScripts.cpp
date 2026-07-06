#include "GameScripts.h"
#include "DungeonGenerator.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <format>

namespace
{
	// Unlit "glow" material for the treasures: tints the mesh's vertex colour toward a
	// glow colour by a pulsing intensity. Showcases a custom per-mesh shader that reads
	// the engine scene UBO at binding 0 (for the view-projection) plus its own material
	// uniform at binding 1.
	constexpr const char* k_GlowShaderSource = R"(
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
	v_Normal = a_Normal;
}

#type fragment
#version 450

layout(location = 0) in vec4 v_Color;
layout(location = 1) in vec3 v_Normal;

layout(std140, binding = 1) uniform GlowParams
{
	vec4 GlowColor; // rgb tint
	vec4 Params;    // x = intensity
};

layout(location = 0) out vec4 o_Color;

void main()
{
	// Emissive tint toward GlowColor by the pulsing intensity, with a touch of facet
	// shading from the normal (which also keeps the shared layout's normal consumed).
	float facet = 0.8 + 0.2 * (normalize(v_Normal).y * 0.5 + 0.5);
	vec3 color = mix(v_Color.rgb, GlowColor.rgb, clamp(Params.x, 0.0, 1.0)) * facet;
	o_Color = vec4(color, v_Color.a);
}
)";

	// CPU mirror of the binding-1 UBO above (std140: two vec4s).
	struct GlowParams
	{
		glm::vec4 GlowColor{ 1.0f, 0.95f, 0.55f, 1.0f };
		glm::vec4 Params{ 0.0f };
	};
}

namespace Dingo
{
	// ======================================================================
	// DungeonControllerScript — builds the world and runs the game loop
	// ======================================================================

	void DungeonControllerScript::OnStart()
	{
		// Reuse Renderer3D's built-in unit primitives for every spawned entity.
		Renderer3D& renderer3D = Application::Get().GetRenderer3D();
		m_Context.BoxMesh = renderer3D.GetBoxMesh();
		m_Context.SphereMesh = renderer3D.GetSphereMesh();

		LoadCharacterModels(); // hero / skeleton OBJs -> m_Context.HeroMesh / SkeletonMesh

		GetScene().SetClearColor({ 0.05f, 0.06f, 0.09f, 1.0f });
		GetScene().SetGravity(GRAVITY); // vec3 overload -> the 3D world

		// Custom per-mesh material for the treasures (created before BuildDungeon spawns
		// them). SetUniform now so the binding-1 UBO exists before the first draw.
		m_GlowShader = Shader::CreateFromSource("TreasureGlowShader", k_GlowShaderSource);
		m_GlowMaterial = Material::Create(MaterialParams()
			.SetDebugName("TreasureGlow")
			.SetShader(m_GlowShader)
			.SetCullMode(CullMode::None));
		m_GlowMaterial->SetUniform(GlowParams{});

		SetupCameraAndLight();
		BuildDungeon();

		// HUD owns its own UI camera + entities (drawn as the SceneRenderer's 2D overlay).
		GetScene().CreateEntity("Hud").AddScript<HudScript>(&m_Context);

		UpdateCamera(); // frame a correct view before the first render
		// OnStart runs before OnPhysicsStart (Scene::OnStart), so every rigid body
		// spawned above is baked when physics comes up immediately after.
	}

	void DungeonControllerScript::OnUpdate(float deltaTime)
	{
		m_Context.ElapsedTime += deltaTime;

		UpdateCamera();

		// Pulse the shared treasure-glow material (a per-material uniform at binding 1).
		if (m_GlowMaterial)
		{
			GlowParams params;
			params.Params.x = 0.35f + 0.35f * std::sin(m_Context.ElapsedTime * 3.0f);
			m_GlowMaterial->SetUniform(params);
		}

		if (m_Context.CurrentState != GameContext::State::Playing)
			return;

		if (m_Context.PlayerHealth <= 0.0f)
		{
			m_Context.PlayerHealth = 0.0f;
			m_Context.CurrentState = GameContext::State::Lost;
		}
		else if (m_Context.TotalTreasure > 0 && m_Context.Collected >= m_Context.TotalTreasure)
		{
			m_Context.CurrentState = GameContext::State::Won;
		}
	}

	void DungeonControllerScript::OnDestroy()
	{
		// The controller owns the treasures' glow material + shader (created in OnStart).
		if (m_GlowMaterial)
		{
			m_GlowMaterial->Destroy();
			delete m_GlowMaterial;
			m_GlowMaterial = nullptr;
		}
		if (m_GlowShader)
		{
			m_GlowShader->Destroy();
			m_GlowShader = nullptr;
		}

		// The controller owns the character part models (loaded in OnStart).
		for (Model*& model : m_PartModels)
		{
			if (model)
			{
				model->Destroy();
				delete model;
				model = nullptr;
			}
		}
	}

	void DungeonControllerScript::LoadCharacterModels()
	{
		// Per-part low-poly OBJs (see assets/models/generate_models.js), each a single
		// sub-mesh. If a load fails we fall back to the unit box so the game still runs.
		auto load = [&](int slot, const char* file) -> Mesh*
		{
			Model* model = Model::LoadFromFile(file);
			m_PartModels[slot] = model;
			if (model && model->GetSubMeshCount() > 0)
				return model->GetSubMeshes()[0].MeshData;
			return m_Context.BoxMesh;
		};

		m_Context.CharParts.Head = load(0, "assets/models/parts/head.obj");
		m_Context.CharParts.Torso = load(1, "assets/models/parts/torso.obj");
		m_Context.CharParts.Arm = load(2, "assets/models/parts/arm.obj");
		m_Context.CharParts.Leg = load(3, "assets/models/parts/leg.obj");
		m_Context.CharParts.Sword = load(4, "assets/models/parts/sword.obj");
	}

	glm::vec3 DungeonControllerScript::CellToWorld(int col, int row, float y) const
	{
		const float gridWidth = m_GridCols * TILE;
		const float gridDepth = m_GridRows * TILE;
		const float x = (col + 0.5f) * TILE - gridWidth * 0.5f;
		const float z = (row + 0.5f) * TILE - gridDepth * 0.5f;
		return { x, y, z };
	}

	void DungeonControllerScript::BuildDungeon()
	{
		// Fresh procedural layout every build (Seed 0 => random). Rooms + corridors,
		// guaranteed connected — see DungeonGenerator.h.
		const GeneratedDungeon dungeon = GenerateDungeon(DungeonParams{});
		m_GridCols = dungeon.Width;
		m_GridRows = dungeon.Height;
		m_Context.Seed = dungeon.Seed;
		m_Context.Collected = 0;
		m_Context.TotalTreasure = 0;
		m_Context.EnemiesSlain = 0;
		m_Context.PlayerHealth = PLAYER_MAX_HEALTH;
		m_Context.CurrentState = GameContext::State::Playing;

		SpawnFloor(m_GridCols * TILE, m_GridRows * TILE);

		for (int row = 0; row < m_GridRows; ++row)
			for (int col = 0; col < m_GridCols; ++col)
				if (dungeon.Map[row][col] == '#')
					SpawnWall(CellToWorld(col, row, WALL_HEIGHT * 0.5f), { TILE, WALL_HEIGHT, TILE }, WallColor(col, row));

		m_Context.Player = SpawnPlayer(CellToWorld(dungeon.PlayerSpawn.x, dungeon.PlayerSpawn.y, PLAYER_RADIUS + 0.05f));

		for (const glm::ivec2& cell : dungeon.EnemySpawns)
			SpawnEnemy(CellToWorld(cell.x, cell.y, PLAYER_RADIUS + 0.05f));

		float phase = 0.0f;
		for (const glm::ivec2& cell : dungeon.TreasureSpawns)
		{
			SpawnTreasure(CellToWorld(cell.x, cell.y, TREASURE_Y), phase);
			phase += 1.0f;
			m_Context.TotalTreasure++;
		}
	}

	Entity DungeonControllerScript::SpawnFloor(float worldWidth, float worldDepth)
	{
		Entity entity = GetScene().CreateEntity("Floor");
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = { 0.0f, -FLOOR_THICKNESS * 0.5f, 0.0f };
		transform.Scale = { worldWidth, FLOOR_THICKNESS, worldDepth };

		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Context.BoxMesh, COLOR_FLOOR));
		entity.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
		entity.AddComponent<BoxCollider3DComponent>();
		return entity;
	}

	Entity DungeonControllerScript::SpawnWall(const glm::vec3& position, const glm::vec3& scale, const glm::vec4& color)
	{
		Entity entity = GetScene().CreateEntity("Wall");
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = position;
		transform.Scale = scale;

		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Context.BoxMesh, color));
		entity.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
		entity.AddComponent<BoxCollider3DComponent>();
		return entity;
	}

	Entity DungeonControllerScript::SpawnPlayer(const glm::vec3& position)
	{
		Entity entity = GetScene().CreateEntity("Player");
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = position;
		transform.Scale = glm::vec3(1.0f);

		// No MeshRenderer: the rigid body is an invisible sphere; PlayerScript spawns and
		// drives a Character rig as the visible hero (its feet track this body).
		entity.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Dynamic));

		SphereCollider3DComponent collider;
		collider.Radius = 0.5f;
		collider.Friction = 0.4f;
		collider.Restitution = 0.0f;
		entity.AddComponent<SphereCollider3DComponent>(collider);

		entity.AddScript<PlayerScript>(&m_Context);
		return entity;
	}

	Entity DungeonControllerScript::SpawnEnemy(const glm::vec3& position)
	{
		Entity entity = GetScene().CreateEntity("Skeleton");
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = position;
		transform.Scale = glm::vec3(1.0f);

		// No MeshRenderer: the invisible sphere body is the collider; EnemyScript drives a
		// Character rig (the skeleton) that tracks it.
		entity.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Dynamic));

		SphereCollider3DComponent collider;
		collider.Radius = 0.5f;
		collider.Friction = 0.3f;
		collider.Restitution = 0.1f;
		entity.AddComponent<SphereCollider3DComponent>(collider);

		entity.AddScript<EnemyScript>(&m_Context);
		return entity;
	}

	Entity DungeonControllerScript::SpawnTreasure(const glm::vec3& position, float phase)
	{
		// No rigid body: the player passes through and collects it by proximity.
		Entity entity = GetScene().CreateEntity("Treasure");
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = position;
		transform.Scale = glm::vec3(0.6f);

		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Context.BoxMesh, COLOR_TREASURE)).Material = m_GlowMaterial;
		entity.AddScript<TreasureScript>(&m_Context, phase);
		return entity;
	}

	void DungeonControllerScript::SetupCameraAndLight()
	{
		m_CameraEntity = GetScene().CreateEntity("Camera");
		auto& camera = m_CameraEntity.AddComponent<CameraComponent>();
		camera.Type = CameraComponent::ProjectionType::Perspective;
		camera.FOV = 50.0f;
		camera.PerspNear = 0.1f;
		camera.PerspFar = 500.0f;
		camera.Primary = true;
		m_CameraEntity.AddComponent<Transform3DComponent>();

		GetScene().CreateEntity("Sun").AddComponent<DirectionalLightComponent>();
	}

	void DungeonControllerScript::UpdateCamera()
	{
		if (!m_CameraEntity.IsValid())
			return;

		const glm::vec3 focus = m_Context.Player.IsValid()
			? m_Context.Player.GetComponent<Transform3DComponent>().Position
			: glm::vec3(0.0f);

		const glm::vec3 eye = focus + CAMERA_OFFSET;
		const glm::vec3 target = focus + glm::vec3(0.0f, 0.5f, 0.0f);

		// The SceneRenderer derives a perspective camera's view as inverse(translate(P) *
		// mat4_cast(R)); set this entity's transform to the camera's world transform so
		// it reproduces lookAt(eye, target, up) exactly.
		auto& transform = m_CameraEntity.GetComponent<Transform3DComponent>();
		transform.Position = eye;
		transform.Rotation = glm::quat_cast(glm::inverse(glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f))));
	}

	// ======================================================================
	// PlayerScript — movement, melee, contact damage
	// ======================================================================

	void PlayerScript::OnStart()
	{
		m_Character.Create(GetScene(), m_Context->CharParts, COLOR_PLAYER, /*withSword*/ true);
	}

	void PlayerScript::OnDestroy()
	{
		m_Character.Destroy();
	}

	void PlayerScript::UpdateRig(float deltaTime, float walkSpeed01)
	{
		const glm::vec3 bodyPos = GetComponent<Transform3DComponent>().Position;
		const glm::vec3 feet = { bodyPos.x, bodyPos.y - PLAYER_RADIUS, bodyPos.z };

		// Flash toward the hurt colour while the i-frame window is active.
		m_Character.SetFlash(COLOR_PLAYER_HURT, glm::clamp(m_Invuln / PLAYER_INVULN, 0.0f, 1.0f));
		m_Character.Update(feet, m_Facing, walkSpeed01, deltaTime);
	}

	void PlayerScript::OnUpdate(float deltaTime)
	{
		if (m_AttackCooldown > 0.0f) m_AttackCooldown -= deltaTime;
		if (m_Invuln > 0.0f) m_Invuln -= deltaTime;

		UpdateAttackFx(deltaTime); // let any lingering ring finish even after the game ends

		if (!GetEntity().IsValid())
			return;

		if (m_Context->CurrentState != GameContext::State::Playing)
		{
			GetScene().SetLinearVelocity(GetEntity(), glm::vec3(0.0f)); // freeze when won/lost
			UpdateRig(deltaTime, 0.0f);
			return;
		}

		// Movement: set horizontal velocity directly (crisp control); keep the vertical
		// component so gravity holds the orb on the floor.
		glm::vec3 move(0.0f);
		if (Input::IsKeyDown(Key::W) || Input::IsKeyDown(Key::Up))    move.z -= 1.0f;
		if (Input::IsKeyDown(Key::S) || Input::IsKeyDown(Key::Down))  move.z += 1.0f;
		if (Input::IsKeyDown(Key::A) || Input::IsKeyDown(Key::Left))  move.x -= 1.0f;
		if (Input::IsKeyDown(Key::D) || Input::IsKeyDown(Key::Right)) move.x += 1.0f;

		glm::vec3 velocity = GetScene().GetLinearVelocity3D(GetEntity());
		if (glm::length(move) > 0.0f)
		{
			move = glm::normalize(move);
			velocity.x = move.x * PLAYER_SPEED;
			velocity.z = move.z * PLAYER_SPEED;
			m_Facing = std::atan2(move.x, move.z); // face the way we walk
		}
		else
		{
			velocity.x = 0.0f;
			velocity.z = 0.0f;
		}
		GetScene().SetLinearVelocity(GetEntity(), velocity);

		if (Input::IsKeyPressed(Key::Space) && m_AttackCooldown <= 0.0f)
			Attack();

		const float walkSpeed01 = glm::length(glm::vec2(velocity.x, velocity.z)) / PLAYER_SPEED;
		UpdateRig(deltaTime, walkSpeed01);

		// Enemy contact damage, gated by the invulnerability window.
		if (m_Invuln <= 0.0f)
		{
			const glm::vec3 playerPos = GetComponent<Transform3DComponent>().Position;
			for (EnemyScript* enemy : GetScene().GetScriptsOfType<EnemyScript>())
			{
				const glm::vec3 enemyPos = enemy->GetEntity().GetComponent<Transform3DComponent>().Position;
				if (DistanceXZ(playerPos, enemyPos) < CONTACT_RADIUS)
				{
					m_Context->PlayerHealth -= ENEMY_CONTACT_DAMAGE;
					m_Invuln = PLAYER_INVULN;
					m_Character.TriggerHit(); // flinch when taking contact damage
					break;
				}
			}
		}
	}

	void PlayerScript::Attack()
	{
		m_AttackCooldown = ATTACK_COOLDOWN;
		m_Character.TriggerAttack(); // visible sword swing

		const glm::vec3 playerPos = GetComponent<Transform3DComponent>().Position;

		for (EnemyScript* enemy : GetScene().GetScriptsOfType<EnemyScript>())
		{
			const glm::vec3 enemyPos = enemy->GetEntity().GetComponent<Transform3DComponent>().Position;
			if (DistanceXZ(playerPos, enemyPos) <= ATTACK_RANGE)
				enemy->Damage(ATTACK_DAMAGE);
		}

		// Spawn (or refresh) the expanding ground-slam ring. Body-less, so the physics
		// write-back never touches it; UpdateAttackFx animates and then destroys it.
		if (m_AttackFx.IsValid())
			m_AttackFx.Destroy();
		m_AttackFx = GetScene().CreateEntity("AttackFx");
		auto& transform = m_AttackFx.AddComponent<Transform3DComponent>();
		transform.Position = { playerPos.x, 0.15f, playerPos.z };
		transform.Scale = { 1.2f, 0.2f, 1.2f };
		m_AttackFx.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Context->SphereMesh, COLOR_ATTACK));
		m_AttackFxTime = ATTACK_FX_TIME;
	}

	void PlayerScript::UpdateAttackFx(float deltaTime)
	{
		if (!m_AttackFx.IsValid())
			return;

		m_AttackFxTime -= deltaTime;
		if (m_AttackFxTime <= 0.0f)
		{
			m_AttackFx.Destroy();
			m_AttackFx = {};
			return;
		}

		const float t = 1.0f - (m_AttackFxTime / ATTACK_FX_TIME); // 0 -> 1
		const float radius = glm::mix(0.8f, ATTACK_RANGE, t);

		auto& transform = m_AttackFx.GetComponent<Transform3DComponent>();
		if (GetEntity().IsValid())
		{
			const glm::vec3 p = GetComponent<Transform3DComponent>().Position;
			transform.Position = { p.x, 0.15f, p.z };
		}
		transform.Scale = { radius * 2.0f, 0.2f, radius * 2.0f };

		m_AttackFx.GetComponent<MeshRendererComponent>().Color =
			{ COLOR_ATTACK.r, COLOR_ATTACK.g, COLOR_ATTACK.b, (1.0f - t) * 0.5f };
	}

	// ======================================================================
	// EnemyScript — chase, take damage, die
	// ======================================================================

	void EnemyScript::Damage(float amount)
	{
		m_Health -= amount;
		m_HitFlash = ENEMY_HIT_FLASH;
		m_Character.TriggerHit(); // recoil/stagger on being struck
		if (m_Health <= 0.0f)
		{
			m_Context->EnemiesSlain++;
			GetEntity().Destroy(); // deferred to the end of the update pass
		}
	}

	void EnemyScript::OnStart()
	{
		m_Character.Create(GetScene(), m_Context->CharParts, COLOR_ENEMY, /*withSword*/ false);
	}

	void EnemyScript::OnDestroy()
	{
		m_Character.Destroy();
	}

	void EnemyScript::OnUpdate(float deltaTime)
	{
		if (m_HitFlash > 0.0f)
			m_HitFlash -= deltaTime;

		if (m_Health <= 0.0f)
			return; // killed this frame; awaiting its deferred destroy

		const glm::vec3 enemyPos = GetComponent<Transform3DComponent>().Position;

		auto driveRig = [&](float walkSpeed01)
		{
			const glm::vec3 feet = { enemyPos.x, enemyPos.y - PLAYER_RADIUS, enemyPos.z };
			m_Character.SetFlash(COLOR_ENEMY_FLASH, glm::clamp(m_HitFlash / ENEMY_HIT_FLASH, 0.0f, 1.0f));
			m_Character.Update(feet, m_Facing, walkSpeed01, deltaTime);
		};

		if (m_Context->CurrentState != GameContext::State::Playing || !m_Context->Player.IsValid())
		{
			GetScene().SetLinearVelocity(GetEntity(), glm::vec3(0.0f));
			driveRig(0.0f);
			return;
		}

		const glm::vec3 playerPos = m_Context->Player.GetComponent<Transform3DComponent>().Position;
		glm::vec3 toPlayer = playerPos - enemyPos;
		toPlayer.y = 0.0f;
		const float dist = glm::length(toPlayer);

		glm::vec3 velocity = GetScene().GetLinearVelocity3D(GetEntity());
		if (dist > 0.001f && dist < ENEMY_AGGRO)
		{
			toPlayer /= dist;
			velocity.x = toPlayer.x * ENEMY_SPEED;
			velocity.z = toPlayer.z * ENEMY_SPEED;
			m_Facing = std::atan2(toPlayer.x, toPlayer.z); // turn to face the player
		}
		else
		{
			velocity.x = 0.0f;
			velocity.z = 0.0f;
		}
		GetScene().SetLinearVelocity(GetEntity(), velocity);

		driveRig(glm::length(glm::vec2(velocity.x, velocity.z)) / ENEMY_SPEED);
	}

	// ======================================================================
	// TreasureScript — spin, bob, collect
	// ======================================================================

	void TreasureScript::OnUpdate(float)
	{
		if (m_Context->CurrentState != GameContext::State::Playing)
			return;

		auto& transform = GetComponent<Transform3DComponent>();
		transform.SetRotationEuler({ 0.0f, m_Context->ElapsedTime * TREASURE_SPIN_DEG, 0.0f });
		transform.Position.y = TREASURE_Y + std::sin(m_Context->ElapsedTime * 2.5f + m_Phase) * 0.12f;

		if (m_Context->Player.IsValid())
		{
			const glm::vec3 playerPos = m_Context->Player.GetComponent<Transform3DComponent>().Position;
			if (DistanceXZ(playerPos, transform.Position) < COLLECT_RADIUS)
			{
				m_Context->Collected++;
				GetEntity().Destroy();
			}
		}
	}

	// ======================================================================
	// HudScript — 2D overlay (UI camera + health bar / counters / banner)
	// ======================================================================

	void HudScript::OnStart()
	{
		m_Font = Font::Create("assets/fonts/arialbd.ttf");

		Scene& scene = GetScene();

		// The orthographic UI camera drives the SceneRenderer's 2D overlay pass (drawn
		// on top of the 3D world). It sits at the origin, so HUD entities are positioned
		// in screen-space world units in OnUpdate.
		auto& camera = scene.CreateEntity("UICamera").AddComponent<CameraComponent>();
		camera.Type = CameraComponent::ProjectionType::Orthographic;
		camera.OrthographicSize = m_OrthoSize;
		camera.Primary = true;

		m_BarBackground = scene.CreateEntity("HudBarBg");
		m_BarBackground.AddComponent<SpriteRendererComponent>().Color = COLOR_BAR_BG;

		m_BarFill = scene.CreateEntity("HudBarFill");
		m_BarFill.AddComponent<SpriteRendererComponent>().Color = COLOR_HP;

		auto makeText = [&](const char* name, float size, const glm::vec4& color, bool centered) -> Entity
		{
			Entity entity = scene.CreateEntity(name);
			auto& text = entity.AddComponent<TextComponent>();
			text.Font = m_Font;
			text.Size = size;
			text.Color = color;
			text.Centered = centered;
			return entity;
		};

		m_HealthText   = makeText("HudHealth", 0.42f, COLOR_TEXT_DIM, false);
		m_TreasureText = makeText("HudTreasure", 0.6f, COLOR_TREASURE, false);
		m_ControlsText = makeText("HudControls", 0.4f, { 0.80f, 0.82f, 0.92f, 1.0f }, true);
		m_SeedText     = makeText("HudSeed", 0.32f, COLOR_TEXT_DIM, false);
		m_Banner       = makeText("HudBanner", 1.0f, COLOR_TEXT, true);

		m_ControlsText.GetComponent<TextComponent>().Text =
			"WASD move      SPACE attack      collect all treasure      R new dungeon";
	}

	void HudScript::OnUpdate(float)
	{
		const glm::vec2 viewport = Application::Get().GetRenderer2D().GetViewportSize();
		const float aspect = (viewport.y > 0.0f) ? viewport.x / viewport.y : 1.0f;
		const float halfH = m_OrthoSize * 0.5f;
		const float halfW = halfH * aspect;
		const float pad = 0.45f;

		// Health bar (top-left): background slab + a fill quad proportional to health.
		const float barW = 4.5f;
		const float barH = 0.45f;
		const float barLeft = -halfW + pad;
		const float barCenterY = halfH - 0.55f;
		const float frac = glm::clamp(m_Context->PlayerHealth / PLAYER_MAX_HEALTH, 0.0f, 1.0f);

		m_BarBackground.GetComponent<TransformComponent>().Position = { barLeft + barW * 0.5f, barCenterY, 0.0f };
		m_BarBackground.GetComponent<TransformComponent>().Size = { barW, barH };

		// The fill sits a hair in front of the background (higher z) so the painter's
		// sort always draws it on top — equal-z sprites are not ordered by creation.
		auto& fillTransform = m_BarFill.GetComponent<TransformComponent>();
		fillTransform.Position = { barLeft + barW * frac * 0.5f, barCenterY, 0.1f };
		fillTransform.Size = { barW * frac, barH * 0.7f };
		m_BarFill.GetComponent<SpriteRendererComponent>().Color = (frac < 0.33f) ? COLOR_HP_LOW : COLOR_HP;

		m_HealthText.GetComponent<TransformComponent>().Position = { barLeft, barCenterY - 0.95f, 0.0f };
		m_HealthText.GetComponent<TextComponent>().Text = std::format("HP {}", static_cast<int>(std::ceil(m_Context->PlayerHealth)));

		// Treasure counter (top-right): right-aligned by measuring the string width.
		{
			const std::string text = std::format("TREASURE  {}/{}", m_Context->Collected, m_Context->TotalTreasure);
			const float width = m_Font->GetStringWidth(text, 0.6f);
			m_TreasureText.GetComponent<TransformComponent>().Position = { halfW - pad - width, halfH - 0.95f, 0.0f };
			m_TreasureText.GetComponent<TextComponent>().Text = text;
		}

		// Controls (centered) + procedural seed (bottom-left, dim).
		m_ControlsText.GetComponent<TransformComponent>().Position = { 0.0f, -halfH + 0.45f, 0.0f };
		m_SeedText.GetComponent<TransformComponent>().Position = { barLeft, -halfH + 0.45f, 0.0f };
		m_SeedText.GetComponent<TextComponent>().Text = std::format("SEED {}", m_Context->Seed);

		// Win / loss banner (centered).
		auto& banner = m_Banner.GetComponent<TextComponent>();
		m_Banner.GetComponent<TransformComponent>().Position = { 0.0f, 0.6f, 0.0f };
		switch (m_Context->CurrentState)
		{
			case GameContext::State::Won:
				banner.Text = "DUNGEON CLEARED!";
				banner.Color = { 1.0f, 0.85f, 0.30f, 1.0f };
				break;
			case GameContext::State::Lost:
				banner.Text = "YOU DIED";
				banner.Color = { 0.90f, 0.30f, 0.25f, 1.0f };
				break;
			default:
				banner.Text.clear();
				break;
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
}
