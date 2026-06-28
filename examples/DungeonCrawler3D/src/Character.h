#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// An animated low-poly character for the 3D dungeon crawler.
//
// DingoEngine's model loader bakes transforms (no skeletal animation) and the ECS has
// no transform hierarchy, so the character is assembled from separate part *entities*
// (head / torso / two arms / two legs / optional sword), each holding one loaded part
// mesh, and animated procedurally: the rig bakes every part's world transform each
// frame from a single root (feet + facing), rotating the limbs about their joints. It
// plays an idle breath, a walk cycle, and an attack in which the right arm and sword
// swing in an overhead chop.
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
		// Spawns the part entities into `scene`. `color` tints the body; the sword (if
		// `withSword`) gets a fixed steel colour. Call Destroy() to remove them.
		void Create(Scene& scene, const CharacterMeshes& meshes, const glm::vec4& color, bool withSword);

		// Destroys all part entities. Idempotent.
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
		enum Part { Head, Torso, ArmL, ArmR, LegL, LegR, Sword, PartCount };

		struct Bone
		{
			Entity entity;
			glm::vec3 restCenter{ 0.0f }; // world pos of the mesh origin at rest, vs the feet
			glm::vec3 pivot{ 0.0f };      // joint to rotate about (== restCenter for rigid parts)
			glm::vec4 baseColor{ 1.0f };
			glm::quat jointRot{ 1.0f, 0.0f, 0.0f, 0.0f }; // recomputed each frame
			glm::quat meshRot{ 1.0f, 0.0f, 0.0f, 0.0f };  // fixed mesh orientation (e.g. the sword grip)
		};

		void PlaceBone(Bone& bone, const glm::vec3& feet, const glm::quat& yaw, float bobY);

	private:
		Bone m_Bones[PartCount];
		bool m_Created = false;
		bool m_HasSword = false;

		float m_Yaw = 0.0f;
		float m_WalkPhase = 0.0f;
		float m_AttackTime = 0.0f;
		float m_HitTime = 0.0f;

		glm::vec4 m_FlashColor{ 1.0f };
		float m_FlashAmount = 0.0f;
	};
}
