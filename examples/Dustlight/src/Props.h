#pragma once

#include <DingoEngine.h>

#include <vector>

namespace Dingo
{

	class Props
	{
	public:
		~Props() { Release(); }

		void Build(Scene& scene);
		void Release();

		Entity GetEmissiveSphere() const { return m_EmissiveSphere; }
		int GetGridSphereCount() const { return static_cast<int>(m_GridEntities.size()); }

	private:
		void Place(Scene& scene, const char* path, const char* name, const glm::vec3& position, float scale, float yawDegrees);
		void BuildSphereGrid(Scene& scene);

	private:
		Mesh* m_Sphere = nullptr;
		std::vector<Material*> m_Materials;
		std::vector<Entity> m_GridEntities;
		Entity m_EmissiveSphere;
	};

}
