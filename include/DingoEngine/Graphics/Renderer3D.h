#pragma once
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/Graphics/Material.h"
#include "DingoEngine/Graphics/Mesh.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/Light.h"
#include "DingoEngine/Graphics/Particles.h"
#include "DingoEngine/Graphics/Pipeline.h"

#include "DingoEngine/Core/PerspectiveCamera.h"

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Dingo
{

	namespace Internal
	{
		class FullscreenShader;
		class ParticleRenderer;
		class TextureReadback;
	}

	// A light of the scene being built, for a shadow probe: Renderer3D::GetLastSubmittedLight.
	struct ShadowProbeLight
	{
		int32_t Index = -1; // among the scene's directional lights, or its point and spot lights; -1 = none
		bool Directional = false;

		bool IsValid() const { return Index >= 0; }
	};

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

		// Past the budget a light drops out at full strength. With a band above 0, a drawn light whose
		// priority comes within that fraction of the first dropped light's fades out as it nears it,
		// so a light crossing the budget's edge as the camera moves fades instead of popping (0.5 =
		// within 50 %). Priority is then one continuous value, brightness seen from the camera over
		// one plus the distance relative to the range, rather than brightness with ties broken by
		// distance. 0 (the default) keeps the hard cut and the old ranking. Casting lights hand their
		// shadow slots over through the same band. Renderer3D::SetLightBudgetFade changes it.
		float LightBudgetFade = 0.0f;

		// Point and spot lights with CastShadows that get a shadow, at most
		// Renderer3D::k_MaxShadowedLocalLights. The drawn casting lights take them in the budget's
		// order; the rest light unshadowed, with a warning (Statistics::UnshadowedLights).
		uint32_t MaxShadowedLocalLights = 8;

		// Skinned instances a frame, across every scene this renderer runs, at most
		// Renderer3D::k_MaxSkinnedInstancesLimit. An instance is a run of SubmitSkinnedMesh calls with
		// the same palette, transform and colour (a model's submeshes); it uploads its joints once,
		// to a volatile buffer that on Vulkan has room for this many writes a frame. Later instances
		// that frame are skipped whole, with a warning (Statistics::DroppedSkinnedDraws).
		uint32_t MaxSkinnedInstances = 64;

		// GPU particles alive at once across every emitter this renderer made, at most
		// Renderer3D::k_MaxParticlesLimit: the pool, 48 bytes a particle, made at the first EndScene with
		// a submission. Each emitter takes a ring of it the size of its effect's Capacity.
		uint32_t MaxParticles = 65536;

		// When true, a mesh too large for an empty batch, a light past the light budget or a
		// skinned instance past MaxSkinnedInstances trips an assert instead of the default
		// warn-once-and-drop. Asserts are compiled out in release, where it warns and drops
		// regardless.
		bool AssertOnOverflow = false;
	};

	// Cascaded shadow maps for the first directional light with CastShadows. Every shadow view of a
	// scene renders into one depth atlas, every batch once for all of them (instanced), before the
	// scene's lit draws sample it. A scene whose lights cast nothing renders exactly as without shadows.
	struct Renderer3DShadowSettings
	{
		// The atlas is AtlasSize x AtlasSize D32 (64 MB at 4096), made the first time a scene casts.
		// A power of two from 2048 to 8192.
		uint32_t AtlasSize = 4096;
		// Cascades for a perspective camera, 1 to 4; an orthographic camera gets one fitted to its view.
		uint32_t CascadeCount = 4;
		// Each cascade's square tile in the atlas, a power of two of at most AtlasSize / 2.
		uint32_t CascadeResolution = 1024;
		// How far along the view shadows reach; the last cascade fades out over its CascadeBlend.
		float MaxDistance = 60.0f;
		// How the view is split between cascades: 0 evenly, 1 logarithmically (detail near the camera).
		float SplitLambda = 0.75f;
		// The part of each cascade, at its far end, over which it fades into the next.
		float CascadeBlend = 0.1f;
		// Pushes the depth stored in the atlas away from the light: DepthBias in units of the smallest
		// depth step, SlopeBias times the triangle's slope toward the light (both baked into the shadow
		// pipeline). NormalBias moves the point a lit surface looks up along its normal, in the
		// cascade's texels. Together they keep a surface from shadowing itself (acne).
		int32_t DepthBias = 4;
		float SlopeBias = 2.0f;
		float NormalBias = 1.5f;
		// The tile of the two highest-ranked shadowed spot lights; the next four get half, the rest a
		// quarter. A point light's six faces each get half of what a spot light of its rank would.
		// A light keeps its size until its rank moves two places, so a still scene never changes. A
		// power of two from 128 to AtlasSize / 2.
		uint32_t LocalShadowResolution = 1024;
		// Tints the scene red, green, blue and yellow by cascade.
		bool DebugCascades = false;
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
		Renderer3DShadowSettings Shadows = {};
	};

	// A batched, forward-lit mesh renderer — the 3D sibling of Renderer2D.
	//
	// Between BeginScene()/EndScene() it groups submitted meshes BY MATERIAL,
	// transforming each into a per-material vertex/index batch on the CPU, then issues
	// one indexed draw per batch on EndScene() (each from its own pooled buffer).
	// Meshes with no explicit material use the built-in lit default material.
	// Depth testing is enabled (the swap-chain carries a depth attachment), so meshes
	// occlude correctly regardless of submission order. Meshes with a translucent material
	// (MaterialParams::Translucent) are the exception: they draw after every opaque one, static
	// and skinned, sorted far to near by their centres.
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

		// Capabilities.LightBudgetFade, clamped to 0..4.
		void SetLightBudgetFade(float band);

		// The light the latest SubmitLight call added to the scene being built, for AddShadowProbe;
		// invalid when that call ignored or dropped its light.
		ShadowProbeLight GetLastSubmittedLight() const { return m_LastSubmittedLight; }

		static constexpr uint32_t k_MaxShadowProbes = 256;

		// Shadow probes: how much of `light` reaches `point` past the shadows this scene draws, from 1
		// (lit) to 0 (in its shadow, ShadowStrength of the way), worked out on the GPU by the lit
		// shader's own shadow lookup, so the shadow a player sees is the one that hides them. Add a
		// probe before EndScene, as lights are (it clears with them), and read the answer by its key
		// with GetShadowProbeResult: the GPU answers one to three frames later, never stalling, and a key
		// keeps its latest answer until the next one arrives. A light drawn without a shadow (no
		// CastShadows, past the shadow slots, out of the budget) answers 1 at once, which is what is
		// drawn; so does a directional light for a point outside this scene's cascades (behind the
		// camera, off screen, past Shadows.MaxDistance), which cover only what the camera sees. A
		// local light's shadow doesn't depend on the camera. The point is taken as it is, with no surface normal to push it off a surface, so
		// probe a point in the air, such as a character's chest. At most k_MaxShadowProbes a scene;
		// past them AddShadowProbe returns false and warns once.
		bool AddShadowProbe(ShadowProbeLight light, const glm::vec3& point, uint64_t key);
		// The latest answer for key; empty until the first one arrives. Answers not refreshed for 600
		// frames are forgotten.
		std::optional<float> GetShadowProbeResult(uint64_t key) const;

		static constexpr uint32_t k_MaxParticlesLimit = 1u << 20;

		// GPU particles. An emitter is a ring of this renderer's pool, the size of the effect's capacity
		// (an emitter the pool has no room for draws nothing, with a warning); dropping its last
		// reference returns the ring. SubmitParticles between BeginScene and EndScene steps it by
		// deltaTime (simulate, then emit, on the GPU) and draws it after the scene's opaque and translucent meshes,
		// unlit and depth-tested without writing depth; submit an emitter once a frame, or the other
		// scenes with a deltaTime of 0. A submission keeps the emitter alive until EndScene. Through the post chain, AO is applied first so particles aren't
		// darkened, and particles with a SoftDistance fade against the scene's depth. See
		// docs/particles.md.
		std::shared_ptr<ParticleEmitter> CreateParticleEmitter(const ParticleEffect* effect);
		void SubmitParticles(ParticleEmitter& emitter, const glm::mat4& transform, float deltaTime);
		// Whether this renderer made the emitter: one made by another Renderer3D draws nothing here.
		bool OwnsParticleEmitter(const ParticleEmitter& emitter) const;
		// The pool, for tooling and tests: 48 bytes a particle (ParticleCommon.glsl); null until the
		// first emitter draws.
		GraphicsBuffer* GetParticlePool() const;
		uint32_t GetParticlePoolCapacity() const;
		uint32_t GetParticlePoolUsed() const;

		// Distance fog over this scene's lit draws (static and skinned; particles, custom shaders and
		// the 2D overlay aren't fogged), scene-scoped like the lights: set it before EndScene, which
		// clears it. It is no light, so a scene with fog and no light keeps the default light. Returns
		// false for a fog it ignores: a non-finite value, a negative Density, or Linear with End not
		// past Start. FogMode::None clears it, as ClearFog does; MaxOpacity is clamped to 0..1. An
		// orthographic camera's scene draws without fog.
		bool SetFog(const Fog& fog);
		void ClearFog();

		// Replaces the default light (Renderer3DParams::LightDirection/Ambient).
		void SetDirectionalLight(const glm::vec3& direction, float ambient);

		static constexpr uint32_t k_MaxShadowCascades = 4;
		static constexpr uint32_t k_MaxShadowedLocalLights = 16;
		// Atlas tiles a scene can render: the cascades and six faces for every shadowed local light.
		static constexpr uint32_t k_MaxShadowTiles = k_MaxShadowCascades + 6 * k_MaxShadowedLocalLights;

		// Values out of range are clamped. A new AtlasSize resizes the atlas in place.
		void SetShadowSettings(const Renderer3DShadowSettings& settings);
		const Renderer3DShadowSettings& GetShadowSettings() const { return m_Params.Shadows; }
		// The depth atlas, for viewing it; null until a scene cast a shadow.
		Framebuffer* GetShadowAtlas() const { return m_ShadowAtlas; }

		static constexpr uint32_t k_MaxDirectionalLights = 4;
		static constexpr uint32_t k_MaxLocalLights = 32;

		// Scenes a renderer can run in one frame. On Vulkan each EndScene writes the volatile scene
		// buffer, which has room for this many writes a frame; later scenes draw with stale lighting.
		static constexpr uint32_t k_MaxScenesPerFrame = 32;

		// Appends a mesh to the batch for the given material (null => the built-in
		// lit default), transformed into world space on the CPU. The vertex stream is
		// a_Position (0), a_Normal (1), a_Color (2, the color passed here) and a_TexCoord
		// (3, the mesh's UVs, for custom materials that sample a texture). No-op outside a
		// Begin/EndScene pair. Every material casts through the renderer's own depth-only pass, so a
		// custom vertex shader's displacement isn't in its shadow, and a translucent mesh casts a full
		// one.
		//
		// A translucent material's meshes draw in the translucent pass, after the opaque ones: each
		// mesh sorted far to near by the centre of its transformed vertices (along the view for an
		// orthographic camera; ties keep submission order), consecutive meshes of one material in one
		// draw. The sort is per mesh, not per triangle, so meshes that intersect, or a large one
		// wrapped around a small one, can blend in the wrong order. With ShadowsOnly it draws nothing
		// lit, so it batches like an opaque mesh.
		void SubmitMesh(const Mesh* mesh, const glm::mat4& transform, const glm::vec4& color, Material* material = nullptr, ShadowCasting shadows = ShadowCasting::On);

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
		// Opaque skinned meshes draw after every opaque static batch. With a translucent material a
		// skinned mesh joins the translucent pass, sorted by the centre of its rest bounds placed with
		// transform (an instance's translucent meshes share the first one's), so it blends with translucent
		// static meshes in depth order.
		//
		// A lit material (null = the default) draws through a skinned twin the renderer keeps for
		// it. A custom material's shader needs a SkinData block (see Renderer3D_Lit.glsl) at a
		// binding from 2 to k_MaxSkinDataBinding that its textures and samplers leave free, or it is
		// drawn with the default material and a warning. A mesh without a skin, with too few joints
		// passed, or skinned to more than k_MaxSkinJoints joints goes through SubmitMesh and draws its
		// rest pose. No-op outside a Begin/EndScene pair.
		// A shadowed scene draws a casting instance twice, into the shadow atlas and then lit, and
		// uploads its palette for each; an instance with opaque and translucent meshes is drawn lit in
		// both passes. That is why the skin buffer holds three writes per instance.
		void SubmitSkinnedMesh(const Mesh* mesh, const glm::mat4& transform, std::span<const glm::mat4> joints, const glm::vec4& color, Material* material = nullptr, ShadowCasting shadows = ShadowCasting::On);

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
		// materials draw both faces, so open meshes and mirrored entities still show. Any
		// Renderer3D can draw it; the caller owns it and must delete it before the renderer that
		// created it shuts down.
		Material* CreateLitMaterial(MaterialParams params) const;

		// The material meshes with no material of their own are drawn with, for changing their
		// shared emissive, Roughness or Specular.
		Material* GetDefaultMaterial() const { return m_Material; }

		// Per-scene render statistics: reset each BeginScene, complete after EndScene. A scene
		// begun while Renderer::IsFrameSkipped() draws nothing and leaves them as they were.
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
			uint32_t ShadowViews = 0;         // atlas tiles rendered (cascades and local light faces), 0 for a scene that casts nothing
			uint32_t ShadowCascades = 0;      // of which cascades
			uint32_t ShadowedLights = 0;      // point and spot lights drawn with a shadow
			uint32_t UnshadowedLights = 0;    // casting point and spot lights drawn without one: past MaxShadowedLocalLights, or no atlas room
			uint32_t FadedLights = 0;         // drawn point and spot lights the budget fade dimmed (LightBudgetFade)
			uint32_t ShadowProbes = 0;        // probes the GPU evaluates this scene; ones answered at once aren't counted
			uint32_t ParticleEmitters = 0;    // emitters simulated and drawn
			uint32_t ParticleSlots = 0;       // their rings' slots, every one simulated and drawn as an instance
			uint32_t ParticlesSpawned = 0;
			uint32_t DroppedParticleSpawns = 0; // spawns past an emitter's ring this step
			uint32_t ParticleDrawCalls = 0;   // one per run of emitters sharing a blend and sprite; not in DrawCalls
			uint32_t ShadowCasters = 0;       // meshes drawn into the atlas, skinned ones included
			uint32_t ShadowDrawCalls = 0;     // instanced atlas draws, one per casting batch and skinned mesh; not in DrawCalls
			uint32_t TranslucentMeshes = 0;   // static and skinned meshes drawn in the translucent pass, also counted in SubmittedMeshes
			uint32_t TranslucentDraws = 0;    // draws of the translucent pass, also counted in DrawCalls
			float ShadowCascadeEnds[k_MaxShadowCascades] = {}; // where each cascade ends along the view
			bool Fogged = false;              // the scene was drawn with fog: SetFog, and a perspective camera
		};

		const Statistics& GetStatistics() const { return m_Statistics; }
		const Renderer3DCapabilities& GetCapabilities() const { return m_Params.Capabilities; }

		// Point and spot lights a scene can draw: Capabilities.MaxLocalLights, capped at k_MaxLocalLights.
		uint32_t GetLocalLightBudget() const;

	private:
		explicit Renderer3D(const Renderer3DParams& params);

		void BeginSceneInternal(const glm::mat4& viewProjection, const glm::vec4& cameraPosition);
		void ResolveSceneLights();
		void ClearSceneLights();
		bool PrepareShadows();
		void EnsureShadowResources();
		void DrawShadowPass();
		void ResolveSkinnedBudget();

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
			float Priority = 0.0f; // Score / (1 + Nearness), the budget fade's continuous rank
			bool CastShadows = false;
			float ShadowStrength = 1.0f;
			uint32_t ShadowFaces = 1;    // 1: a spot light's single view; 6: a cube around the light
			float OuterConeAngle = 0.0f; // degrees, for a spot light's view
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
			glm::vec4 FogColor{ 0.0f };  // rgb = colour, a = max opacity
			glm::vec4 FogParams{ 0.0f }; // x = start, y = end, z = density, w = FogMode (0 = none)
		};
		static_assert(offsetof(CameraData, LightDirection) == 64 && offsetof(CameraData, Ambient) == 80,
			"the frozen prefix custom materials declare must not move");
		static_assert(offsetof(CameraData, CameraPosition) == 96 && offsetof(CameraData, AmbientColor) == 112 &&
			offsetof(CameraData, LightCounts) == 128 && offsetof(CameraData, DirectionalLights) == 144 &&
			sizeof(DirectionalLightData) == 32 && offsetof(CameraData, LocalLights) == 144 + k_MaxDirectionalLights * 32 &&
			sizeof(LocalLightData) == 48 && offsetof(CameraData, FogColor) == 144 + k_MaxDirectionalLights * 32 + k_MaxLocalLights * 48 &&
			offsetof(CameraData, FogParams) == offsetof(CameraData, FogColor) + 16 && sizeof(CameraData) == offsetof(CameraData, FogParams) + 16,
			"CameraData must match the std140 block in Renderer3D_Lit.glsl");
		CameraData m_CameraData = {};

		LocalLightCandidate* AddLocalLight();
		template<typename LightType>
		bool SubmitLocalLight(const LightType& light);
		static constexpr uint32_t k_MaxPendingLocalLights = 8192;

		std::vector<LocalLightCandidate> m_LocalLights;
		std::vector<uint32_t> m_VisibleLocalLights; // the first m_DrawnLocalLights are the drawn ones, slot by slot
		uint32_t m_DrawnLocalLights = 0;
		bool m_SceneLightSubmitted = false;
		uint32_t m_DroppedLights = 0;
		bool m_DirectionalOverflowWarned = false;
		bool m_LocalOverflowWarned = false;
		bool m_PendingOverflowWarned = false;
		bool m_LitSlotsWarned = false;
		bool m_UnshadowedWarned = false;
		bool m_AtlasFullWarned = false;
		bool m_NoClipDistanceWarned = false;
		bool m_FogWarned = false;

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
			bool Enqueued = false; // checked and, unless SkinnedOnly or Translucent, in m_DrawOrder for the scene in progress
			// Its shader skins (has a SkinData block), so its meshes draw with the default material instead.
			bool SkinnedOnly = false;
			// Its meshes go to the translucent pass, so the batch itself stays empty.
			bool Translucent = false;
			uint32_t IdleScenes = 0;
		};
		// A material's meshes batch apart by how they cast, so a pass can take or leave a whole batch.
		struct BatchKey
		{
			Dingo::Material* Material = nullptr;
			ShadowCasting Shadows = ShadowCasting::On;
			bool operator==(const BatchKey&) const = default;
		};
		struct BatchKeyHash
		{
			size_t operator()(const BatchKey& key) const
			{
				return std::hash<const void*>()(key.Material) ^ (static_cast<size_t>(key.Shadows) * 0x9e3779b97f4a7c15ull);
			}
		};
		std::unordered_map<BatchKey, MaterialBatch, BatchKeyHash> m_Batches;
		static constexpr uint32_t k_MaxIdleBatchScenes = 300;

		// Batches in the order they were first submitted to this scene. Draw order has to
		// come from here, not from the map: unordered_map iteration follows pointer hashing,
		// so the same scene would submit its materials in a different order between runs —
		// invisible for depth-tested opaques, but not for anything blended.
		std::vector<BatchKey> m_DrawOrder;

		// The scene's non-empty chunks, uploaded before any pass draws them: the shadow pass needs
		// them before the lit pass does.
		struct ChunkDraw
		{
			BatchKey Key;
			uint32_t Buffer = 0;   // into the pooled vertex/index buffers
			uint32_t IndexCount = 0;
			bool Translucent = false; // translucent casters, for the shadow pass only (no material)
		};
		std::vector<ChunkDraw> m_ChunkDraws;

		uint32_t UploadChunk(const MeshChunk& chunk, uint32_t& batchIndex);

		// A mesh of the translucent pass. A static one's vertices are transformed at submission into
		// m_TranslucentVertices, its indices counted from its first vertex.
		struct TranslucentSubmission
		{
			Dingo::Material* Material = nullptr;
			uint32_t FirstVertex = 0;
			uint32_t VertexCount = 0;
			uint32_t FirstIndex = 0;
			uint32_t IndexCount = 0;
			int32_t Skinned = -1; // into m_SkinnedSubmissions; -1 = static
			ShadowCasting Shadows = ShadowCasting::On;
			float SortKey = 0.0f; // larger is farther from the camera
		};
		// One draw of the translucent pass, far to near: a run of static meshes of one material that
		// lie back to back in one pooled buffer, or one skinned submission.
		struct TranslucentDraw
		{
			Dingo::Material* Material = nullptr;
			int32_t Skinned = -1;
			uint32_t Buffer = 0;
			uint32_t FirstIndex = 0;
			uint32_t IndexCount = 0;
			uint32_t Meshes = 0;
		};

		float TranslucentSortKey(const glm::vec3& point) const;
		// After the opaque uploads: sorts the scene's translucent meshes, packs them (and, for a
		// shadowed scene, the casting ones again in m_ChunkDraws) into pooled buffers.
		void PrepareTranslucentPass(uint32_t& batchIndex, bool shadows);
		void DrawTranslucentPass(Texture* shadowAtlas);

		std::vector<TranslucentSubmission> m_TranslucentSubmissions;
		std::vector<Vertex> m_TranslucentVertices;
		std::vector<uint32_t> m_TranslucentIndices;
		std::vector<MeshChunk> m_TranslucentChunks; // storage kept from busier scenes
		std::vector<TranslucentDraw> m_TranslucentDraws;
		std::vector<std::pair<Dingo::Material*, bool>> m_TranslucentMaterials; // checked this scene: drawable or not

		// Pooled GPU buffers — one (vertex, index) pair per batch drawn in a frame, grown
		// on demand and reused. Each batch gets its own buffer, so no shared buffer is
		// re-uploaded mid-frame.
		std::vector<GraphicsBuffer*> m_BatchVertexBuffers;
		std::vector<GraphicsBuffer*> m_BatchIndexBuffers;

		Mesh* m_BoxMesh = nullptr;
		Mesh* m_SphereMesh = nullptr;

		bool m_SceneActive = false;
		bool m_SceneSkipped = false; // begun in a Renderer::SkipFrame frame: submits nothing, and EndScene only clears the lights
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
			ShadowCasting Shadows = ShadowCasting::On;
			bool Translucent = false; // drawn lit by the translucent pass
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
		// Draws one lit; false when it was skipped (its instance dropped, ShadowsOnly, or an unusable lit material).
		bool DrawSkinnedSubmission(const SkinnedSubmission& submission, uint32_t& uploadedInstance, Texture* shadowAtlas);

		// Made on the first skinned draw, so an app that never skins compiles no second lit program.
		Shader* m_SkinnedShader = nullptr;
		GraphicsBuffer* m_SkinBuffer = nullptr;
		VertexLayout m_SkinnedLayout;
		VertexLayout m_SkinnedShadowLayout;
		SkinData m_SkinData;
		// D3D11 drops a partial constant-buffer update on drivers without ConstantBufferPartialUpdate.
		bool m_FullSkinUploads = false;

		std::vector<SkinnedSubmission> m_SkinnedSubmissions;
		std::vector<SkinnedInstance> m_SkinnedInstances;
		std::vector<glm::mat4> m_SkinnedJoints; // every instance's palette, back to back
		std::unordered_map<uint64_t, SkinnedTwin> m_SkinnedTwins;

		std::vector<uint8_t> m_SkinnedInstanceDropped; // per instance of the scene: past the frame's budget
		uint64_t m_SkinnedFrameIndex = 0;
		uint32_t m_SkinnedInstancesThisFrame = 0;
		bool m_SkinnedBudgetWarned = false;
		bool m_SkinFallbackWarned = false;
		bool m_SkinnedOnlyWarned = false;
		bool m_SkinMaterialWarned = false;

		void UploadSkinData(const SkinnedInstance& instance);
		void EnsureSkinBuffers(const Mesh* mesh);

		// ── Shadows ───────────────────────────────────────────────────────────

		// std140, mirrored by ShadowTile, LocalShadow and ShadowData in Shadows.glsl: bound by name to
		// every material whose shader declares it, uploaded every EndScene (the lit shader always binds it).
		struct ShadowTileData
		{
			glm::mat4 ViewProjection{ 1.0f };
			glm::vec4 AtlasRect{ 0.0f }; // xy = top-left in atlas UV, zw = size
			glm::vec4 Params{ 0.0f };    // x = a cascade's end depth along the view; y = a texel in world units, times the distance from the light when z = 1 (perspective)
		};
		struct LocalShadowData
		{
			glm::vec4 Record{ -1.0f, 0.0f, 0.0f, 0.0f }; // x = first tile (-1 = none), y = tiles (1 or 6), z = strength
			glm::vec4 Position{ 0.0f };                  // xyz = the light's position
		};
		struct ShadowData
		{
			glm::ivec4 ShadowCounts{ 0, -1, 0, 0 }; // x = cascades, y = the casting directional light, z = debug tint, w = shadowed local lights
			glm::vec4 ShadowOrigin{ 0.0f };
			glm::vec4 ShadowForward{ 0.0f };         // w = cascade blend fraction
			glm::vec4 ShadowParams{ 0.0f };          // x = strength, y = normal bias in texels, z = atlas texel in UV, w = max distance
			ShadowTileData Tiles[k_MaxShadowTiles];  // the cascades first
			LocalShadowData LocalShadows[k_MaxLocalLights]; // by local light slot
		};
		static_assert(sizeof(ShadowTileData) == 96 && sizeof(LocalShadowData) == 32 && offsetof(ShadowData, Tiles) == 64 &&
			offsetof(ShadowData, LocalShadows) == 64 + 96 * k_MaxShadowTiles && sizeof(ShadowData) == 64 + 96 * k_MaxShadowTiles + 32 * k_MaxLocalLights,
			"ShadowData must match the std140 block in Shadows.glsl");

		// std140, mirrored by ShadowViews in Renderer3D_Shadow.glsl: view i renders tile i.
		struct ShadowViewData
		{
			glm::mat4 ViewProjection{ 1.0f };
			glm::vec4 Tile{ 0.0f }; // xy = the tile's centre in atlas clip space, zw = its scale
		};
		struct ShadowViews
		{
			ShadowViewData Views[k_MaxShadowTiles];
		};
		static_assert(sizeof(ShadowViewData) == 80, "ShadowViews must match the std140 block in Renderer3D_Shadow.glsl");

		class ShadowAtlasAllocator;
		bool PrepareCascades(ShadowAtlasAllocator& allocator);
		void PrepareLocalShadows(ShadowAtlasAllocator& allocator);
		// Fills tile m_ShadowViewCount from a view-projection and the atlas square it renders into.
		void AddShadowTile(const glm::mat4& viewProjection, const glm::uvec2& corner, uint32_t size, const glm::vec4& params);

		ShadowData m_ShadowData;
		ShadowViews m_ShadowViews;
		uint32_t m_ShadowViewCount = 0;
		int32_t m_ShadowLight = -1; // the directional light (submission index) that casts this scene
		float m_ShadowStrength = 1.0f;
		bool m_SecondShadowLightWarned = false;

		std::unique_ptr<Internal::ParticleRenderer> m_Particles;

		// ── Shadow probes ─────────────────────────────────────────────────────

		struct ShadowProbe
		{
			ShadowProbeLight Light;
			glm::vec3 Point{ 0.0f };
			uint64_t Key = 0;
		};
		// std140, mirrored by ProbeData in Renderer3D_ShadowProbe.glsl.
		struct ShadowProbeData
		{
			glm::vec4 Probes[k_MaxShadowProbes]; // xyz = the point, w = -1: the shadowed directional light, else a local light slot
		};
		struct ShadowProbeAnswer
		{
			float Value = 1.0f;
			uint64_t Frame = 0;
		};
		struct ShadowProbeAnswers
		{
			std::unordered_map<uint64_t, ShadowProbeAnswer> ByKey;
		};

		// Before ClearSceneLights, while the scene's light slots are known: answers what it can at once
		// and keeps the rest for DrawShadowProbes.
		void ResolveShadowProbes();
		void DrawShadowProbes();

		ShadowProbeLight m_LastSubmittedLight;
		std::vector<ShadowProbe> m_ShadowProbes;
		std::vector<uint64_t> m_GpuProbeKeys;
		ShadowProbeData m_ShadowProbeData{};
		std::shared_ptr<ShadowProbeAnswers> m_ProbeAnswers = std::make_shared<ShadowProbeAnswers>();
		uint64_t m_ProbePruneFrame = 0;
		bool m_ProbeOverflowWarned = false;
		Internal::FullscreenShader* m_ProbeShader = nullptr;
		Material* m_ProbeMaterial = nullptr;
		GraphicsBuffer* m_ProbeBuffer = nullptr;
		Framebuffer* m_ProbeTarget = nullptr;
		// Reads in flight, a few frames each; a scene finding none free draws no probes and keeps the
		// latest answers.
		std::vector<std::shared_ptr<Internal::TextureReadback>> m_ProbeReadbacks;
		static constexpr size_t k_MaxProbeReadbacks = 16;

		// Each local light's last tile tier, by submission index, while the scene submits as many
		// local lights as the last one did: a light keeps its tile size until its rank moves two places.
		// One table per scene of the frame, in the order they begin.
		std::vector<std::vector<uint8_t>> m_LocalShadowTiers;
		uint64_t m_SceneFrame = ~0ull;
		uint32_t m_SceneOfFrame = 0;

		// Where the scene's casters are, so a cascade's depth range reaches back to every one of them.
		glm::vec3 m_CasterMin{ 0.0f };
		glm::vec3 m_CasterMax{ 0.0f };
		bool m_HasCasters = false;
		uint32_t m_StaticCasters = 0;

		GraphicsBuffer* m_ShadowDataBuffer = nullptr;
		GraphicsBuffer* m_ShadowViewsBuffer = nullptr;
		Framebuffer* m_ShadowAtlas = nullptr;
		Texture* m_PlaceholderShadowAtlas = nullptr; // bound until the first scene casts
		Sampler* m_ShadowSampler = nullptr;          // comparison, linear: hardware 2 x 2 PCF
		Shader* m_ShadowShader = nullptr;
		Shader* m_SkinnedShadowShader = nullptr;
		Material* m_ShadowMaterial = nullptr;
		Material* m_SkinnedShadowMaterial = nullptr;
	};

}
