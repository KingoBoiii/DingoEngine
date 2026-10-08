#pragma once
#include "CameraRig.h"
#include "GameAssets.h"

#include <DingoEngine.h>

#include <span>
#include <vector>

namespace Dingo
{

	class ArenaVfx;
	class GameAudio;

	float GetArenaApothem();

	// How far a capsule of radius `margin` standing at `from` can go along the unit `direction` before it meets a
	// wall.
	float GetArenaFreeDistance(const glm::vec2& from, const glm::vec2& direction, float margin);

	// Draws with the assets' arena materials and mesh, which outlive it. Destroy it before the audio engine goes: it
	// stops the braziers' crackle.
	class ArenaWorld
	{
	public:
		// With `audio`, each brazier crackles in a loop.
		// vfx: the brazier flames' effects, or null for none.
		ArenaWorld(Scene& scene, const GameAssets& assets, const GameAudio* audio = nullptr, const ArenaVfx* vfx = nullptr);
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
			std::vector<Entity> Emitters;
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
		void BuildBraziers(const GameAudio* audio, const ArenaVfx* vfx);
		Entity SpawnEmitter(const char* name, ParticleEffect* effect, const glm::vec3& position);

	private:
		Scene& m_Scene;
		ArenaAssets m_Arena;
		Mesh* m_BoxMesh = nullptr;
		std::vector<Occluder> m_Occluders;
		std::vector<AudioSoundId> m_Crackles;
	};

}
