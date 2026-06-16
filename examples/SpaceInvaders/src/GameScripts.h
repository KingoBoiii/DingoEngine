#pragma once
#include <DingoEngine.h>

#include "GameContext.h"
#include "GameTuning.h"

#include <glm/glm.hpp>

// All gameplay logic lives in ScriptableEntity subclasses. Each is attached to an
// entity with entity.AddScript<T>() and driven by the scene. Game-specific data
// lives as members of these scripts — no engine-side custom components, and no EnTT.
namespace Dingo
{

	// Identity + data for a single invader. Movement is handled by the formation
	// controller, so this script has no per-frame logic of its own.
	class InvaderScript : public ScriptableEntity
	{
	public:
		InvaderScript(int row, int col) : m_Row(row), m_Col(col) {}

		int Row() const { return m_Row; }
		int Column() const { return m_Col; }
		int Points() const { return (INVADER_ROWS - m_Row) * 10; }

	private:
		int m_Row = 0;
		int m_Col = 0;
	};

	// Marker behaviour for a destructible shield block, so projectiles can find
	// shields via Scene::GetScriptsOfType<ShieldScript>().
	class ShieldScript : public ScriptableEntity
	{
	};

	// A bullet (player) or bomb (invader): moves itself, despawns off-screen, and
	// resolves its own collisions.
	class ProjectileScript : public ScriptableEntity
	{
	public:
		ProjectileScript(GameContext* context, const glm::vec2& velocity, bool fromPlayer)
			: m_Context(context), m_Velocity(velocity), m_FromPlayer(fromPlayer) {}

		bool IsFromPlayer() const { return m_FromPlayer; }

	protected:
		void OnUpdate(float deltaTime) override;

	private:
		GameContext* m_Context = nullptr;
		glm::vec2 m_Velocity{ 0.0f };
		bool m_FromPlayer = true;
	};

	// The player ship: movement + firing.
	class PlayerScript : public ScriptableEntity
	{
	public:
		PlayerScript(GameContext* context) : m_Context(context) {}

	protected:
		void OnUpdate(float deltaTime) override;

	private:
		GameContext* m_Context = nullptr;
		float m_FireCooldown = 0.0f;
	};

	// Drives the whole invader formation: marching, edge-drop, the invasion check,
	// periodic bombing, and spawning the next wave when the formation is cleared.
	class FormationControllerScript : public ScriptableEntity
	{
	public:
		FormationControllerScript(GameContext* context) : m_Context(context) {}

	protected:
		void OnCreate() override;
		void OnUpdate(float deltaTime) override;

	private:
		void ResetForWave();

	private:
		GameContext* m_Context = nullptr;
		float m_Direction = 1.0f;
		float m_Speed = 0.0f;
		float m_FireTimer = 0.0f;
	};

	// --- Shared spawn helpers (used by both the GameLayer and the scripts) ---

	Entity SpawnProjectile(Scene& scene, GameContext& context, const glm::vec2& position, const glm::vec2& velocity, bool fromPlayer);
	void SpawnInvaderFormation(Scene& scene, GameContext& context);

}
