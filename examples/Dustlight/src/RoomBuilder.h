#pragma once

#include <DingoEngine.h>

#include <vector>

namespace Dingo
{

	namespace RoomLayout
	{
		constexpr float k_HalfWidth = 6.0f;
		constexpr float k_Height = 5.0f;
		constexpr float k_HalfLength = 8.0f;
		constexpr float k_PlinthTop = 0.4f;
		constexpr glm::vec3 k_PlinthCentre = { 0.0f, 0.2f, 7.2f };
		constexpr glm::vec3 k_AlcoveCentre = { -7.0f, 1.6f, 0.0f };
		constexpr glm::vec3 k_AlcoveHalfExtents = { 1.0f, 1.6f, 2.0f };
	}

	class RoomBuilder
	{
	public:
		~RoomBuilder() { Release(); }

		void Build(Scene& scene);
		void Release();

	private:
		Entity AddBox(Scene& scene, const char* name, const glm::vec3& min, const glm::vec3& max, Material* material);
		Material* MakeMaterial(const glm::vec3& color, float roughness);

	private:
		Mesh* m_Box = nullptr;
		std::vector<Material*> m_Materials;
	};

}
