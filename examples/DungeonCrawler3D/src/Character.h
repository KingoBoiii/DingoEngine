#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// An animated low-poly character for the 3D dungeon crawler.
//
// DingoEngine's model loader bakes transforms (no skeletal animation), so the character is
// assembled from part entities (head / torso / two arms / two legs / optional sword), each
// holding one loaded part mesh, parented into a small rig: a root that stands at the feet and
// faces the walk direction, a joint entity at each hip and shoulder, and every part hung from
// its joint (the sword from the right shoulder, so it swings with the arm). Animating it is
// setting the root's pose and the joints' local rotations. It plays an idle breath, a walk
// cycle, and an attack in which the right arm and sword swing in an overhead chop.
//
// The character is a pure visual — pair it with an invisible rigid body for movement
// (see DungeonCrawler3D's PlayerScript / EnemyScript) and point the rig at the body.
namespace Dingo
{
	// The shared part meshes (loaded once from assets/models/parts/*.obj). The same set
	// drives every character; per-instance look comes from the colour passed to Create.
	struct CharacterMeshes
	{
		Mesh* Head = nullptr;
		Mesh* Torso = nullptr;
		Mesh* Arm = nullptr;   // reused for both arms
		Mesh* Leg = nullptr;   // reused for both legs
		Mesh* Sword = nullptr;
	};

	class Character
	{
	public:
		// Spawns the rig into `scene`. `color` tints the body; the sword (if `withSword`) gets a
		// fixed steel colour. Call Destroy() to remove it.
		void Create(Scene& scene, const CharacterMeshes& meshes, const glm::vec4& color, bool withSword);

		// Destroys the rig. Idempotent.
		void Destroy();

		// Position + animate. `feetPosition` is where the character stands; `targetYaw`
		// the desired facing about +Y (radians, eased toward); `walkSpeed01` in [0,1]
		// blends idle -> walk; `deltaTime` advances the walk / attack timers.
		void Update(const glm::vec3& feetPosition, float targetYaw, float walkSpeed01, float deltaTime);

		// Tint every part toward `color` by `amount` in [0,1] (hurt / hit flash).
		void SetFlash(const glm::vec4& color, float amount);

		// Start a one-shot overhead chop of the right arm + sword.
		void TriggerAttack();

		// Start a one-shot hit reaction: the whole body recoils backward about the feet.
		void TriggerHit();

		bool IsValid() const { return m_Created; }
		float FacingYaw() const { return m_Yaw; }

	private:
		enum Joint { HipL, HipR, ShoulderL, ShoulderR, JointCount };
		enum Part { Head, Torso, ArmL, ArmR, LegL, LegR, Sword, PartCount };

	private:
		Entity m_Root;
		Entity m_Joints[JointCount];
		Entity m_Parts[PartCount];
		glm::vec4 m_BaseColors[PartCount];
		bool m_Created = false;

		float m_Yaw = 0.0f;
		float m_WalkPhase = 0.0f;
		float m_AttackTime = 0.0f;
		float m_HitTime = 0.0f;

		glm::vec4 m_FlashColor{ 1.0f };
		float m_FlashAmount = 0.0f;
	};
}
