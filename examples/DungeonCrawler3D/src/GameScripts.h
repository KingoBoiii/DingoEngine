#pragma once
#include <DingoEngine.h>

#include "GameContext.h"
#include "Character.h"

#include <glm/glm.hpp>

// All of the 3D dungeon crawler's logic lives in these ScriptableEntity behaviours.
// The layer only creates the scene and attaches the DungeonControllerScript; the
// controller's OnStart builds the whole world (dungeon, player, enemies, treasure,
// camera, light, HUD) and the per-entity scripts drive everything from there.
namespace Dingo
{
	// Root "game manager": builds the dungeon and owns the shared GameContext. Its
	// OnStart runs before physics (so the bodies it spawns get baked), and its OnUpdate
	// advances the clock, follows the camera, and resolves win / loss.
	class DungeonControllerScript : public ScriptableEntity
	{
	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		void LoadCharacterModels();
		void BuildDungeon();
		glm::vec3 CellToWorld(int col, int row, float y) const;
		Entity SpawnFloor(float worldWidth, float worldDepth);
		Entity SpawnWall(const glm::vec3& position, const glm::vec3& scale, const glm::vec4& color);
		Entity SpawnPlayer(const glm::vec3& position);
		Entity SpawnEnemy(const glm::vec3& position);
		Entity SpawnTreasure(const glm::vec3& position, float phase);
		void SetupCameraAndLight();
		void UpdateCamera();

	private:
		GameContext m_Context;
		Entity m_CameraEntity;
		int m_GridCols = 0;
		int m_GridRows = 0;

		// A custom unlit/glow material the treasures render with — shows off per-mesh
		// materials (a custom shader + a per-material uniform, pulsed each frame).
		Shader* m_GlowShader = nullptr;
		Material* m_GlowMaterial = nullptr;

		// OBJ-loaded character part models (own the part meshes the Characters render).
		// Destroyed in OnDestroy.
		static constexpr int k_PartModelCount = 5;
		Model* m_PartModels[k_PartModelCount] = {};
	};

	// Player orb: WASD movement, a radial melee swing (SPACE) with an expanding ground
	// ring, contact damage with an invulnerability window, and a hurt tint.
	class PlayerScript : public ScriptableEntity
	{
	public:
		PlayerScript(GameContext* context) : m_Context(context) {}

	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		void Attack();
		void UpdateAttackFx(float deltaTime);
		void UpdateRig(float deltaTime, float walkSpeed01);

	private:
		GameContext* m_Context = nullptr;
		float m_Invuln = 0.0f;
		float m_AttackCooldown = 0.0f;
		Entity m_AttackFx;          // expanding ring, briefly alive after a swing
		float m_AttackFxTime = 0.0f;

		Character m_Character;      // the visible hero; the rigid body is invisible
		float m_Facing = 0.0f;      // yaw (radians), kept while idle
	};

	// Skeleton: chases the player within aggro range, flashes when hit, and despawns
	// (counting toward EnemiesSlain) when its health runs out.
	class EnemyScript : public ScriptableEntity
	{
	public:
		EnemyScript(GameContext* context) : m_Context(context) {}

		void Damage(float amount);

	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		GameContext* m_Context = nullptr;
		float m_Health = ENEMY_MAX_HEALTH;
		float m_HitFlash = 0.0f;

		Character m_Character;      // the visible skeleton; the rigid body is invisible
		float m_Facing = 0.0f;      // yaw (radians), kept while idle
	};

	// Treasure: spins and bobs, and is collected by player proximity.
	class TreasureScript : public ScriptableEntity
	{
	public:
		TreasureScript(GameContext* context, float phase) : m_Context(context), m_Phase(phase) {}

	protected:
		void OnUpdate(float deltaTime) override;

	private:
		GameContext* m_Context = nullptr;
		float m_Phase = 0.0f;
	};

	// HUD: owns a 2D orthographic UI camera plus the health bar / counters / banner
	// entities, drawn by the SceneRenderer as an overlay on top of the 3D world. Its
	// OnStart builds the entities; OnUpdate lays them out (screen-space) and refreshes
	// their values from the GameContext.
	class HudScript : public ScriptableEntity
	{
	public:
		HudScript(GameContext* context) : m_Context(context) {}

	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		GameContext* m_Context = nullptr;
		Font* m_Font = nullptr;
		float m_OrthoSize = 11.0f;

		Entity m_BarBackground;
		Entity m_BarFill;
		Entity m_HealthText;
		Entity m_TreasureText;
		Entity m_ControlsText;
		Entity m_SeedText;
		Entity m_Banner;
	};
}
