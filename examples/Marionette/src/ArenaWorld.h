#pragma once
#include "CameraRig.h"

#include <DingoEngine.h>

#include <span>
#include <vector>

namespace Dingo
{

	class GameAudio;

	float GetArenaApothem();

	// Destroy it before the audio engine goes: it stops the braziers' crackle.
	class ArenaWorld
	{
	public:
		// With `audio`, each brazier crackles in a loop.
		ArenaWorld(Scene& scene, const GameAudio* audio = nullptr);
		~ArenaWorld();

		ArenaWorld(const ArenaWorld&) = delete;
		ArenaWorld& operator=(const ArenaWorld&) = delete;

		std::vector<glm::vec3> GetRimPoints() const;

		// Hides the braziers and wall pieces that stand between the camera and the subjects (their lights keep
		// shining) and shows them again once the view has been clear for a moment.
		void UpdateOcclusion(float deltaTime, const glm::vec3& eye, std::span<const CameraSubject> subjects);

	private:
		struct Occluder
		{
			std::vector<Entity> Parts;
			glm::vec3 Center{ 0.0f };
			glm::vec3 HalfSize{ 0.0f };
			float Yaw = 0.0f;
			bool Hidden = false;
			float ClearFor = 0.0f;
		};

		static void SetVisible(Occluder& occluder, bool visible);
		Entity SpawnSolid(const char* name, const glm::vec3& center, const glm::vec3& size, float yawRadians, const glm::vec4& color, Material* material);
		void BuildFloor();
		void BuildWalls();
		void BuildBraziers(const GameAudio* audio);

	private:
		Scene& m_Scene;
		Mesh* m_BoxMesh = nullptr;
		Mesh* m_FlameMesh = nullptr;
		Material* m_FloorMaterial = nullptr;
		Material* m_WallMaterial = nullptr;
		Material* m_BrazierMaterial = nullptr;
		Material* m_FlameMaterial = nullptr;
		std::vector<Occluder> m_Occluders;
		std::vector<AudioSoundId> m_Crackles;
	};

}
