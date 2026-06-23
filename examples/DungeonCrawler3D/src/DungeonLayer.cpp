#include "DungeonLayer.h"
#include "DungeonGenerator.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <format>

namespace Dingo
{

	// --- Tuning ---------------------------------------------------------------

	static constexpr float TILE = 2.0f;             // world size of one grid cell
	static constexpr float WALL_HEIGHT = 2.5f;
	static constexpr float FLOOR_THICKNESS = 1.0f;
	static constexpr float PLAYER_RADIUS = 0.5f;    // the unit sphere mesh has r = 0.5

	static constexpr float PLAYER_SPEED = 6.5f;
	static constexpr float PLAYER_MAX_HEALTH = 100.0f;
	static constexpr float PLAYER_INVULN = 0.7f;    // i-frames after taking a hit

	static constexpr float ATTACK_RANGE = 2.6f;     // world-space reach of the swing
	static constexpr float ATTACK_DAMAGE = 34.0f;   // enemy has 60 HP -> 2 hits
	static constexpr float ATTACK_COOLDOWN = 0.4f;
	static constexpr float ATTACK_FX_TIME = 0.22f;  // ring grow/fade duration

	static constexpr float ENEMY_SPEED = 3.3f;
	static constexpr float ENEMY_AGGRO = 18.0f;     // only chases within this range
	static constexpr float ENEMY_MAX_HEALTH = 60.0f;
	static constexpr float ENEMY_CONTACT_DAMAGE = 16.0f;
	static constexpr float ENEMY_HIT_FLASH = 0.12f;
	static constexpr float CONTACT_RADIUS = 1.1f;   // enemy touches player

	static constexpr float COLLECT_RADIUS = 1.3f;
	static constexpr float TREASURE_Y = 0.9f;
	static constexpr float TREASURE_SPIN_DEG = 90.0f;

	static const glm::vec3 GRAVITY = { 0.0f, -18.0f, 0.0f };
	static const glm::vec3 CAMERA_OFFSET = { 0.0f, 16.0f, 12.0f };

	static const glm::vec4 COLOR_FLOOR       = { 0.15f, 0.16f, 0.20f, 1.0f };
	static const glm::vec4 COLOR_WALL_A      = { 0.42f, 0.44f, 0.52f, 1.0f };
	static const glm::vec4 COLOR_WALL_B      = { 0.34f, 0.36f, 0.45f, 1.0f };
	static const glm::vec4 COLOR_PLAYER      = { 0.30f, 0.85f, 0.95f, 1.0f };
	static const glm::vec4 COLOR_PLAYER_HURT = { 1.00f, 0.55f, 0.55f, 1.0f };
	static const glm::vec4 COLOR_TREASURE    = { 1.00f, 0.80f, 0.20f, 1.0f };
	static const glm::vec4 COLOR_ENEMY       = { 0.90f, 0.28f, 0.28f, 1.0f };
	static const glm::vec4 COLOR_ENEMY_FLASH = { 1.00f, 0.92f, 0.92f, 1.0f };
	static const glm::vec4 COLOR_ATTACK      = { 1.00f, 0.92f, 0.45f, 0.45f }; // translucent
	static const glm::vec4 COLOR_HP          = { 0.32f, 0.82f, 0.45f, 1.0f };
	static const glm::vec4 COLOR_HP_LOW      = { 0.92f, 0.35f, 0.30f, 1.0f };
	static const glm::vec4 COLOR_BAR_BG      = { 0.06f, 0.06f, 0.08f, 1.0f };
	static const glm::vec4 COLOR_TEXT        = { 0.92f, 0.94f, 0.98f, 1.0f };
	static const glm::vec4 COLOR_TEXT_DIM    = { 0.65f, 0.67f, 0.74f, 1.0f };

	namespace
	{
		glm::vec4 WallColor(int col, int row)
		{
			return ((col + row) & 1) ? COLOR_WALL_A : COLOR_WALL_B;
		}

		float DistanceXZ(const glm::vec3& a, const glm::vec3& b)
		{
			const float dx = a.x - b.x;
			const float dz = a.z - b.z;
			return std::sqrt(dx * dx + dz * dz);
		}
	}

	// --- Lifecycle ------------------------------------------------------------

	void DungeonLayer::OnAttach()
	{
		// Reuse Renderer3D's built-in unit primitives — no per-layer meshes needed.
		Renderer3D& renderer3D = Application::Get().GetRenderer3D();
		m_BoxMesh = renderer3D.GetBoxMesh();
		m_SphereMesh = renderer3D.GetSphereMesh();

		const Window& window = Application::Get().GetWindow();
		m_AspectRatio = static_cast<float>(window.GetWidth()) / static_cast<float>(window.GetHeight());

		m_Camera = PerspectiveCamera(50.0f, m_AspectRatio, 0.1f, 500.0f);

		m_Font = Font::Create("assets/fonts/arialbd.ttf");
		UpdateOrthoProjection();

		m_Scene = std::make_unique<Scene>("Dungeon Crawler 3D");
		m_Scene->SetClearColor({ 0.05f, 0.06f, 0.09f, 1.0f });
		m_Scene->SetGravity(GRAVITY); // vec3 overload -> the 3D world

		BuildDungeon();
		m_Scene->OnPhysicsStart();
		UpdateCamera();
	}

	void DungeonLayer::OnDetach()
	{
		// Meshes are owned by Renderer3D — do not delete them here.
		m_Scene.reset();

		if (m_Font)
		{
			m_Font->Destroy();
			m_Font = nullptr;
		}
	}

	void DungeonLayer::OnUpdate(float deltaTime)
	{
		if (Input::IsKeyDown(Key::Escape))
			Application::Get().Close();

		if (Input::IsKeyDown(Key::R))
			Restart(); // builds a fresh procedural dungeon

		m_ElapsedTime += deltaTime;
		if (m_Invuln > 0.0f) m_Invuln -= deltaTime;
		if (m_AttackCooldown > 0.0f) m_AttackCooldown -= deltaTime;

		if (m_State == State::Playing)
		{
			UpdatePlayer(deltaTime);      // movement + attack input
			UpdateEnemies(deltaTime);     // aggro steering + hit-flash

			m_Scene->OnUpdate(deltaTime); // steps physics + writes transforms back

			UpdateTreasures(deltaTime);   // spin / bob / collect (post-step positions)
			UpdateAttackFx(deltaTime);    // grow / fade the swing ring

			// Enemy contact damage, gated by the invulnerability window.
			if (m_Invuln <= 0.0f && m_Player.IsValid())
			{
				const glm::vec3 playerPos = m_Player.GetComponent<Transform3DComponent>().Position;
				for (Enemy& enemy : m_Enemies)
				{
					if (!enemy.Handle.IsValid())
						continue;

					const glm::vec3 enemyPos = enemy.Handle.GetComponent<Transform3DComponent>().Position;
					if (DistanceXZ(playerPos, enemyPos) < CONTACT_RADIUS)
					{
						m_PlayerHealth -= ENEMY_CONTACT_DAMAGE;
						m_Invuln = PLAYER_INVULN;
						break;
					}
				}
			}

			if (m_PlayerHealth <= 0.0f)
			{
				m_PlayerHealth = 0.0f;
				m_State = State::Lost;
			}
			else if (m_TotalTreasure > 0 && m_Collected >= m_TotalTreasure)
			{
				m_State = State::Won;
			}
		}
		else
		{
			UpdateAttackFx(deltaTime); // let any lingering ring finish fading
		}

		UpdateCamera();

		m_Scene->OnRender3D(Application::Get().GetRenderer3D(), m_Camera);
		RenderHud();
	}

	void DungeonLayer::OnEvent(Event& event)
	{
		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& e)
		{
			if (e.GetWidth() > 0 && e.GetHeight() > 0)
			{
				m_AspectRatio = static_cast<float>(e.GetWidth()) / static_cast<float>(e.GetHeight());
				m_Camera.SetAspectRatio(m_AspectRatio);
				UpdateOrthoProjection();
			}
			return false;
		});
	}

	// --- Scene construction ---------------------------------------------------

	glm::vec3 DungeonLayer::CellToWorld(int col, int row, float y) const
	{
		const float gridWidth = m_GridCols * TILE;
		const float gridDepth = m_GridRows * TILE;
		const float x = (col + 0.5f) * TILE - gridWidth * 0.5f;
		const float z = (row + 0.5f) * TILE - gridDepth * 0.5f;
		return { x, y, z };
	}

	void DungeonLayer::BuildDungeon()
	{
		m_Collected = 0;
		m_TotalTreasure = 0;
		m_EnemiesSlain = 0;

		// Fresh procedural layout every build (Seed 0 => random). Rooms + corridors,
		// guaranteed connected — see DungeonGenerator.h.
		const GeneratedDungeon dungeon = GenerateDungeon(DungeonParams{});
		m_GridCols = dungeon.Width;
		m_GridRows = dungeon.Height;
		m_Seed = dungeon.Seed;

		SpawnFloor(m_GridCols * TILE, m_GridRows * TILE);

		// One box wall per solid ('#') cell.
		for (int row = 0; row < m_GridRows; ++row)
			for (int col = 0; col < m_GridCols; ++col)
				if (dungeon.Map[row][col] == '#')
					SpawnWall(CellToWorld(col, row, WALL_HEIGHT * 0.5f), { TILE, WALL_HEIGHT, TILE }, WallColor(col, row));

		m_PlayerSpawn = CellToWorld(dungeon.PlayerSpawn.x, dungeon.PlayerSpawn.y, PLAYER_RADIUS + 0.05f);
		m_Player = SpawnPlayer(m_PlayerSpawn);

		for (const glm::ivec2& cell : dungeon.EnemySpawns)
			m_Enemies.push_back({ SpawnEnemy(CellToWorld(cell.x, cell.y, PLAYER_RADIUS + 0.05f)), ENEMY_MAX_HEALTH, 0.0f });

		for (const glm::ivec2& cell : dungeon.TreasureSpawns)
		{
			m_Treasures.push_back(SpawnTreasure(CellToWorld(cell.x, cell.y, TREASURE_Y)));
			m_TotalTreasure++;
		}

		m_PlayerHealth = PLAYER_MAX_HEALTH;
	}

	void DungeonLayer::Restart()
	{
		m_Scene->Clear(); // stops physics and destroys every entity (incl. the attack FX)
		m_Treasures.clear();
		m_Enemies.clear();
		m_Player = {};
		m_AttackFx = {};
		m_AttackFxTime = 0.0f;
		m_AttackCooldown = 0.0f;
		m_Invuln = 0.0f;
		m_State = State::Playing;

		BuildDungeon();
		m_Scene->OnPhysicsStart();
		UpdateCamera();
	}

	Entity DungeonLayer::SpawnFloor(float worldWidth, float worldDepth)
	{
		Entity entity = m_Scene->CreateEntity("Floor");
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = { 0.0f, -FLOOR_THICKNESS * 0.5f, 0.0f };
		transform.Scale = { worldWidth, FLOOR_THICKNESS, worldDepth };

		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_BoxMesh, COLOR_FLOOR));
		entity.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
		entity.AddComponent<BoxCollider3DComponent>();
		return entity;
	}

	Entity DungeonLayer::SpawnWall(const glm::vec3& position, const glm::vec3& scale, const glm::vec4& color)
	{
		Entity entity = m_Scene->CreateEntity("Wall");
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = position;
		transform.Scale = scale;

		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_BoxMesh, color));
		entity.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
		entity.AddComponent<BoxCollider3DComponent>();
		return entity;
	}

	Entity DungeonLayer::SpawnPlayer(const glm::vec3& position)
	{
		Entity entity = m_Scene->CreateEntity("Player");
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = position;
		transform.Scale = glm::vec3(1.0f); // unit sphere -> radius 0.5

		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_SphereMesh, COLOR_PLAYER));
		entity.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Dynamic));

		SphereCollider3DComponent collider;
		collider.Radius = 0.5f;
		collider.Friction = 0.4f;
		collider.Restitution = 0.0f;
		entity.AddComponent<SphereCollider3DComponent>(collider);

		m_Scene->CreateRigidBody(entity); // no-op during the build; OnPhysicsStart makes the body
		return entity;
	}

	Entity DungeonLayer::SpawnTreasure(const glm::vec3& position)
	{
		// No rigid body: the player passes through and collects it by proximity.
		Entity entity = m_Scene->CreateEntity("Treasure");
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = position;
		transform.Scale = glm::vec3(0.6f);

		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_BoxMesh, COLOR_TREASURE));
		return entity;
	}

	Entity DungeonLayer::SpawnEnemy(const glm::vec3& position)
	{
		Entity entity = m_Scene->CreateEntity("Skeleton");
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = position;
		transform.Scale = glm::vec3(1.0f);

		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_SphereMesh, COLOR_ENEMY));
		entity.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Dynamic));

		SphereCollider3DComponent collider;
		collider.Radius = 0.5f;
		collider.Friction = 0.3f;
		collider.Restitution = 0.1f;
		entity.AddComponent<SphereCollider3DComponent>(collider);

		m_Scene->CreateRigidBody(entity);
		return entity;
	}

	// --- Gameplay -------------------------------------------------------------

	void DungeonLayer::UpdatePlayer(float)
	{
		if (!m_Player.IsValid())
			return;

		glm::vec3 move(0.0f);
		if (Input::IsKeyPressed(Key::W) || Input::IsKeyPressed(Key::Up))    move.z -= 1.0f;
		if (Input::IsKeyPressed(Key::S) || Input::IsKeyPressed(Key::Down))  move.z += 1.0f;
		if (Input::IsKeyPressed(Key::A) || Input::IsKeyPressed(Key::Left))  move.x -= 1.0f;
		if (Input::IsKeyPressed(Key::D) || Input::IsKeyPressed(Key::Right)) move.x += 1.0f;

		// Set horizontal velocity directly (crisp control); keep the vertical component
		// so gravity holds the orb on the floor.
		glm::vec3 velocity = m_Scene->GetLinearVelocity3D(m_Player);
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
		m_Scene->SetLinearVelocity(m_Player, velocity);

		// Radial melee swing.
		if (Input::IsKeyDown(Key::Space) && m_AttackCooldown <= 0.0f)
			Attack();

		// Hurt tint while invulnerable.
		m_Player.GetComponent<MeshRendererComponent>().Color = (m_Invuln > 0.0f) ? COLOR_PLAYER_HURT : COLOR_PLAYER;
	}

	void DungeonLayer::Attack()
	{
		m_AttackCooldown = ATTACK_COOLDOWN;

		const glm::vec3 playerPos = m_Player.GetComponent<Transform3DComponent>().Position;

		// Hit every enemy within reach; drop the ones that die.
		for (std::size_t i = 0; i < m_Enemies.size(); )
		{
			Enemy& enemy = m_Enemies[i];
			if (!enemy.Handle.IsValid())
			{
				m_Enemies.erase(m_Enemies.begin() + i);
				continue;
			}

			const glm::vec3 enemyPos = enemy.Handle.GetComponent<Transform3DComponent>().Position;
			if (DistanceXZ(playerPos, enemyPos) <= ATTACK_RANGE)
			{
				enemy.Health -= ATTACK_DAMAGE;
				enemy.HitFlash = ENEMY_HIT_FLASH;
				if (enemy.Health <= 0.0f)
				{
					enemy.Handle.Destroy();
					m_Enemies.erase(m_Enemies.begin() + i);
					m_EnemiesSlain++;
					continue;
				}
			}
			++i;
		}

		// Spawn (or refresh) the expanding ground-slam ring. Body-less, so the physics
		// write-back never touches it; UpdateAttackFx animates and then destroys it.
		if (m_AttackFx.IsValid())
			m_AttackFx.Destroy();
		m_AttackFx = m_Scene->CreateEntity("AttackFx");
		auto& transform = m_AttackFx.AddComponent<Transform3DComponent>();
		transform.Position = { playerPos.x, 0.15f, playerPos.z };
		transform.Scale = { 1.2f, 0.2f, 1.2f };
		m_AttackFx.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_SphereMesh, COLOR_ATTACK));
		m_AttackFxTime = ATTACK_FX_TIME;
	}

	void DungeonLayer::UpdateAttackFx(float deltaTime)
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
		if (m_Player.IsValid())
		{
			const glm::vec3 p = m_Player.GetComponent<Transform3DComponent>().Position;
			transform.Position = { p.x, 0.15f, p.z };
		}
		transform.Scale = { radius * 2.0f, 0.2f, radius * 2.0f };

		m_AttackFx.GetComponent<MeshRendererComponent>().Color =
			{ COLOR_ATTACK.r, COLOR_ATTACK.g, COLOR_ATTACK.b, (1.0f - t) * 0.5f };
	}

	void DungeonLayer::UpdateEnemies(float deltaTime)
	{
		if (!m_Player.IsValid())
			return;

		const glm::vec3 playerPos = m_Player.GetComponent<Transform3DComponent>().Position;
		for (Enemy& enemy : m_Enemies)
		{
			if (!enemy.Handle.IsValid())
				continue;

			if (enemy.HitFlash > 0.0f)
				enemy.HitFlash -= deltaTime;

			const glm::vec3 enemyPos = enemy.Handle.GetComponent<Transform3DComponent>().Position;
			glm::vec3 toPlayer = playerPos - enemyPos;
			toPlayer.y = 0.0f;
			const float dist = glm::length(toPlayer);

			glm::vec3 velocity = m_Scene->GetLinearVelocity3D(enemy.Handle);
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
			m_Scene->SetLinearVelocity(enemy.Handle, velocity);

			enemy.Handle.GetComponent<MeshRendererComponent>().Color =
				(enemy.HitFlash > 0.0f) ? COLOR_ENEMY_FLASH : COLOR_ENEMY;
		}
	}

	void DungeonLayer::UpdateTreasures(float)
	{
		const bool hasPlayer = m_Player.IsValid();
		const glm::vec3 playerPos = hasPlayer ? m_Player.GetComponent<Transform3DComponent>().Position : glm::vec3(0.0f);

		for (std::size_t i = 0; i < m_Treasures.size(); )
		{
			Entity treasure = m_Treasures[i];
			if (!treasure.IsValid())
			{
				m_Treasures.erase(m_Treasures.begin() + i);
				continue;
			}

			auto& transform = treasure.GetComponent<Transform3DComponent>();
			transform.SetRotationEuler({ 0.0f, m_ElapsedTime * TREASURE_SPIN_DEG, 0.0f });
			transform.Position.y = TREASURE_Y + std::sin(m_ElapsedTime * 2.5f + static_cast<float>(i)) * 0.12f;

			if (hasPlayer && DistanceXZ(playerPos, transform.Position) < COLLECT_RADIUS)
			{
				treasure.Destroy();
				m_Treasures.erase(m_Treasures.begin() + i);
				m_Collected++;
				continue;
			}

			++i;
		}
	}

	void DungeonLayer::UpdateCamera()
	{
		const glm::vec3 focus = m_Player.IsValid()
			? m_Player.GetComponent<Transform3DComponent>().Position
			: glm::vec3(0.0f);

		m_Camera.SetPosition(focus + CAMERA_OFFSET);
		m_Camera.SetTarget(focus + glm::vec3(0.0f, 0.5f, 0.0f));
	}

	// --- HUD ------------------------------------------------------------------

	void DungeonLayer::UpdateOrthoProjection()
	{
		const float halfH = m_OrthoSize * 0.5f;
		const float halfW = halfH * m_AspectRatio;
		m_OrthoProjection = glm::ortho(-halfW, halfW, -halfH, halfH, -1.0f, 1.0f);
	}

	void DungeonLayer::DrawCenteredText(Renderer2D& renderer, const std::string& text, float size, float y, const glm::vec4& color)
	{
		const float width = m_Font->GetStringWidth(text, size);
		renderer.DrawText(text, m_Font, glm::vec2(-width * 0.5f, y), size, { color });
	}

	void DungeonLayer::RenderHud()
	{
		if (!m_Font)
			return;

		const float halfH = m_OrthoSize * 0.5f;
		const float halfW = halfH * m_AspectRatio;
		const float pad = 0.45f;

		Renderer2D& renderer = Application::Get().GetRenderer2D();
		renderer.BeginScene(m_OrthoProjection);

		// Health bar (top-left): background slab + a fill quad proportional to health.
		const float barW = 4.5f;
		const float barH = 0.45f;
		const float barLeft = -halfW + pad;
		const float barCenterY = halfH - 0.55f;
		const float frac = glm::clamp(m_PlayerHealth / PLAYER_MAX_HEALTH, 0.0f, 1.0f);

		renderer.DrawQuad(glm::vec2(barLeft + barW * 0.5f, barCenterY), glm::vec2(barW, barH), COLOR_BAR_BG);
		if (frac > 0.0f)
		{
			const glm::vec4 fill = (frac < 0.33f) ? COLOR_HP_LOW : COLOR_HP;
			renderer.DrawQuad(glm::vec2(barLeft + barW * frac * 0.5f, barCenterY), glm::vec2(barW * frac, barH * 0.7f), fill);
		}
		renderer.DrawText(std::format("HP {}", static_cast<int>(std::ceil(m_PlayerHealth))), m_Font,
			glm::vec2(barLeft, barCenterY - 0.95f), 0.42f, { COLOR_TEXT_DIM });

		// Treasure counter (top-right).
		{
			const std::string text = std::format("TREASURE  {}/{}", m_Collected, m_TotalTreasure);
			const float width = m_Font->GetStringWidth(text, 0.6f);
			renderer.DrawText(text, m_Font, glm::vec2(halfW - pad - width, halfH - 0.95f), 0.6f, { COLOR_TREASURE });
		}

		// Controls (bottom) + procedural seed (bottom-left, dim).
		DrawCenteredText(renderer, "WASD move      SPACE attack      collect all treasure      R new dungeon",
			0.4f, -halfH + 0.45f, { 0.80f, 0.82f, 0.92f, 1.0f });
		renderer.DrawText(std::format("SEED {}", m_Seed), m_Font, glm::vec2(barLeft, -halfH + 0.45f), 0.32f, { COLOR_TEXT_DIM });

		if (m_State == State::Won)
		{
			DrawCenteredText(renderer, "DUNGEON CLEARED!", 1.2f, 0.6f, { 0.45f, 0.95f, 0.55f, 1.0f });
			DrawCenteredText(renderer, std::format("{} slain", m_EnemiesSlain), 0.5f, -0.4f, { COLOR_TEXT });
			DrawCenteredText(renderer, "Press R for a new dungeon", 0.45f, -1.2f, { 0.85f, 0.87f, 0.95f, 1.0f });
		}
		else if (m_State == State::Lost)
		{
			DrawCenteredText(renderer, "YOU DIED", 1.2f, 0.5f, { 0.95f, 0.35f, 0.30f, 1.0f });
			DrawCenteredText(renderer, "Press R for a new dungeon", 0.45f, -0.6f, { 0.85f, 0.87f, 0.95f, 1.0f });
		}

		renderer.EndScene();
	}

}
