#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Dingo
{

	class KeepMap;

	class Player
	{
	public:
		Player(Scene& scene, const KeepMap& map, const glm::ivec2& spawnTile);
		~Player();

		Player(const Player&) = delete;
		Player& operator=(const Player&) = delete;

		void Update(float deltaTime);

		Entity GetEntity() const { return m_Entity; }
		glm::vec3 GetPosition() const;

		// Walking this frame: steering, unlocked, on the ground and carried forward by the last step,
		// so pushing against a wall is not walking.
		bool IsMoving() const { return m_Moving; }

		// The way the body faces: local +Z is forward.
		glm::quat GetFacing() const;

		// Takes effect on the next Update, so a lock set by a later subsystem trails by one frame.
		void SetMovementLocked(bool locked) { m_MovementLocked = locked; }

		// Locks movement and stops the controller now, for a subsystem that runs after Update.
		void Halt();

		void Teleport(const glm::vec3& feet);

	private:
		void PlaceVisuals(const glm::vec3& feet);

	private:
		Scene& m_Scene;

		Entity m_Entity;
		Entity m_Body;
		Entity m_Head;
		Entity m_Visor;

		Material* m_CloakMaterial = nullptr;
		Material* m_FaceMaterial = nullptr;

		glm::vec3 m_LastFeet{ 0.0f };
		float m_VerticalVelocity = 0.0f;
		float m_Yaw = 0.0f;
		bool m_MovementLocked = false;
		bool m_Moving = false;
	};

}
