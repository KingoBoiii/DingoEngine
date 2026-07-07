#pragma once
#include <DingoEngine.h>

#include "GameContext.h"

#include <glm/glm.hpp>
#include <optional>
#include <vector>

// All of EchoVault's gameplay lives in these ScriptableEntity behaviours. The layer only
// creates the scenes and attaches the root controller scripts; each controller's OnStart
// builds its scene's world and the per-entity scripts drive everything from there.
namespace Dingo
{
	// Segment-patrol state shared by kinematic ping-pong movers (moving platforms, sentries).
	// Owns only the position math; callers derive facing/rotation themselves if they need it.
	struct PingPongPath
	{
		glm::vec3 A{ 0.0f };
		glm::vec3 B{ 0.0f };
		float Speed = 1.0f;
		float T = 0.0f;   // 0..1 along A->B
		int Dir = 1;      // +1 A->B, -1 B->A

		glm::vec3 Advance(float deltaTime);
	};

	// ======================================================================
	// Game scene
	// ======================================================================

	// Root "game manager" for the Game scene: builds the floating-platform course and owns
	// the shared GameContext. Its OnStart runs before physics (so the bodies + character
	// controller it spawns are baked when the scene starts); its OnUpdate advances the clock,
	// follows the camera, respawns on falls, and requests the Win transition when every orb
	// is collected.
	class CourseControllerScript : public ScriptableEntity
	{
	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		void LoadAudio();
		Entity SpawnPlatform(const glm::vec3& center, const glm::vec3& size, const glm::vec4& color);
		Entity SpawnMovingPlatform(const glm::vec3& a, const glm::vec3& b, const glm::vec3& size, float speed);
		Entity SpawnPlayer(const glm::vec3& feetPos);
		Entity SpawnOrb(const glm::vec3& position);
		Entity SpawnSentry(const glm::vec3& a, const glm::vec3& b, float speed);
		Entity SpawnGoalPad(const glm::vec3& center);
		void BuildCourse();
		void SetupCameraAndLight();
		void UpdateCamera(float deltaTime);

	private:
		GameContext m_Context;
		Entity m_CameraEntity;

		// Custom emissive shader + the materials orbs / sentry eyes render with.
		Shader* m_EmissiveShader = nullptr;
		Material* m_OrbMaterial = nullptr;
		Material* m_SentryEyeMaterial = nullptr;

		bool m_CameraInitialized = false;
	};

	// Player: a capsule character controller. Each frame reads WASD into a desired
	// horizontal velocity, keeps/gravity-integrates the vertical component, jumps on SPACE
	// when grounded, and adds the ground velocity so it rides moving platforms. Footsteps
	// play as timed spatialized one-shots while moving and grounded.
	class PlayerScript : public ScriptableEntity
	{
	public:
		PlayerScript(GameContext* context) : m_Context(context) {}

		// Applied by a sentry on detection.
		void ApplyKnockback(const glm::vec3& horizontalDir);

	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;

	private:
		GameContext* m_Context = nullptr;
		float m_VerticalVelocity = 0.0f;  // integrated separately; controller Y is authoritative when grounded
		float m_FootstepTimer = 0.0f;
		std::optional<glm::vec3> m_PendingKnockback;
	};

	// Kinematic moving platform: ping-pongs between A and B via Physics3D::MoveKinematic,
	// carrying whatever stands on it (the character controller reads its ground velocity).
	class MovingPlatformScript : public ScriptableEntity
	{
	public:
		MovingPlatformScript(GameContext* context, const glm::vec3& a, const glm::vec3& b, float speed)
			: m_Context(context), m_Path{ a, b, speed } {}

	protected:
		void OnUpdate(float deltaTime) override;

	private:
		GameContext* m_Context = nullptr;
		PingPongPath m_Path;
	};

	// Collectible orb: spins/bobs, chimes with a looping spatialized AudioSource so you can
	// navigate toward it by ear, and is collected by player proximity (pickup one-shot).
	class OrbScript : public ScriptableEntity
	{
	public:
		OrbScript(GameContext* context, float phase) : m_Context(context), m_Phase(phase) {}

	protected:
		void OnUpdate(float deltaTime) override;

	private:
		GameContext* m_Context = nullptr;
		float m_Phase = 0.0f;
		float m_BaseY = 0.0f;
		bool m_BaseYSet = false;
	};

	// Patrolling sentry: a kinematic body that ping-pongs and casts a line-of-sight ray
	// toward the player. When the player is inside its view cone/range and the ray reaches
	// the player uninterrupted, it knocks the player back and plays a detection sting.
	class SentryScript : public ScriptableEntity
	{
	public:
		SentryScript(GameContext* context, const glm::vec3& a, const glm::vec3& b, float speed)
			: m_Context(context), m_Path{ a, b, speed } {}

	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;

	private:
		bool HasLineOfSight(const glm::vec3& eye, const glm::vec3& target) const;

	private:
		GameContext* m_Context = nullptr;
		PingPongPath m_Path;
		float m_Cooldown = 0.0f;
		glm::vec3 m_Facing{ 0.0f, 0.0f, 1.0f };
	};

	// HUD: a 2D orthographic overlay (orb counter, hints, alert flash) drawn on top of the
	// 3D world by the SceneRenderer.
	class HudScript : public ScriptableEntity
	{
	public:
		HudScript(GameContext* context) : m_Context(context) {}

		void FlashAlert(); // pulsed when a sentry catches the player

	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		GameContext* m_Context = nullptr;
		Font* m_Font = nullptr;
		float m_OrthoSize = 11.0f;
		float m_Alert = 0.0f;
		bool m_AlertActive = false;

		Entity m_OrbText;
		Entity m_HintText;
		Entity m_AlertText;

		int m_LastCollected = -1;
		int m_LastTotalOrbs = -1;
	};

	// ======================================================================
	// Menu + Win scenes
	// ======================================================================

	// Menu: builds title/prompt text and, on SPACE/ENTER or gamepad A/Start, requests the
	// Game transition. The prompt adapts to the connected controller family.
	class MenuControllerScript : public ScriptableEntity
	{
	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		Font* m_Font = nullptr;
		Entity m_Prompt;
	};

	// Win: builds the victory text, plays a win fanfare on start, and returns to the Menu
	// on SPACE/ENTER or gamepad A/Start.
	class WinControllerScript : public ScriptableEntity
	{
	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		Font* m_Font = nullptr;
		Entity m_Prompt;
	};
}
