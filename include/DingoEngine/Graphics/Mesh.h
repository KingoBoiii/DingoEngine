#pragma once
#include <glm/glm.hpp>
#include <glm/ext/vector_uint4_sized.hpp>
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

	// What the GPU skins: positions and normals in skin space, the four heaviest joints
	// (indices into the owning Model's Skeleton) and their weights, which sum to 1.
	struct SkinnedMeshVertex
	{
		glm::vec3    Position;
		glm::vec3    Normal;
		glm::vec2    TexCoord;
		glm::u16vec4 Joints;
		glm::vec4    Weights;
	};

	class Mesh
	{
	public:
		static Mesh* Create(const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices);
		// restVertices are the skin posed at the skeleton's rest pose, in model space; they are
		// what GetVertices() returns, so physics and SubmitMesh see the rest pose.
		static Mesh* CreateSkinned(std::vector<MeshVertex> restVertices, std::vector<SkinnedMeshVertex> skinVertices, std::vector<uint32_t> indices);

		// Primitive factories
		static Mesh* CreateBox(float width = 1.0f, float height = 1.0f, float depth = 1.0f);
		static Mesh* CreateSphere(float radius = 0.5f, uint32_t rings = 16, uint32_t segments = 16);

	public:
		Mesh() = default;

		const std::vector<MeshVertex>& GetVertices() const { return m_Vertices; }
		const std::vector<uint32_t>& GetIndices() const { return m_Indices; }
		uint32_t GetVertexCount() const { return static_cast<uint32_t>(m_Vertices.size()); }
		uint32_t GetIndexCount() const { return static_cast<uint32_t>(m_Indices.size()); }

		bool HasSkin() const { return !m_SkinVertices.empty(); }
		const std::vector<SkinnedMeshVertex>& GetSkinVertices() const { return m_SkinVertices; }

		// Never reused, unlike the Mesh's address, so a cache keyed on it cannot hand a
		// freed mesh's data to a new mesh allocated at the same address.
		std::uint64_t GetId() const { return m_Id; }

	private:
		static std::uint64_t AllocateId();

	private:
		std::vector<MeshVertex> m_Vertices;
		std::vector<uint32_t> m_Indices;
		std::vector<SkinnedMeshVertex> m_SkinVertices;
		std::uint64_t m_Id = AllocateId();
	};

}
