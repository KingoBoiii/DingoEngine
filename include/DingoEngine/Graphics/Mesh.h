#pragma once
#include <glm/glm.hpp>
#include <glm/ext/vector_uint4_sized.hpp>
#include <cstdint>
#include <vector>

namespace Dingo
{

	class GraphicsBuffer;

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
		~Mesh();
		Mesh(const Mesh&) = delete;
		Mesh& operator=(const Mesh&) = delete;

		const std::vector<MeshVertex>& GetVertices() const { return m_Vertices; }
		const std::vector<uint32_t>& GetIndices() const { return m_Indices; }
		uint32_t GetVertexCount() const { return static_cast<uint32_t>(m_Vertices.size()); }
		uint32_t GetIndexCount() const { return static_cast<uint32_t>(m_Indices.size()); }
		// The model-space box around GetVertices(); min above max for a mesh without vertices.
		const glm::vec3& GetBoundsMin() const { return m_BoundsMin; }
		const glm::vec3& GetBoundsMax() const { return m_BoundsMax; }

		bool HasSkin() const { return !m_SkinVertices.empty(); }
		const std::vector<SkinnedMeshVertex>& GetSkinVertices() const { return m_SkinVertices; }
		// One past the highest joint a skin vertex names: the palette entries a skinned draw needs.
		uint32_t GetSkinJointCount() const { return m_SkinJointCount; }

		// Never reused, unlike the Mesh's address, so a cache keyed on it cannot hand a
		// freed mesh's data to a new mesh allocated at the same address.
		std::uint64_t GetId() const { return m_Id; }

	private:
		static std::uint64_t AllocateId();

		// A model reload: the source's vertices, a new id (Jolt caches shapes by it) and no GPU copy,
		// so the next skinned draw uploads the new skin.
		void Reinitialize(Mesh& source);
		void Clear();
		void ComputeBounds();

		friend class Model;

	private:
		std::vector<MeshVertex> m_Vertices;
		std::vector<uint32_t> m_Indices;
		std::vector<SkinnedMeshVertex> m_SkinVertices;
		uint32_t m_SkinJointCount = 0;
		glm::vec3 m_BoundsMin{ 1.0f };
		glm::vec3 m_BoundsMax{ -1.0f };
		std::uint64_t m_Id = AllocateId();

		// GPU copies of the skin, made by Renderer3D on the first skinned draw.
		mutable GraphicsBuffer* m_SkinVertexBuffer = nullptr;
		mutable GraphicsBuffer* m_SkinIndexBuffer  = nullptr;

		friend class Renderer3D;
	};

}
