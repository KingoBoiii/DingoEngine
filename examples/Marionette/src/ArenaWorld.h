#pragma once
#include <DingoEngine.h>

#include <vector>

namespace Dingo
{

	class ArenaWorld
	{
	public:
		explicit ArenaWorld(Scene& scene);
		~ArenaWorld();

		ArenaWorld(const ArenaWorld&) = delete;
		ArenaWorld& operator=(const ArenaWorld&) = delete;

		std::vector<glm::vec3> GetRimPoints() const;

	private:
		Entity SpawnSolid(const char* name, const glm::vec3& center, const glm::vec3& size, float yawRadians, const glm::vec4& color, Material* material);
		void BuildFloor();
		void BuildWalls();
		void BuildBraziers();

	private:
		Scene& m_Scene;
		Mesh* m_BoxMesh = nullptr;
		Mesh* m_FlameMesh = nullptr;
		Material* m_FloorMaterial = nullptr;
		Material* m_WallMaterial = nullptr;
		Material* m_BrazierMaterial = nullptr;
		Material* m_FlameMaterial = nullptr;
	};

}
