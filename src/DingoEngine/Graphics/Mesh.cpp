#include "depch.h"
#include "DingoEngine/Graphics/Mesh.h"

#include <glm/gtc/constants.hpp>
#include <cmath>

namespace Dingo
{

	Mesh* Mesh::Create(const std::vector<MeshVertex>& vertices, const std::vector<uint16_t>& indices)
	{
		Mesh* mesh = new Mesh();
		mesh->m_Vertices = vertices;
		mesh->m_Indices = indices;
		return mesh;
	}

	Mesh* Mesh::CreateBox(float width, float height, float depth)
	{
		float hw = width  * 0.5f;
		float hh = height * 0.5f;
		float hd = depth  * 0.5f;

		std::vector<MeshVertex> vertices = {
			// back face  (z = -hd)
			{{ -hw, -hh, -hd }},  // 0 back-bottom-left
			{{  hw, -hh, -hd }},  // 1 back-bottom-right
			{{  hw,  hh, -hd }},  // 2 back-top-right
			{{ -hw,  hh, -hd }},  // 3 back-top-left
			// front face (z = +hd)
			{{ -hw, -hh,  hd }},  // 4 front-bottom-left
			{{  hw, -hh,  hd }},  // 5 front-bottom-right
			{{  hw,  hh,  hd }},  // 6 front-top-right
			{{ -hw,  hh,  hd }},  // 7 front-top-left
		};

		std::vector<uint16_t> indices = {
			// front  (z = +hd)
			4, 5, 6,   4, 6, 7,
			// back   (z = -hd)
			1, 0, 3,   1, 3, 2,
			// left   (x = -hw)
			0, 4, 7,   0, 7, 3,
			// right  (x = +hw)
			5, 1, 2,   5, 2, 6,
			// top    (y = +hh)
			7, 6, 2,   7, 2, 3,
			// bottom (y = -hh)
			0, 1, 5,   0, 5, 4,
		};

		return Create(vertices, indices);
	}

	Mesh* Mesh::CreateSphere(float radius, uint32_t rings, uint32_t segments)
	{
		std::vector<MeshVertex> vertices;
		std::vector<uint16_t> indices;

		const float pi = glm::pi<float>();
		const float twoPi = glm::two_pi<float>();

		for (uint32_t ring = 0; ring <= rings; ++ring)
		{
			float theta = pi * static_cast<float>(ring) / static_cast<float>(rings);
			float sinTheta = std::sin(theta);
			float cosTheta = std::cos(theta);

			for (uint32_t seg = 0; seg <= segments; ++seg)
			{
				float phi = twoPi * static_cast<float>(seg) / static_cast<float>(segments);
				float sinPhi = std::sin(phi);
				float cosPhi = std::cos(phi);

				glm::vec3 pos = {
					radius * sinTheta * cosPhi,
					radius * cosTheta,
					radius * sinTheta * sinPhi
				};

				vertices.push_back({ pos });
			}
		}

		for (uint32_t ring = 0; ring < rings; ++ring)
		{
			for (uint32_t seg = 0; seg < segments; ++seg)
			{
				uint16_t i0 = static_cast<uint16_t>(ring * (segments + 1) + seg);
				uint16_t i1 = static_cast<uint16_t>(i0 + 1);
				uint16_t i2 = static_cast<uint16_t>((ring + 1) * (segments + 1) + seg);
				uint16_t i3 = static_cast<uint16_t>(i2 + 1);

				indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
				indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
			}
		}

		return Create(vertices, indices);
	}

}
