#include "depch.h"
#include "DingoEngine/Graphics/Mesh.h"

#include <glm/gtc/constants.hpp>
#include <cmath>

namespace Dingo
{

	Mesh* Mesh::Create(const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices)
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

		// 24 vertices (4 per face × 6 faces) so each face gets its own normals and UVs
		std::vector<MeshVertex> vertices = {
			// Front face (z = +hd, normal = +Z)
			{{ -hw, -hh,  hd }, { 0, 0, 1 }, { 0, 0 }},
			{{  hw, -hh,  hd }, { 0, 0, 1 }, { 1, 0 }},
			{{  hw,  hh,  hd }, { 0, 0, 1 }, { 1, 1 }},
			{{ -hw,  hh,  hd }, { 0, 0, 1 }, { 0, 1 }},
			// Back face (z = -hd, normal = -Z)
			{{  hw, -hh, -hd }, { 0, 0,-1 }, { 0, 0 }},
			{{ -hw, -hh, -hd }, { 0, 0,-1 }, { 1, 0 }},
			{{ -hw,  hh, -hd }, { 0, 0,-1 }, { 1, 1 }},
			{{  hw,  hh, -hd }, { 0, 0,-1 }, { 0, 1 }},
			// Left face (x = -hw, normal = -X)
			{{ -hw, -hh, -hd }, {-1, 0, 0 }, { 0, 0 }},
			{{ -hw, -hh,  hd }, {-1, 0, 0 }, { 1, 0 }},
			{{ -hw,  hh,  hd }, {-1, 0, 0 }, { 1, 1 }},
			{{ -hw,  hh, -hd }, {-1, 0, 0 }, { 0, 1 }},
			// Right face (x = +hw, normal = +X)
			{{  hw, -hh,  hd }, { 1, 0, 0 }, { 0, 0 }},
			{{  hw, -hh, -hd }, { 1, 0, 0 }, { 1, 0 }},
			{{  hw,  hh, -hd }, { 1, 0, 0 }, { 1, 1 }},
			{{  hw,  hh,  hd }, { 1, 0, 0 }, { 0, 1 }},
			// Top face (y = +hh, normal = +Y)
			{{ -hw,  hh,  hd }, { 0, 1, 0 }, { 0, 0 }},
			{{  hw,  hh,  hd }, { 0, 1, 0 }, { 1, 0 }},
			{{  hw,  hh, -hd }, { 0, 1, 0 }, { 1, 1 }},
			{{ -hw,  hh, -hd }, { 0, 1, 0 }, { 0, 1 }},
			// Bottom face (y = -hh, normal = -Y)
			{{ -hw, -hh, -hd }, { 0,-1, 0 }, { 0, 0 }},
			{{  hw, -hh, -hd }, { 0,-1, 0 }, { 1, 0 }},
			{{  hw, -hh,  hd }, { 0,-1, 0 }, { 1, 1 }},
			{{ -hw, -hh,  hd }, { 0,-1, 0 }, { 0, 1 }},
		};

		std::vector<uint32_t> indices;
		indices.reserve(36);
		for (uint32_t face = 0; face < 6; ++face)
		{
			uint32_t base = face * 4;
			indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
			indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);
		}

		return Create(vertices, indices);
	}

	Mesh* Mesh::CreateSphere(float radius, uint32_t rings, uint32_t segments)
	{
		std::vector<MeshVertex> vertices;
		std::vector<uint32_t> indices;

		const float pi    = glm::pi<float>();
		const float twoPi = glm::two_pi<float>();

		for (uint32_t ring = 0; ring <= rings; ++ring)
		{
			float theta    = pi * static_cast<float>(ring) / static_cast<float>(rings);
			float sinTheta = std::sin(theta);
			float cosTheta = std::cos(theta);

			for (uint32_t seg = 0; seg <= segments; ++seg)
			{
				float phi    = twoPi * static_cast<float>(seg) / static_cast<float>(segments);
				float sinPhi = std::sin(phi);
				float cosPhi = std::cos(phi);

				glm::vec3 pos = {
					radius * sinTheta * cosPhi,
					radius * cosTheta,
					radius * sinTheta * sinPhi
				};
				glm::vec3 normal   = glm::normalize(pos);
				glm::vec2 texcoord = {
					static_cast<float>(seg)  / static_cast<float>(segments),
					static_cast<float>(ring) / static_cast<float>(rings)
				};

				vertices.push_back({ pos, normal, texcoord });
			}
		}

		for (uint32_t ring = 0; ring < rings; ++ring)
		{
			for (uint32_t seg = 0; seg < segments; ++seg)
			{
				uint32_t i0 = ring * (segments + 1) + seg;
				uint32_t i1 = i0 + 1;
				uint32_t i2 = (ring + 1) * (segments + 1) + seg;
				uint32_t i3 = i2 + 1;

				indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
				indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
			}
		}

		return Create(vertices, indices);
	}

}
