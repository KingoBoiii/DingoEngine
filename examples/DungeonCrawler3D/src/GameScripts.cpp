#include "GameScripts.h"
#include "DungeonGenerator.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <format>

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

		GetScene().SetClearColor({ 0.05f, 0.06f, 0.09f, 1.0f });
		GetScene().SetGravity(GRAVITY); // vec3 overload -> the 3D world

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
		transform.Scale = glm::vec3(1.0f); // unit sphere -> radius 0.5

		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Context.SphereMesh, COLOR_PLAYER));
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

		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Context.SphereMesh, COLOR_ENEMY));
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

		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Context.BoxMesh, COLOR_TREASURE));
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

	void PlayerScript::OnUpdate(float deltaTime)
	{
		if (m_AttackCooldown > 0.0f) m_AttackCooldown -= deltaTime;
		if (m_Invuln > 0.0f) m_Invuln -= deltaTime;

		UpdateAttackFx(deltaTime); // let any lingering ring finish even after the game ends

		if (!GetEntity().IsValid())
			return;

		// Hurt tint regardless of state.
		GetComponent<MeshRendererComponent>().Color = (m_Invuln > 0.0f) ? COLOR_PLAYER_HURT : COLOR_PLAYER;

		if (m_Context->CurrentState != GameContext::State::Playing)
		{
			GetScene().SetLinearVelocity(GetEntity(), glm::vec3(0.0f)); // freeze when won/lost
			return;
		}

		// Movement: set horizontal velocity directly (crisp control); keep the vertical
		// component so gravity holds the orb on the floor.
		glm::vec3 move(0.0f);
		if (Input::IsKeyPressed(Key::W) || Input::IsKeyPressed(Key::Up))    move.z -= 1.0f;
		if (Input::IsKeyPressed(Key::S) || Input::IsKeyPressed(Key::Down))  move.z += 1.0f;
		if (Input::IsKeyPressed(Key::A) || Input::IsKeyPressed(Key::Left))  move.x -= 1.0f;
		if (Input::IsKeyPressed(Key::D) || Input::IsKeyPressed(Key::Right)) move.x += 1.0f;

		glm::vec3 velocity = GetScene().GetLinearVelocity3D(GetEntity());
		if (glm::length(move) > 0.0f)
		{
			move = glm::normalize(move);
			velocity.x = move.x * PLAYER_SPEED;
			velocity.z = move.z * PLAYER_SPEED;
		}
		else
		{
			velocity.x = 0.0f;
			velocity.z = 0.0f;
		}
		GetScene().SetLinearVelocity(GetEntity(), velocity);

		if (Input::IsKeyDown(Key::Space) && m_AttackCooldown <= 0.0f)
			Attack();

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
					break;
				}
			}
		}
	}

	void PlayerScript::Attack()
	{
		m_AttackCooldown = ATTACK_COOLDOWN;

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
		if (m_Health <= 0.0f)
		{
			m_Context->EnemiesSlain++;
			GetEntity().Destroy(); // deferred to the end of the update pass
		}
	}

	void EnemyScript::OnUpdate(float deltaTime)
	{
		if (m_HitFlash > 0.0f)
			m_HitFlash -= deltaTime;

		if (m_Health <= 0.0f)
			return; // killed this frame; awaiting its deferred destroy

		GetComponent<MeshRendererComponent>().Color = (m_HitFlash > 0.0f) ? COLOR_ENEMY_FLASH : COLOR_ENEMY;

		if (m_Context->CurrentState != GameContext::State::Playing || !m_Context->Player.IsValid())
		{
			GetScene().SetLinearVelocity(GetEntity(), glm::vec3(0.0f));
			return;
		}

		const glm::vec3 playerPos = m_Context->Player.GetComponent<Transform3DComponent>().Position;
		const glm::vec3 enemyPos = GetComponent<Transform3DComponent>().Position;
		glm::vec3 toPlayer = playerPos - enemyPos;
		toPlayer.y = 0.0f;
		const float dist = glm::length(toPlayer);

		glm::vec3 velocity = GetScene().GetLinearVelocity3D(GetEntity());
		if (dist > 0.001f && dist < ENEMY_AGGRO)
		{
			toPlayer /= dist;
			velocity.x = toPlayer.x * ENEMY_SPEED;
			velocity.z = toPlayer.z * ENEMY_SPEED;
		}
		else
		{
			velocity.x = 0.0f;
			velocity.z = 0.0f;
		}
		GetScene().SetLinearVelocity(GetEntity(), velocity);
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

		auto& fillTransform = m_BarFill.GetComponent<TransformComponent>();
		fillTransform.Position = { barLeft + barW * frac * 0.5f, barCenterY, 0.0f };
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
