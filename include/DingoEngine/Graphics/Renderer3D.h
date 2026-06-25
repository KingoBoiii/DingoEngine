#pragma once
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/Graphics/Material.h"
#include "DingoEngine/Graphics/Mesh.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/Pipeline.h"

#include "DingoEngine/Core/PerspectiveCamera.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Dingo
{

	struct Renderer3DCapabilities
	{
		// Capacity of the single per-scene batch. A box is 24 verts / 36 indices and
		// a default sphere ~289 verts / 1536 indices, so the defaults comfortably hold
		// a few thousand primitives. Submissions past capacity are dropped (with a
		// warning) rather than triggering a mid-frame flush — see SubmitMesh.
		uint32_t MaxVertices = 65536;
		uint32_t MaxIndices = 98304;
	};

	struct Renderer3DParams
	{
		// One directional light, baked into the mesh shader. Direction points *from*
		// the light (i.e. the way the light travels); Ambient lifts the unlit faces so
		// nothing is fully black.
		glm::vec3 LightDirection = { -0.4f, -1.0f, -0.35f };
		float Ambient = 0.35f;

		Renderer3DCapabilities Capabilities = {};
	};

	// A batched, directional-lit mesh renderer — the 3D sibling of Renderer2D.
	//
	// Between BeginScene()/EndScene() it groups submitted meshes BY MATERIAL,
	// transforming each into a per-material vertex/index batch on the CPU, then issues
	// one indexed draw per material on EndScene() (each from its own pooled buffer).
	// Meshes with no explicit material use the built-in flat directional-lit default.
	// Depth testing is enabled (the swap-chain carries a depth attachment), so meshes
	// occlude correctly regardless of submission order.
	//
	// Camera + light live in a shared "scene" uniform buffer bound at binding 0 on
	// every material (Material::SetSceneUniformBuffer); a custom material's own uniforms
	// bind at 1 and its textures at 2+. The SceneRenderer drives this for
	// Transform3D + MeshRenderer entities (via Scene::RenderEntities3D).
	//
	// Note: each material's batch is capped at the configured capacity; submissions past
	// it are dropped (with a warning) rather than re-uploading a buffer mid-frame. Raise
	// Capabilities for very dense scenes.
	class Renderer3D
	{
	public:
		static Renderer3D* Create(const Renderer3DParams& params = {});

	public:
		Renderer3D() = delete;
		Renderer3D(const Renderer3D&) = delete;
		Renderer3D& operator=(const Renderer3D&) = delete;
		Renderer3D(Renderer3D&&) = delete;
		Renderer3D& operator=(Renderer3D&&) = delete;
		~Renderer3D() = default;

	public:
		void Initialize();
		void Shutdown();

		// Begins accumulating a batch for the given view. Does not clear — call
		// Clear() first if you want a fresh frame (it clears colour *and* depth).
		void BeginScene(const PerspectiveCamera& camera);
		void BeginScene(const glm::mat4& viewProjection);

		// Uploads and draws the accumulated batch.
		void EndScene();

		// Clears the current render target's colour and depth.
		void Clear(const glm::vec4& clearColor);

		// Updates the directional light used by the mesh shader. Takes effect on the
		// next BeginScene (which re-uploads the camera/light uniform). The SceneRenderer
		// drives this from a DirectionalLightComponent.
		void SetDirectionalLight(const glm::vec3& direction, float ambient);

		// Appends a mesh to the batch for the given material (null => the built-in
		// flat-lit default), transformed into world space on the CPU. The per-vertex
		// color is written into the vertex stream. No-op outside a Begin/EndScene pair.
		void SubmitMesh(const Mesh* mesh, const glm::mat4& transform, const glm::vec4& color, Material* material = nullptr);

		// Convenience primitives drawn with the renderer's built-in unit meshes
		// (a 1x1x1 box centred on the origin, and a unit-diameter sphere).
		void DrawBox(const glm::mat4& transform, const glm::vec4& color);
		void DrawSphere(const glm::mat4& transform, const glm::vec4& color);

		Mesh* GetBoxMesh() const { return m_BoxMesh; }
		Mesh* GetSphereMesh() const { return m_SphereMesh; }

	private:
		Renderer3D(const Renderer3DParams& params) : m_Params(params) {}

	private:
		Renderer3DParams m_Params;

		struct Vertex
		{
			glm::vec3 Position;
			glm::vec3 Normal;
			glm::vec4 Color;
		};

		// std140: a mat4 followed by two vec4s.
		struct CameraData
		{
			glm::mat4 ViewProjection{ 1.0f };
			glm::vec4 LightDirection{ 0.0f }; // xyz = direction
			glm::vec4 Ambient{ 0.0f };        // x = ambient strength
		};
		CameraData m_CameraData = {};

		Shader* m_Shader = nullptr;
		Material* m_Material = nullptr; // built-in flat-lit default material
		VertexLayout m_Layout;

		// Camera + light, uploaded once per BeginScene and bound at binding 0 on every
		// material the renderer draws (Material::SetSceneUniformBuffer).
		GraphicsBuffer* m_SceneUniformBuffer = nullptr;

		// One CPU batch per material, accumulated during the scene and drawn on EndScene.
		struct MeshBatch
		{
			std::vector<Vertex> Vertices;
			std::vector<uint32_t> Indices;
			bool OverflowWarned = false;
		};
		std::unordered_map<Material*, MeshBatch> m_Batches;

		// Pooled GPU buffers — one (vertex, index) pair per material batch drawn in a
		// frame, grown on demand and reused. Each batch gets its own buffer, so no shared
		// buffer is re-uploaded mid-frame.
		std::vector<GraphicsBuffer*> m_BatchVertexBuffers;
		std::vector<GraphicsBuffer*> m_BatchIndexBuffers;

		Mesh* m_BoxMesh = nullptr;
		Mesh* m_SphereMesh = nullptr;

		bool m_SceneActive = false;
	};

}
