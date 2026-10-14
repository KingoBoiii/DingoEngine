#pragma once
#include <glm/glm.hpp>
#include <glm/ext/vector_uint4_sized.hpp>
#include <cstdint>
#include <vector>

namespace Dingo
{

	class GraphicsBuffer;

	/// One vertex of a static Mesh.
	struct MeshVertex
	{
		glm::vec3 Position; ///< Position in the mesh's own (model) space.
		glm::vec3 Normal; ///< Unit surface normal in the same space.
		/// Texture coordinates, in 0..1 across the image for the built-in shapes.
		glm::vec2 TexCoord;
	};

	/// What the GPU skins: positions and normals in skin space, the four heaviest joints
	/// (indices into the owning Model's Skeleton) and their weights, which sum to 1.
	struct SkinnedMeshVertex
	{
		glm::vec3    Position; ///< Position in skin space, before the joint palette is applied.
		glm::vec3    Normal; ///< Unit normal in skin space.
		glm::vec2    TexCoord; ///< Texture coordinates.
		glm::u16vec4 Joints; ///< The four joints that move the vertex, as Skeleton joint indices.
		glm::vec4    Weights; ///< Each joint's weight, in the order of Joints; they sum to 1.
	};

	/// Triangle-list geometry on the CPU, with a never-reused id. Its GPU copies are made by
	/// Renderer3D when a draw first needs them and freed with the mesh.
	class Mesh
	{
	public:
		/// Copies the vertices and indices (three per triangle) into a new mesh and computes its
		/// bounds. The caller owns the result and deletes it; a Model's meshes belong to the Model.
		[[nodiscard]] static Mesh* Create(const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices);
		/// A skinned mesh; takes ownership of the data. restVertices are the skin posed at the
		/// skeleton's rest pose, in model space; they are what GetVertices() returns, so physics and
		/// SubmitMesh see the rest pose. Returns nullptr unless there is one skin vertex per rest
		/// vertex. The caller owns the result and deletes it.
		[[nodiscard]] static Mesh* CreateSkinned(std::vector<MeshVertex> restVertices, std::vector<SkinnedMeshVertex> skinVertices, std::vector<uint32_t> indices);

		// Primitive factories
		/// A box centred on the origin, `width` along X, `height` along Y and `depth` along Z, in
		/// world units; 24 vertices so each face has its own normal and UVs. The caller owns it.
		[[nodiscard]] static Mesh* CreateBox(float width = 1.0f, float height = 1.0f, float depth = 1.0f);
		/// A UV sphere centred on the origin with its poles on the Y axis. `rings` split it from
		/// pole to pole and `segments` around the Y axis; rings is raised to at least 2 and
		/// segments to at least 3, the fewest that enclose a volume. The caller owns it.
		[[nodiscard]] static Mesh* CreateSphere(float radius = 0.5f, uint32_t rings = 16, uint32_t segments = 16);

	public:
		/// An empty mesh; use a Create function for real geometry.
		Mesh() = default;
		/// Frees the mesh's GPU copies; never delete a mesh a frame still draws.
		~Mesh();
		///@{
		/// Copies would share an id and GPU buffers.
		Mesh(const Mesh&) = delete;
		Mesh& operator=(const Mesh&) = delete;
		///@}

		/// The vertices; for a skinned mesh the rest pose in model space.
		const std::vector<MeshVertex>& GetVertices() const { return m_Vertices; }
		/// Three indices per triangle into GetVertices().
		const std::vector<uint32_t>& GetIndices() const { return m_Indices; }
		/// The number of vertices in GetVertices().
		uint32_t GetVertexCount() const { return static_cast<uint32_t>(m_Vertices.size()); }
		/// The number of indices; a third of it is the triangle count.
		uint32_t GetIndexCount() const { return static_cast<uint32_t>(m_Indices.size()); }
		/// The model-space box around GetVertices(); min above max for a mesh without vertices.
		const glm::vec3& GetBoundsMin() const { return m_BoundsMin; }
		/// The upper corner of the box GetBoundsMin() starts.
		const glm::vec3& GetBoundsMax() const { return m_BoundsMax; }

		/// True for a mesh made by CreateSkinned with vertices.
		bool HasSkin() const { return !m_SkinVertices.empty(); }
		/// The skin vertices, parallel to GetVertices(); empty for a static mesh.
		const std::vector<SkinnedMeshVertex>& GetSkinVertices() const { return m_SkinVertices; }
		/// One past the highest joint a skin vertex names: the palette entries a skinned draw
		/// needs.
		uint32_t GetSkinJointCount() const { return m_SkinJointCount; }

		/// Never reused, unlike the Mesh's address, so a cache keyed on it cannot hand a
		/// freed mesh's data to a new mesh allocated at the same address. A model reload gives each
		/// of its meshes a new id.
		std::uint64_t GetId() const { return m_Id; }

	private:
		static std::uint64_t AllocateId();

		// A model reload: the source's vertices, a new id (Jolt caches shapes by it) and no GPU copy,
		// so the next skinned or instanced draw uploads the new vertices.
		void Reinitialize(Mesh& source);
		// Bumped by every Reinitialize, so a static batch walks its meshes' ids only after a reload.
		static std::uint64_t GetReinitializeCount();
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
		// GPU copies of GetVertices() and the indices, made by Renderer3D on the first instanced draw.
		mutable GraphicsBuffer* m_StaticVertexBuffer = nullptr;
		mutable GraphicsBuffer* m_StaticIndexBuffer  = nullptr;

		friend class Renderer3D;
	};

}
