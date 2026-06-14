#pragma once
#include <glm/glm.hpp>
#include <vector>

namespace Dingo
{

	struct MeshVertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoord;
	};

	class Mesh
	{
	public:
		static Mesh* Create(const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices);

		// Primitive factories
		static Mesh* CreateBox(float width = 1.0f, float height = 1.0f, float depth = 1.0f);
		static Mesh* CreateSphere(float radius = 0.5f, uint32_t rings = 16, uint32_t segments = 16);

	public:
		Mesh() = default;

		const std::vector<MeshVertex>& GetVertices() const { return m_Vertices; }
		const std::vector<uint32_t>& GetIndices() const { return m_Indices; }
		uint32_t GetVertexCount() const { return static_cast<uint32_t>(m_Vertices.size()); }
		uint32_t GetIndexCount() const { return static_cast<uint32_t>(m_Indices.size()); }

	private:
		std::vector<MeshVertex> m_Vertices;
		std::vector<uint32_t> m_Indices;
	};

}
