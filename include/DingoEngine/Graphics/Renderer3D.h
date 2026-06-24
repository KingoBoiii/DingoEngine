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
	// Between BeginScene()/EndScene() it transforms each submitted mesh on the CPU
	// into one shared vertex/index buffer, then issues a single indexed draw on
	// EndScene(). Depth testing is enabled (the swap-chain framebuffer carries a
	// depth attachment), so meshes occlude correctly regardless of submission order.
	//
	// The SceneRenderer drives it for entities that have a Transform3D + MeshRenderer
	// component (via Scene::RenderEntities3D); games can also drive it directly for
	// HUD-free 3D drawing the way Breakout3D rolls its own batcher.
	//
	// Note: a scene is a single batch. Submitting more than the configured capacity
	// drops the overflow (with a warning) instead of flushing mid-frame, which would
	// re-upload the shared buffer while a prior draw is still in flight on the render
	// thread. Raise Capabilities for very dense scenes.
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

		// Appends a mesh, transformed into world space on the CPU and flat-shaded with
		// the scene's directional light. No-op outside a Begin/EndScene pair.
		void SubmitMesh(const Mesh* mesh, const glm::mat4& transform, const glm::vec4& color);

		// Convenience primitives drawn with the renderer's built-in unit meshes
		// (a 1x1x1 box centred on the origin, and a unit-diameter sphere).
		void DrawBox(const glm::mat4& transform, const glm::vec4& color);
		void DrawSphere(const glm::mat4& transform, const glm::vec4& color);

		Mesh* GetBoxMesh() const { return m_BoxMesh; }
		Mesh* GetSphereMesh() const { return m_SphereMesh; }

	private:
		Renderer3D(const Renderer3DParams& params) : m_Params(params) {}

		void Flush();

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
		Material* m_Material = nullptr;
		VertexLayout m_Layout;

		GraphicsBuffer* m_VertexBuffer = nullptr;
		GraphicsBuffer* m_IndexBuffer = nullptr;

		Vertex* m_VertexBase = nullptr;
		Vertex* m_VertexPtr = nullptr;
		uint32_t* m_IndexBase = nullptr;
		uint32_t* m_IndexPtr = nullptr;
		uint32_t m_IndexCount = 0;
		uint32_t m_VertexOffset = 0;

		Mesh* m_BoxMesh = nullptr;
		Mesh* m_SphereMesh = nullptr;

		bool m_SceneActive = false;
		bool m_OverflowWarned = false;
	};

}
