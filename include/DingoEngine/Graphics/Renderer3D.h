#pragma once
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/Graphics/Material.h"
#include "DingoEngine/Graphics/Mesh.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/Light.h"
#include "DingoEngine/Graphics/Pipeline.h"

#include "DingoEngine/Core/PerspectiveCamera.h"

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace Dingo
{

	struct Renderer3DCapabilities
	{
		// Capacity of one batch, i.e. one indexed draw. A box is 24 verts / 36 indices and
		// a default sphere ~289 verts / 1536 indices. A material that outgrows a batch
		// spills into another one; only a single mesh too large for an empty batch is
		// dropped, with a warning.
		uint32_t MaxVertices = 65536;
		uint32_t MaxIndices = 98304;

		// Point and spot lights drawn per scene, at most Renderer3D::k_MaxLocalLights. Lights
		// whose range can't reach anything in view are skipped first. Past the budget the
		// brightest as seen from the camera are kept and the rest dropped with a warning; ties go
		// to the light the camera is nearer to relative to its range, then to the earlier-submitted
		// one, so a still scene picks the same lights every frame.
		uint32_t MaxLocalLights = 32;

		// Skinned instances a frame, across every scene this renderer runs, at most
		// Renderer3D::k_MaxSkinnedInstancesLimit. An instance is a run of SubmitSkinnedMesh calls with
		// the same palette, transform and colour (a model's submeshes); it uploads its joints once,
		// to a volatile buffer that on Vulkan has room for this many writes a frame. Later instances
		// that frame are skipped whole, with a warning (Statistics::DroppedSkinnedDraws).
		uint32_t MaxSkinnedInstances = 64;

		// When true, a mesh too large for an empty batch, a light past the light budget or a
		// skinned instance past MaxSkinnedInstances trips an assert instead of the default
		// warn-once-and-drop. Asserts are compiled out in release, where it warns and drops
		// regardless.
		bool AssertOnOverflow = false;
	};

	struct Renderer3DParams
	{
		// The default light: what a scene that submits no light and no ambient of its own is lit
		// by, so code written before scene lighting existed keeps its look. Direction is the way
		// the light travels; Ambient lifts every face, and lit faces get the remaining
		// (1 - Ambient) scaled by how squarely they face the light.
		glm::vec3 LightDirection = { -0.4f, -1.0f, -0.35f };
		float Ambient = 0.35f;

		Renderer3DCapabilities Capabilities = {};
	};

	// A batched, forward-lit mesh renderer — the 3D sibling of Renderer2D.
	//
	// Between BeginScene()/EndScene() it groups submitted meshes BY MATERIAL,
	// transforming each into a per-material vertex/index batch on the CPU, then issues
	// one indexed draw per batch on EndScene() (each from its own pooled buffer).
	// Meshes with no explicit material use the built-in lit default material.
	// Depth testing is enabled (the swap-chain carries a depth attachment), so meshes
	// occlude correctly regardless of submission order.
	//
	// Camera + lights live in a shared "scene" uniform buffer bound at binding 0 on
	// every material (Material::SetSceneUniformBuffer); a custom material's own uniforms
	// bind at 1 and its textures at 2+. The SceneRenderer drives this for
	// Transform3D + MeshRenderer entities (via Scene::SubmitLights and
	// Scene::RenderEntities3D).
	//
	// Note: each batch is capped at the configured capacity. A material that outgrows one
	// spills into another batch — one more draw call — so raise Capabilities to cut draw
	// calls in very dense scenes.
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
		~Renderer3D();

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

		// Scene lighting is rebuilt for every scene: lights and ambient submitted since the last
		// EndScene - before or after BeginScene - light the next EndScene, which then clears
		// them. A scene that submits no light and no ambient is lit by the default light instead.
		// Up to k_MaxDirectionalLights directional lights count; further ones are dropped with a
		// warning. Point and spot lights share the MaxLocalLights budget (see
		// Renderer3DCapabilities). SetAmbientLight replaces the scene's ambient, which is black
		// otherwise. SubmitLight returns false for a light it ignores (non-finite, or a point or spot
		// light without positive intensity and range) or drops (a fifth directional light, or one
		// past the pending-light cap).
		bool SubmitLight(const DirectionalLight& light);
		bool SubmitLight(const PointLight& light);
		bool SubmitLight(const SpotLight& light);
		void SetAmbientLight(const glm::vec3& color, float intensity);

		// Replaces the default light (Renderer3DParams::LightDirection/Ambient).
		void SetDirectionalLight(const glm::vec3& direction, float ambient);

		static constexpr uint32_t k_MaxDirectionalLights = 4;
		static constexpr uint32_t k_MaxLocalLights = 32;

		// Scenes a renderer can run in one frame. On Vulkan each EndScene writes the volatile scene
		// buffer, which has room for this many writes a frame; later scenes draw with stale lighting.
		static constexpr uint32_t k_MaxScenesPerFrame = 32;

		// Appends a mesh to the batch for the given material (null => the built-in
		// lit default), transformed into world space on the CPU. The vertex stream is
		// a_Position (0), a_Normal (1), a_Color (2, the color passed here) and a_TexCoord
		// (3, the mesh's UVs, for custom materials that sample a texture). No-op outside a
		// Begin/EndScene pair.
		void SubmitMesh(const Mesh* mesh, const glm::mat4& transform, const glm::vec4& color, Material* material = nullptr);

		// Convenience primitives drawn with the renderer's built-in unit meshes
		// (a 1x1x1 box centred on the origin, and a unit-diameter sphere).
		void DrawBox(const glm::mat4& transform, const glm::vec4& color);
		void DrawSphere(const glm::mat4& transform, const glm::vec4& color);

		// The most joints one skinned draw can use: the length of SkinData's palette.
		static constexpr uint32_t k_MaxSkinJoints = 128;
		// The highest binding a custom shader may give its SkinData block; D3D11 has 14 constant
		// buffer slots.
		static constexpr uint32_t k_MaxSkinDataBinding = 13;
		// Each instance of the budget holds a SkinData version in host-visible memory on Vulkan.
		static constexpr uint32_t k_MaxSkinnedInstancesLimit = 256;

		// Skins a mesh on the GPU and places it with transform, one draw per call. joints is the
		// skinning palette (Skeleton::ComputeSkinningPalette, or GetRestPalette for the rest pose)
		// and must hold at least mesh->GetSkinJointCount() matrices; consecutive calls with the same
		// palette, transform and colour, such as a model's submeshes, share one upload.
		//
		// Skinned meshes draw after every static batch, so a translucent static mesh in front of one
		// hides it rather than blending over it.
		//
		// A lit material (null = the default) draws through a skinned twin the renderer keeps for
		// it. A custom material's shader needs a SkinData block (see Renderer3D_Lit.glsl) at a
		// binding from 2 to k_MaxSkinDataBinding that its textures and samplers leave free, or it is
		// drawn with the default material and a warning. A mesh without a skin, with too few joints
		// passed, or skinned to more than k_MaxSkinJoints joints goes through SubmitMesh and draws its
		// rest pose. No-op outside a Begin/EndScene pair.
		void SubmitSkinnedMesh(const Mesh* mesh, const glm::mat4& transform, std::span<const glm::mat4> joints, const glm::vec4& color, Material* material = nullptr);

		// Skinned instances a frame: Capabilities.MaxSkinnedInstances, between 1 and
		// k_MaxSkinnedInstancesLimit.
		uint32_t GetSkinnedInstanceBudget() const;

		Mesh* GetBoxMesh() const { return m_BoxMesh; }
		Mesh* GetSphereMesh() const { return m_SphereMesh; }

		// A material lit like the default one but with its own emissive, Roughness and Specular
		// (see MaterialParams) and an optional albedo texture in texture slot 0, whose colour
		// multiplies the mesh colour; transparency comes from the mesh colour's alpha alone. An
		// empty slot 0 draws white, and an empty sampler slot 0 uses the clamp sampler. Slot 0 is
		// the only one the lit shader has: with a texture or sampler in any other slot the material
		// is not drawn, and the renderer warns once. Shader and CullMode are set for you: lit
		// materials draw both faces, because front-face winding differs between the Vulkan and D3D
		// back-ends. Any Renderer3D can draw it; the caller owns it and must delete it before the
		// renderer that created it shuts down.
		Material* CreateLitMaterial(MaterialParams params) const;

		// The material meshes with no material of their own are drawn with, for changing their
		// shared emissive, Roughness or Specular.
		Material* GetDefaultMaterial() const { return m_Material; }

		// Per-scene render statistics: reset each BeginScene, complete after EndScene.
		struct Statistics
		{
			uint32_t DrawCalls = 0;       // one indexed draw per non-empty batch; a material may take several
			uint32_t SubmittedMeshes = 0; // meshes accepted into a batch this scene
			uint32_t DroppedMeshes = 0;   // meshes too large for an empty batch
			uint32_t VertexCount = 0;     // vertices batched this scene
			uint32_t IndexCount = 0;      // indices batched this scene
			uint32_t DirectionalLights = 0; // directional lights the scene was lit by, the default light included
			uint32_t LocalLights = 0;       // point and spot lights the scene was lit by
			uint32_t CulledLights = 0;      // point and spot lights whose range can't reach anything in view
			uint32_t DroppedLights = 0;     // directional lights past k_MaxDirectionalLights, point and spot lights past the budget
			uint32_t SkinnedDraws = 0;        // skinned meshes drawn, also counted in DrawCalls and SubmittedMeshes; not batched, so not in VertexCount/IndexCount
			uint32_t SkinnedInstances = 0;    // joint palettes uploaded; a model's submeshes share one
			uint32_t DroppedSkinnedDraws = 0; // skinned meshes of instances past MaxSkinnedInstances for the frame
			uint32_t SkinnedJoints = 0;       // joint matrices uploaded
		};

		const Statistics& GetStatistics() const { return m_Statistics; }
		const Renderer3DCapabilities& GetCapabilities() const { return m_Params.Capabilities; }

		// Point and spot lights a scene can draw: Capabilities.MaxLocalLights, capped at k_MaxLocalLights.
		uint32_t GetLocalLightBudget() const;

	private:
		Renderer3D(const Renderer3DParams& params) : m_Params(params) {}

		void BeginSceneInternal(const glm::mat4& viewProjection, const glm::vec4& cameraPosition);
		void ResolveSceneLights();

	private:
		Renderer3DParams m_Params;
		Statistics m_Statistics;

		struct Vertex
		{
			glm::vec3 Position;
			glm::vec3 Normal;
			glm::vec4 Color;
			glm::vec2 TexCoord;
		};

		struct DirectionalLightData
		{
			glm::vec4 Direction{ 0.0f };
			glm::vec4 Color{ 0.0f }; // rgb = colour × intensity
		};

		// A point light is a spot light whose cone factor is always 1: scale 0, offset 1.
		struct LocalLightData
		{
			glm::vec4 PositionRange{ 0.0f }; // xyz = world position, w = range
			glm::vec4 Color{ 0.0f };         // rgb = colour × intensity, w = cone scale
			glm::vec4 SpotDirection{ 0.0f }; // xyz = the way the cone points, w = cone offset
		};

		struct LocalLightCandidate
		{
			LocalLightData Data;
			float Brightness = 0.0f; // strongest colour channel × intensity
			float Score = 0.0f;
			float Nearness = 0.0f; // camera distance / range
		};

		// std140, mirrored by CameraData in Renderer3D_Lit.glsl. The first three members are a
		// frozen prefix that custom material shaders declare on their own: only ever append.
		struct CameraData
		{
			glm::mat4 ViewProjection{ 1.0f };
			glm::vec4 LightDirection{ 0.0f }; // the first directional light's direction, or the default light's
			glm::vec4 Ambient{ 0.0f };        // x = the scene's ambient as one value
			glm::vec4 CameraPosition{ 0.0f }; // w = 1: world position; w = 0: orthographic, xyz = towards the camera
			glm::vec4 AmbientColor{ 0.0f };   // rgb = colour × intensity
			glm::ivec4 LightCounts{ 0 };      // x = directional lights, y = point and spot lights
			DirectionalLightData DirectionalLights[k_MaxDirectionalLights];
			LocalLightData LocalLights[k_MaxLocalLights];
		};
		static_assert(offsetof(CameraData, LightDirection) == 64 && offsetof(CameraData, Ambient) == 80,
			"the frozen prefix custom materials declare must not move");
		static_assert(offsetof(CameraData, CameraPosition) == 96 && offsetof(CameraData, AmbientColor) == 112 &&
			offsetof(CameraData, LightCounts) == 128 && offsetof(CameraData, DirectionalLights) == 144 &&
			sizeof(DirectionalLightData) == 32 && offsetof(CameraData, LocalLights) == 144 + k_MaxDirectionalLights * 32 &&
			sizeof(LocalLightData) == 48 && sizeof(CameraData) == 144 + k_MaxDirectionalLights * 32 + k_MaxLocalLights * 48,
			"CameraData must match the std140 block in Renderer3D_Lit.glsl");
		CameraData m_CameraData = {};

		LocalLightCandidate* AddLocalLight();
		template<typename LightType>
		bool SubmitLocalLight(const LightType& light);
		static constexpr uint32_t k_MaxPendingLocalLights = 8192;

		std::vector<LocalLightCandidate> m_LocalLights;
		std::vector<uint32_t> m_VisibleLocalLights;
		bool m_SceneLightSubmitted = false;
		uint32_t m_DroppedLights = 0;
		bool m_DirectionalOverflowWarned = false;
		bool m_LocalOverflowWarned = false;
		bool m_PendingOverflowWarned = false;
		bool m_LitSlotsWarned = false;

		// std140, mirrored by MaterialData in Renderer3D_Lit.glsl: binding 1 of every lit material,
		// rebuilt from its MaterialParams each EndScene. Custom shaders bring their own layout.
		struct LitMaterialData
		{
			glm::vec4 EmissiveColor{ 0.0f }; // rgb = colour
			glm::vec4 Surface{ 0.0f };       // x = emissive strength, y = roughness, z = specular
		};

		void PrepareLitMaterial(Material* material) const;

		Shader* m_Shader = nullptr;
		Material* m_Material = nullptr; // built-in lit default material
		VertexLayout m_Layout;

		// Camera + lights, uploaded each EndScene and bound at binding 0 on every material the
		// renderer draws (Material::SetSceneUniformBuffer).
		GraphicsBuffer* m_SceneUniformBuffer = nullptr;

		// One batch: capped at the capabilities, drawn with one indexed draw.
		struct MeshChunk
		{
			std::vector<Vertex> Vertices;
			std::vector<uint32_t> Indices;
		};

		// Chunks past ChunksInUse are storage kept from a busier scene, so a steady frame
		// doesn't reallocate.
		struct MaterialBatch
		{
			std::vector<MeshChunk> Chunks;
			uint32_t ChunksInUse = 0;
			bool Enqueued = false; // already in m_DrawOrder for the scene in progress
			uint32_t IdleScenes = 0;
		};
		std::unordered_map<Material*, MaterialBatch> m_Batches;
		static constexpr uint32_t k_MaxIdleBatchScenes = 300;

		// Materials in the order they were first submitted to this scene. Draw order has to
		// come from here, not from the map: unordered_map iteration follows pointer hashing,
		// so the same scene would submit its materials in a different order between runs —
		// invisible for depth-tested opaques, but not for anything blended.
		std::vector<Material*> m_DrawOrder;

		// Pooled GPU buffers — one (vertex, index) pair per batch drawn in a frame, grown
		// on demand and reused. Each batch gets its own buffer, so no shared buffer is
		// re-uploaded mid-frame.
		std::vector<GraphicsBuffer*> m_BatchVertexBuffers;
		std::vector<GraphicsBuffer*> m_BatchIndexBuffers;

		Mesh* m_BoxMesh = nullptr;
		Mesh* m_SphereMesh = nullptr;

		bool m_SceneActive = false;
		bool m_MeshOverflowWarned = false;

		// std140, mirrored by SkinData in Renderer3D_Lit.glsl. Each draw uploads it only as far as
		// its mesh's last joint.
		struct SkinData
		{
			glm::mat4 Model{ 1.0f };
			glm::mat4 NormalMatrix{ 1.0f };
			glm::vec4 Color{ 1.0f };
			glm::mat4 Joints[k_MaxSkinJoints];
		};
		static_assert(offsetof(SkinData, NormalMatrix) == 64 && offsetof(SkinData, Color) == 128 &&
			offsetof(SkinData, Joints) == 144 && sizeof(SkinData) == 144 + k_MaxSkinJoints * 64,
			"SkinData must match the std140 block in Renderer3D_Lit.glsl");

		// One SkinData upload, shared by the submissions of one instance.
		struct SkinnedInstance
		{
			const glm::mat4* Source = nullptr;
			glm::mat4 Transform{ 1.0f };
			glm::vec4 Color{ 1.0f };
			uint32_t FirstJoint = 0;
			uint32_t JointCount = 0;
		};

		struct SkinnedSubmission
		{
			const Dingo::Mesh* Mesh = nullptr;
			Dingo::Material* Material = nullptr;
			uint32_t Instance = 0;
		};

		// A lit material's copy on the skinned shader, synced from it before every draw. Keyed by
		// Material::GetId, so a new material at a freed one's address gets a twin of its own; released
		// when idle, like the batches, since nothing reports a deleted material.
		struct SkinnedTwin
		{
			Material* Twin = nullptr;
			uint64_t SourceRevision = ~0ull;
			uint32_t IdleScenes = 0;
			bool Used = false;
		};

		void EnsureSkinningResources();
		Material* ResolveSkinnedMaterial(Material* material);
		Material* GetSkinnedTwin(Material* source);
		void DrawSkinnedSubmissions();

		// Made on the first skinned draw, so an app that never skins compiles no second lit program.
		Shader* m_SkinnedShader = nullptr;
		GraphicsBuffer* m_SkinBuffer = nullptr;
		VertexLayout m_SkinnedLayout;
		SkinData m_SkinData;
		// D3D11 drops a partial constant-buffer update on drivers without ConstantBufferPartialUpdate.
		bool m_FullSkinUploads = false;

		std::vector<SkinnedSubmission> m_SkinnedSubmissions;
		std::vector<SkinnedInstance> m_SkinnedInstances;
		std::vector<glm::mat4> m_SkinnedJoints; // every instance's palette, back to back
		std::unordered_map<uint64_t, SkinnedTwin> m_SkinnedTwins;

		uint64_t m_SkinnedFrameIndex = 0;
		uint32_t m_SkinnedInstancesThisFrame = 0;
		bool m_SkinnedBudgetWarned = false;
		bool m_SkinFallbackWarned = false;
		bool m_SkinMaterialWarned = false;
	};

}
