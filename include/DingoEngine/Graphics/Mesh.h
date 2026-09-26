#pragma once
#include <glm/glm.hpp>
#include <cstdint>
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

		// Never reused, unlike the Mesh's address, so a cache keyed on it cannot hand a
		// freed mesh's data to a new mesh allocated at the same address.
		std::uint64_t GetId() const { return m_Id; }

	private:
		static std::uint64_t AllocateId();

	private:
		std::vector<MeshVertex> m_Vertices;
		std::vector<uint32_t> m_Indices;
		std::uint64_t m_Id = AllocateId();
	};

}
