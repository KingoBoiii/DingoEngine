#pragma once
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/Graphics/Material.h"
#include "DingoEngine/Graphics/PbrMaterial.h"
#include "DingoEngine/Graphics/Mesh.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/Frustum.h"
#include "DingoEngine/Graphics/Environment.h"
#include "DingoEngine/Graphics/Light.h"
#include "DingoEngine/Graphics/Particles.h"
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/StaticBatch.h"

#include "DingoEngine/Core/PerspectiveCamera.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
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
		struct Renderer3DAccess;
		class ShadowAtlasAllocator;
		class SkinnedVisibility;
		class TextureReadback;
		struct StaticBatchShared;
		struct StaticBatchData;
		struct StaticCell;

		inline constexpr uint32_t k_MaxSkinDataInstances = 256;
	}

	/// A light of the scene being built, for a shadow probe: Renderer3D::GetLastSubmittedLight.
	struct ShadowProbeLight
	{
		/// Among the scene's directional lights, or its point and spot lights; -1 = none.
		int32_t Index = -1;
		/// True for a directional light, false for a point or spot light.
		bool Directional = false;

		/// True when Index names a light.
		bool IsValid() const { return Index >= 0; }
	};

	/// The resolution of the Raymarch FogVolume pass, relative to the render target.
	enum class RaymarchResolution
	{
		Half,    ///< Half the width and height.
		Quarter  ///< A quarter of the width and height.
	};

	/// Budgets and switches of a renderer, set in Renderer3DParams::Capabilities at Create.
	/// Afterwards only Instancing (SetInstancingEnabled), LightBudgetFade (SetLightBudgetFade) and the
	/// volumetric switch, grid and raymarch resolution (SetVolumetricsEnabled, SetVolumetricGrid,
	/// SetVolumeRaymarchResolution) can change.
	struct Renderer3DCapabilities
	{
		/// The most one indexed draw takes. A box is 24 verts / 36 indices and a default sphere
		/// ~289 verts / 1536 indices. A material whose meshes in view outgrow one draw spills into
		/// another; only a single mesh larger than one draw is dropped, with a warning. Every
		/// draw of a scene comes from one upload arena that grows to what the busiest frame needs.
		///@{
		uint32_t MaxVertices = 65536;
		uint32_t MaxIndices = 98304;
		///@}

		/// Point and spot lights drawn per scene, at most Renderer3D::k_MaxLocalLights. Lights
		/// whose range can't reach anything in view are skipped first. Past the budget the
		/// brightest as seen from the camera are kept and the rest dropped with a warning; ties go
		/// to the light the camera is nearer to relative to its range, then to the
		/// earlier-submitted one, so a still scene picks the same lights every frame.
		uint32_t MaxLocalLights = 32;

		/// Past the budget a light drops out at full strength. With a band above 0, a drawn light
		/// whose priority comes within that fraction of the first dropped light's fades out as it
		/// nears it, so a light crossing the budget's edge as the camera moves fades instead of
		/// popping (0.5 = within 50 %). Priority is then one continuous value, brightness seen from
		/// the camera over one plus the distance relative to the range, rather than brightness with
		/// ties broken by distance. 0 (the default) keeps the hard cut and the old ranking. Casting
		/// lights hand their shadow slots over through the same band.
		/// Renderer3D::SetLightBudgetFade changes it.
		float LightBudgetFade = 0.0f;

		/// Point and spot lights with CastShadows that get a shadow, at most
		/// Renderer3D::k_MaxShadowedLocalLights. The drawn casting lights take them in the budget's
		/// order; the rest light unshadowed, with a warning (Statistics::UnshadowedLights).
		uint32_t MaxShadowedLocalLights = 8;

		/// Skinned instances a frame, across every scene this renderer runs, at most
		/// Renderer3D::k_MaxSkinnedInstancesLimit. An instance is a run of SubmitSkinnedMesh calls
		/// with the same palette, transform and colour (a model's submeshes); its joints go once
		/// into the scene's skin arena, one storage buffer uploaded once a scene, and each of its
		/// draws pushes the 16 bytes that find them as a push constant. Later instances that frame
		/// are skipped whole, with a warning (Statistics::DroppedSkinnedDraws). A custom shader with
		/// a SkinData block takes a write of the whole palette a draw and has at most
		/// k_MaxSkinDataInstances of them.
		uint32_t MaxSkinnedInstances = 1024;

		/// GPU particles alive at once across every emitter this renderer made, at most
		/// Renderer3D::k_MaxParticlesLimit: the pool, 48 bytes a particle, made at the first
		/// EndScene with a submission. Each emitter takes a ring of it the size of its effect's
		/// Capacity.
		uint32_t MaxParticles = 65536;

		/// Copies of one mesh with one material and shadow setting submitted to a scene (culled or
		/// not, so the camera never changes the choice): at least InstanceThreshold of them, or
		/// more vertices together than one draw takes (MaxVertices), are drawn as one instanced
		/// draw from the mesh's own buffers, placed on the GPU, instead of being transformed into a
		/// batch on the CPU. Off (Renderer3D::SetInstancingEnabled at runtime) every static mesh is
		/// batched as before, except those of a material with MaterialParams::Instancing. Either
		/// way the picture is the same, within the rounding of the two transforms.
		///@{
		bool Instancing = true;
		uint32_t InstanceThreshold = 8;
		///@}

		/// When true, a mesh too large for an empty batch, a light past the light budget, a
		/// skinned instance past MaxSkinnedInstances or instanced draws booked past
		/// Renderer3D::k_MaxInstancedDrawsPerFrame trip an assert instead of the default
		/// warn-once-and-drop. Asserts are compiled out in release, where it warns and drops
		/// regardless.
		bool AssertOnOverflow = false;

		/// Allows froxel volumetric fog (VolumetricFog). It uses no GPU memory and costs nothing until
		/// first used: the first scene with a volumetric fog set allocates the volume. Once used, the
		/// volume stays allocated while the renderer lives, and the dispatches stop when this is off.
		/// Renderer3D::SetVolumetricsEnabled changes it.
		bool Volumetrics = false;
		/// The volume's froxel grid: x and y across the view (clamped to 8..256) and z slices in
		/// depth (8..128). Memory is about 8 bytes per froxel for each of three textures.
		/// Renderer3D::SetVolumetricGrid changes it.
		glm::uvec3 VolumetricGrid{ 160, 90, 64 };
		/// FogVolumes a scene draws, at most Renderer3D::k_MaxFogVolumes, at most
		/// Renderer3D::k_MaxRaymarchVolumes of them in Raymarch mode. Volumes outside the view are
		/// skipped first; past the budget the ones farthest from the camera are dropped with a
		/// warning (Renderer3D::Statistics::FogVolumesDropped).
		uint32_t MaxFogVolumes = 8;
		/// Resolution of the Raymarch FogVolume pass; Renderer3D::SetVolumeRaymarchResolution
		/// changes it.
		RaymarchResolution VolumeRaymarchResolution = RaymarchResolution::Half;
	};

	/// Cascaded shadow maps for the first directional light with CastShadows. Every shadow view of
	/// a scene renders into one depth atlas, every batch once for all of them (instanced), before
	/// the scene's lit draws sample it. A scene whose lights cast nothing renders exactly as
	/// without shadows.
	struct Renderer3DShadowSettings
	{
		/// The atlas is AtlasSize x AtlasSize D32 (64 MB at 4096), made the first time a scene
		/// casts. A power of two from 2048 to 8192.
		uint32_t AtlasSize = 4096;
		/// Cascades for a perspective camera, 1 to 4; an orthographic camera gets one fitted to its
		/// view.
		uint32_t CascadeCount = 4;
		/// Each cascade's square tile in the atlas, a power of two of at most AtlasSize / 2.
		uint32_t CascadeResolution = 1024;
		/// How far along the view shadows reach; the last cascade fades out over its CascadeBlend.
		float MaxDistance = 60.0f;
		/// How the view is split between cascades: 0 evenly, 1 logarithmically (detail near the
		/// camera).
		float SplitLambda = 0.75f;
		/// The part of each cascade, at its far end, over which it fades into the next.
		float CascadeBlend = 0.1f;
		/// Pushes the depth stored in the atlas away from the light: DepthBias in units of the
		/// smallest depth step, SlopeBias times the triangle's slope toward the light (both baked
		/// into the shadow pipeline). NormalBias moves the point a lit surface looks up along its
		/// normal, in the cascade's texels. Together they keep a surface from shadowing itself
		/// (acne).
		///@{
		int32_t DepthBias = 4;
		float SlopeBias = 2.0f;
		float NormalBias = 1.5f;
		///@}
		/// The tile of the two highest-ranked shadowed spot lights; the next four get half, the
		/// rest a quarter. A point light's six faces each get half of what a spot light of its rank
		/// would. A light keeps its size until its rank moves two places, so a still scene never
		/// changes. A power of two from 128 to AtlasSize / 2.
		uint32_t LocalShadowResolution = 1024;
		/// Tints the scene red, green, blue and yellow by cascade.
		bool DebugCascades = false;
	};

	/// Settings for Renderer3D::Create.
	struct Renderer3DParams
	{
		/// The default light: what a scene that submits no light and no ambient of its own is lit
		/// by, so code written before scene lighting existed keeps its look. Direction is the way
		/// the light travels; Ambient lifts every face, and lit faces get the remaining
		/// (1 - Ambient) scaled by how squarely they face the light.
		///@{
		glm::vec3 LightDirection = { -0.4f, -1.0f, -0.35f };
		float Ambient = 0.35f;
		///@}

		/// Budgets and switches; see Renderer3DCapabilities.
		Renderer3DCapabilities Capabilities = {};
		/// Initial shadow settings; SetShadowSettings changes them later.
		Renderer3DShadowSettings Shadows = {};
	};

	/// A batched, forward-lit mesh renderer - the 3D sibling of Renderer2D.
	///
	/// Between BeginScene()/EndScene() it records the submitted meshes. EndScene() culls them
	/// against the camera's view and each shadow view (Frustum.h), transforms what is left on the
	/// CPU into one vertex/index batch per material, uploads the scene in one go, and issues one
	/// indexed draw per batch. A mesh repeated often enough is drawn instanced instead, all its
	/// copies in one draw placed on the GPU (Renderer3DCapabilities::Instancing). Meshes with no
	/// explicit material use the built-in lit default material. Depth testing is enabled (the
	/// swap-chain carries a depth attachment), so meshes occlude correctly regardless of submission
	/// order. Meshes with a translucent material (MaterialParams::Translucent) are the exception:
	/// they draw after every opaque one, static and skinned, sorted far to near by their centres.
	///
	/// Camera + lights live in a shared "scene" uniform buffer bound at binding 0 on every material
	/// (Material::SetSceneUniformBuffer); a custom material's own uniforms bind at 1 and its
	/// textures at 2+. The SceneRenderer drives this for Transform3D + MeshRenderer entities (via
	/// Scene::SubmitLights and Scene::RenderEntities3D).
	///
	/// Note: each batch is capped at the configured capacity. A material that outgrows one spills
	/// into another batch - one more draw call - so raise Capabilities to cut draw calls in very
	/// dense scenes.
	class Renderer3D
	{
	public:
		/// Makes a renderer; the caller owns it and deletes it. Needs the graphics device
		/// (Renderer) up.
		[[nodiscard]] static Renderer3D* Create(const Renderer3DParams& params = {});

	public:
		/// Not constructible, copyable or movable: make one with Create and delete it when done.
		///@{
		Renderer3D() = delete;
		Renderer3D(const Renderer3D&) = delete;
		Renderer3D& operator=(const Renderer3D&) = delete;
		Renderer3D(Renderer3D&&) = delete;
		Renderer3D& operator=(Renderer3D&&) = delete;
		///@}
		/// Calls Shutdown.
		~Renderer3D();

	public:
		/// Frees the renderer's GPU objects early; the destructor does it anyway. A lit material it
		/// made must be deleted before this.
		void Shutdown();

		/// Begins accumulating a batch for the given view. Does not clear - call Clear() first if
		/// you want a fresh frame (it clears colour *and* depth). The matrix overload works out the
		/// camera position from the matrix, and treats a matrix without perspective as an
		/// orthographic camera.
		///@{
		void BeginScene(const PerspectiveCamera& camera);
		void BeginScene(const glm::mat4& viewProjection);
		///@}

		/// Uploads and draws the accumulated batch. Culls, uploads and draws what was submitted
		/// (shadow views first), then clears the scene's lights, fog and shadow probes. Does
		/// nothing without a BeginScene, or in a frame that renders nothing.
		void EndScene();

		/// Clears the current render target's colour and depth.
		void Clear(const glm::vec4& clearColor);

		/// Scene lighting is rebuilt for every scene: lights and ambient submitted since the last
		/// EndScene - before or after BeginScene - light the next EndScene, which then clears them.
		/// A scene that submits no light and no ambient is lit by the default light instead. Up to
		/// k_MaxDirectionalLights directional lights count; further ones are dropped with a
		/// warning. Point and spot lights share the MaxLocalLights budget (see
		/// Renderer3DCapabilities). SetAmbientLight replaces the scene's ambient, which is black
		/// otherwise. SubmitLight returns false for a light it ignores (non-finite, or a point or
		/// spot light without positive intensity and range) or drops (a fifth directional light, or
		/// one past the pending-light cap).
		///@{
		bool SubmitLight(const DirectionalLight& light);
		bool SubmitLight(const PointLight& light);
		bool SubmitLight(const SpotLight& light);
		///@}
		/// Sets the scene's ambient light to color (linear RGB) times intensity; ignored when not
		/// finite. Cleared at EndScene.
		void SetAmbientLight(const glm::vec3& color, float intensity);

		/// Capabilities.LightBudgetFade, clamped to 0..4.
		void SetLightBudgetFade(float band);

		/// The light the latest SubmitLight call added to the scene being built, for
		/// AddShadowProbe; invalid when that call ignored or dropped its light.
		ShadowProbeLight GetLastSubmittedLight() const { return m_LastSubmittedLight; }

		/// Shadow probes a scene can hold.
		static constexpr uint32_t k_MaxShadowProbes = 256;

		/// Shadow probes: how much of `light` reaches `point` past the shadows this scene draws,
		/// from 1 (lit) to 0 (in its shadow, ShadowStrength of the way), worked out on the GPU by
		/// the lit shader's own shadow lookup, so the shadow a player sees is the one that hides
		/// them. Add a probe before EndScene, as lights are (it clears with them), and read the
		/// answer by its key with GetShadowProbeResult: the GPU answers one to three frames later,
		/// never stalling, and a key keeps its latest answer until the next one arrives. A light
		/// drawn without a shadow (no CastShadows, past the shadow slots, out of the budget)
		/// answers 1 at once, which is what is drawn; so does a directional light for a point
		/// outside this scene's cascades (behind the camera, off screen, past Shadows.MaxDistance),
		/// which cover only what the camera sees. A local light's shadow doesn't depend on the
		/// camera. The point is taken as it is, with no surface normal to push it off a surface, so
		/// probe a point in the air, such as a character's chest. At most k_MaxShadowProbes a
		/// scene; past them AddShadowProbe returns false and warns once. `clearance` moves the
		/// point that far towards the light first (at most to the light), so a caster within it
		/// doesn't count: a point inside a character's body, probed while the body casts.
		bool AddShadowProbe(ShadowProbeLight light, const glm::vec3& point, uint64_t key, float clearance = 0.0f);
		/// The latest answer for key; empty until the first one arrives. Answers not refreshed for
		/// 600 frames are forgotten.
		std::optional<float> GetShadowProbeResult(uint64_t key) const;

		/// Upper limit of Renderer3DCapabilities::MaxParticles.
		static constexpr uint32_t k_MaxParticlesLimit = 1u << 20;

		/// GPU particles. An emitter is a ring of this renderer's pool, the size of the effect's
		/// capacity (an emitter the pool has no room for draws nothing, with a warning); dropping
		/// its last reference returns the ring. SubmitParticles between BeginScene and EndScene
		/// steps it by deltaTime (simulate, then emit, on the GPU) and draws it after the scene's
		/// opaque and translucent meshes, unlit and depth-tested without writing depth; submit an
		/// emitter once a frame, or the other scenes with a deltaTime of 0. A submission keeps the
		/// emitter alive until EndScene. Through the post chain, AO is applied first so particles
		/// aren't darkened, and particles with a SoftDistance fade against the scene's depth. See
		/// docs/particles.md.
		[[nodiscard]] std::shared_ptr<ParticleEmitter> CreateParticleEmitter(const ParticleEffect* effect);
		/// Steps the emitter by deltaTime seconds and draws it this scene, at transform (the
		/// emitter's local space to world). Does nothing outside a Begin/EndScene pair.
		void SubmitParticles(ParticleEmitter& emitter, const glm::mat4& transform, float deltaTime);
		/// Whether this renderer made the emitter: one made by another Renderer3D draws nothing
		/// here.
		bool OwnsParticleEmitter(const ParticleEmitter& emitter) const;
		/// The pool, for tooling and tests: 48 bytes a particle (ParticleCommon.glsl); null until
		/// the first emitter draws.
		GraphicsBuffer* GetParticlePool() const;
		/// Particles the pool holds: Capabilities.MaxParticles clamped to 64..k_MaxParticlesLimit.
		uint32_t GetParticlePoolCapacity() const;
		/// Particles of the pool taken by the live emitters' rings.
		uint32_t GetParticlePoolUsed() const;

		/// Distance fog over this scene's lit draws (static and skinned; particles, custom shaders,
		/// the skybox and the 2D overlay aren't fogged: with a skybox, match Fog.Color to its horizon), scene-scoped like the lights: set it before EndScene,
		/// which clears it. It is no light, so a scene with fog and no light keeps the default
		/// light. Returns false for a fog it ignores: a non-finite value, a negative Density, or
		/// Linear with End not past Start. FogMode::None clears it, as ClearFog does; MaxOpacity is
		/// clamped to 0..1. An orthographic camera's scene draws without fog.
		bool SetFog(const Fog& fog);
		/// Removes the scene's fog, as SetFog with FogMode::None does.
		void ClearFog();

		/// Froxel volumetric fog for this scene, scene-scoped like SetFog: set it before EndScene,
		/// which clears it. It takes effect only with Renderer3DCapabilities::Volumetrics on and a
		/// perspective camera, and then replaces the scene's distance fog. Returns false, warning
		/// once, for settings it ignores: a non-finite value, a negative Density, HeightFalloff or
		/// intensity, MaxInScatter, a NearDistance of 0 or less, or a MaxDistance not past NearDistance.
		/// A non-finite or negative ScatterTint is rejected the same way. Anisotropy is clamped to
		/// -0.95..0.95, Albedo to 0..1 and OutsideShadowLight to 0..1.
		bool SetVolumetricFog(const VolumetricFog& fog);
		/// Removes the scene's volumetric fog.
		void ClearVolumetricFog();
		/// Adds a local volume of fog (FogVolume) to this scene, scene-scoped like SetVolumetricFog:
		/// submit it before EndScene, which clears them all. `transform` is the volume's world
		/// transform: its translation places the centre, its rotation and scale turn and size
		/// FogVolume::HalfExtents. It takes effect only with Renderer3DCapabilities::Volumetrics on
		/// and a perspective camera. Returns false, warning once, for a volume it ignores: a
		/// non-finite value, a negative Density, intensity, EdgeFade, noise value or MaxInScatter, a
		/// HalfExtents or NoiseScale of 0 or less, an unknown Shape or Mode, or a transform that is
		/// not finite, not affine or squashes an axis flat. Anisotropy, Albedo, OutsideShadowLight,
		/// the fade keeps and RaymarchSteps are clamped.
		bool SubmitFogVolume(const FogVolume& volume, const glm::mat4& transform);
		/// The clock the FogVolume noise drifts by, in seconds, kept until set again. The ECS route
		/// (Scene::SubmitLights) passes the scene's own clock, which advances with Scene::OnUpdate.
		/// While a noisy FogVolume with a non-zero NoiseVelocity is kept, a jump backwards or of
		/// more than half a second drops the volumetric history, as ResetVolumetricHistory does;
		/// otherwise the clock is not looked at.
		void SetFogVolumeTime(double seconds) { m_FogVolumeTime = seconds; }
		/// The clock set by SetFogVolumeTime.
		double GetFogVolumeTime() const { return m_FogVolumeTime; }
		/// Chooses a debug picture of the volumetric fog; see VolumetricDebugView. Persistent.
		void SetVolumetricDebugView(VolumetricDebugView view) { m_VolumetricDebugView = view; }
		/// The view set by SetVolumetricDebugView.
		VolumetricDebugView GetVolumetricDebugView() const { return m_VolumetricDebugView; }
		/// Drops the volumetric fog's temporal history, so the next scene that uses it draws a frame
		/// with no blend against the past. Call it on a camera cut or a teleport, where the previous
		/// frame's air no longer matches the view and would ghost for a few frames. A grid change
		/// and a skipped frame do the same by themselves.
		void ResetVolumetricHistory() { InvalidateVolumetricHistory(); }

		/// Draws environment.Environment as the sky behind this scene's opaque meshes, at the far plane
		/// and under translucents and particles, scene-scoped like SetFog: the last call before EndScene
		/// wins and EndScene clears it. The first scene that uses an Environment bakes it (Environment::Prepare).
		/// By default (Lighting = true) it also replaces AmbientColor for PBR materials with
		/// image-based lighting from the same environment. Returns false, rejecting the whole
		/// environment and its sky, for an Intensity or LightingIntensity that is negative or not
		/// finite, or a Rotation that is not finite, leaving the scene as it was. The sky is skipped for an orthographic camera and while
		/// the Environment is not ready; a null Environment clears it.
		bool SetEnvironment(const SceneEnvironment& environment);
		/// Removes the scene's environment.
		void ClearEnvironment();

		/// Replaces the default light (Renderer3DParams::LightDirection/Ambient).
		[[deprecated("submit the light each scene with SubmitLight and SetAmbientLight, or set Renderer3DParams::LightDirection/Ambient; removed in 2.0")]]
		void SetDirectionalLight(const glm::vec3& direction, float ambient);

		/// Upper limit of Renderer3DShadowSettings::CascadeCount.
		static constexpr uint32_t k_MaxShadowCascades = 4;
		/// Upper limit of Renderer3DCapabilities::MaxShadowedLocalLights.
		static constexpr uint32_t k_MaxShadowedLocalLights = 16;
		/// Atlas tiles a scene can render: the cascades and six faces for every shadowed local
		/// light.
		static constexpr uint32_t k_MaxShadowTiles = k_MaxShadowCascades + 6 * k_MaxShadowedLocalLights;

		/// Values out of range are clamped. A new AtlasSize resizes the atlas in place.
		void SetShadowSettings(const Renderer3DShadowSettings& settings);
		/// The settings as clamped by SetShadowSettings.
		const Renderer3DShadowSettings& GetShadowSettings() const { return m_Params.Shadows; }
		/// The depth atlas, for viewing it; null until a scene cast a shadow.
		Framebuffer* GetShadowAtlas() const { return m_ShadowAtlas; }

		/// Directional lights a scene can use; further ones are dropped.
		static constexpr uint32_t k_MaxDirectionalLights = 4;
		/// Upper limit of Renderer3DCapabilities::MaxLocalLights.
		static constexpr uint32_t k_MaxLocalLights = 32;
		/// Upper limit of Renderer3DCapabilities::MaxFogVolumes.
		static constexpr uint32_t k_MaxFogVolumes = 16;
		/// FogVolumes in Raymarch mode a scene draws, inside MaxFogVolumes.
		static constexpr uint32_t k_MaxRaymarchVolumes = 4;

		/// Scenes a renderer can run in one frame. On Vulkan each EndScene writes the volatile
		/// scene buffer, which has room for this many writes a frame; later scenes draw with stale
		/// lighting.
		static constexpr uint32_t k_MaxScenesPerFrame = 32;

		/// Adds a mesh to the scene's batch for the given material (null => the built-in lit
		/// default); EndScene culls it and transforms what is drawn into world space on the CPU,
		/// or, for a mesh with enough copies of one material and shadow setting in the scene,
		/// counted before culling (Renderer3DCapabilities::InstanceThreshold), draws its copies
		/// instanced from the mesh's own buffers, so the mesh must stay alive and unchanged until
		/// then. It is culled when its bounds (Mesh::GetBoundsMin/Max placed by
		/// transform) lie outside the camera's view, and, for a perspective camera, when
		/// cullDistance is above 0 and the camera is farther than that from its bounds; a culled
		/// caster still casts into any shadow view its bounds reach, unless it is past its
		/// cullDistance. The near plane never culls (D3D clamps depth there instead of clipping),
		/// and the far plane only meshes whose material depth-tests with Less, without depth bias,
		/// into a target with depth, which can't draw past it on any backend. A non-affine
		/// transform is never culled, nor, by the camera, a mesh whose material isn't
		/// FrustumCulled. The vertex stream is a_Position (0), a_Normal (1), a_Color (2, the color
		/// passed here) and a_TexCoord (3, the mesh's UVs, for custom materials that sample a
		/// texture); a MaterialParams::Instancing material reads the mesh's own vertices and its
		/// instance record instead. No-op outside a Begin/EndScene pair, and for a mesh with no
		/// vertices or no indices (not counted in SubmittedMeshes). Every material casts
		/// through the renderer's own depth-only pass, so a custom vertex shader's displacement
		/// isn't in its shadow, and a translucent mesh casts a full one. shadowGroups is a bit
		/// mask: a casting light takes the mesh into its shadows only when it shares a bit with the
		/// light's ShadowCasterGroups.
		///
		/// A translucent material's meshes draw in the translucent pass, after the opaque ones:
		/// each mesh sorted far to near by the centre of its transformed vertices (along the view
		/// for an orthographic camera; ties keep submission order), consecutive meshes of one
		/// material in one draw. The sort is per mesh, not per triangle, so meshes that intersect,
		/// or a large one wrapped around a small one, can blend in the wrong order. A two-sided
		/// material's mesh (CullMode::None: every lit material) puts its triangles facing away from
		/// the camera first, so its far side never blends over its near side. With ShadowsOnly it
		/// draws nothing lit, so it batches like an opaque mesh.
		///
		/// A surface, when given, replaces the lit material's emissive, roughness and specular for
		/// this mesh alone (copied at the call), so meshes that differ only in those share one
		/// material's batch. A custom material's mesh ignores it.
		void SubmitMesh(const Mesh* mesh, const glm::mat4& transform, const glm::vec4& color, Material* material = nullptr, ShadowCasting shadows = ShadowCasting::On, uint32_t shadowGroups = 1, float cullDistance = 0.0f, const SurfaceOverride* surface = nullptr);

		/// Static geometry (StaticBatch.h): meshes baked into world space once, in cells kept on
		/// the GPU. A batch belongs to the renderer that made it and draws nothing on another
		/// (warned once); it may outlive the renderer, drawing nothing. SubmitStaticBatch between
		/// BeginScene and EndScene bakes what changed since the last submission, then culls each
		/// cell's draws against the camera and the shadow views as SubmitMesh culls a mesh, and
		/// keeps the batch alive until EndScene. A cell draws before the CPU batches of the same
		/// material, its meshes by cell, then by material and shadow setting, then by StaticMeshId
		/// (ids are reused, so not necessarily the order of Add), and its materials join the
		/// scene's draw order at the submission, so two meshes meeting at exactly equal depth can
		/// resolve the other way than through SubmitMesh. Call it before Shutdown; the batch it
		/// returns is never null.
		[[nodiscard]] std::shared_ptr<StaticBatch> CreateStaticBatch(const StaticBatchParams& params = {});
		/// Bakes what changed in the batch and draws it this scene; does nothing outside a
		/// Begin/EndScene pair, or for a batch another renderer made.
		void SubmitStaticBatch(StaticBatch& batch);
		/// Whether this renderer made the batch.
		bool OwnsStaticBatch(const StaticBatch& batch) const;

		/// Convenience primitives drawn with the renderer's built-in unit meshes
		/// (a 1x1x1 box centred on the origin, and a unit-diameter sphere).
		///@{
		void DrawBox(const glm::mat4& transform, const glm::vec4& color);
		void DrawSphere(const glm::mat4& transform, const glm::vec4& color);
		///@}

		/// The most joints one skinned draw can use: the length of SkinData's palette.
		static constexpr uint32_t k_MaxSkinJoints = 128;
		/// The highest binding a custom shader may give its deprecated SkinData block; D3D11 has 14
		/// constant buffer slots.
		static constexpr uint32_t k_MaxSkinUniformBinding = 13;
		/// Old name of k_MaxSkinUniformBinding.
		[[deprecated("use k_MaxSkinUniformBinding; removed in 2.0")]]
		static constexpr uint32_t k_MaxSkinDataBinding = k_MaxSkinUniformBinding;
		/// Upper limit of Renderer3DCapabilities::MaxSkinnedInstances. Each instance of the budget
		/// holds its palette in a stretch of the skin arena.
		static constexpr uint32_t k_MaxSkinnedInstancesLimit = 4096;
		/// Instances a frame drawn with a custom material whose shader has the deprecated SkinData
		/// block. On Vulkan each holds eight SkinData versions in host-visible memory (two writes a
		/// frame, four frames in flight), each about 8 KiB padded to the device's uniform-buffer
		/// alignment: about 66 KiB for each of the first 256 instances of the skinned budget
		/// (16.5 MiB at the default), made on the first such draw.
		[[deprecated("the SkinData block is deprecated: declare Skinning.glsl's DE_SKIN_PALETTE blocks; removed in 2.0")]]
		static constexpr uint32_t k_MaxSkinDataInstances = Internal::k_MaxSkinDataInstances;

		/// Skins a mesh on the GPU and places it with transform, one draw per call. joints is the
		/// skinning palette (Skeleton::ComputeSkinningPalette, or GetRestPalette for the rest pose)
		/// and must hold at least mesh->GetSkinJointCount() matrices; consecutive calls with the
		/// same palette, transform and colour, such as a model's submeshes, share one upload. The
		/// palette is copied at the call.
		///
		/// Opaque skinned meshes draw after every opaque static batch. With a translucent material
		/// a skinned mesh joins the translucent pass, sorted by the centre of its rest bounds
		/// placed with transform (an instance's translucent meshes share the first one's), so it
		/// blends with translucent static meshes in depth order. A two-sided material's mesh draws
		/// twice there, culling its near side, then its far side.
		///
		/// A lit material (null = the default) draws through a skinned twin the renderer keeps for
		/// it. A custom material's shader needs Skinning.glsl's blocks, either the DE_SkinDraw
		/// push-constant block and DE_SkinPalette (DE_SKIN_PALETTE defined), or the deprecated
		/// SkinData (warned once) at a binding from 2 to k_MaxSkinUniformBinding, with DE_SkinPalette
		/// or SkinData at a binding its textures and samplers leave free, or it is drawn with the
		/// default material and a warning. The renderer owns the shader's push constants, so it
		/// declares no other push-constant block. A mesh without a skin, with too few joints passed,
		/// or skinned to more than k_MaxSkinJoints joints goes through SubmitMesh and draws its rest
		/// pose. No-op outside a Begin/EndScene pair. A shadowed scene draws a casting instance twice,
		/// into the shadow atlas and then lit; an instance with opaque and translucent meshes is
		/// drawn lit in both passes. Each draw passes where its instance starts in the arena as a
		/// push constant, so no buffer is written between draws, except a SkinData shader's block,
		/// which is written per draw. Culled as SubmitMesh culls, by the rest pose's bounds padded on
		/// every side by half their longest side for the animation: a pose reaching farther can be
		/// culled while in view. An instance with every mesh culled from every pass takes nothing
		/// of the budget.
		void SubmitSkinnedMesh(const Mesh* mesh, const glm::mat4& transform, std::span<const glm::mat4> joints, const glm::vec4& color, Material* material = nullptr, ShadowCasting shadows = ShadowCasting::On, uint32_t shadowGroups = 1, float cullDistance = 0.0f);

		/// Skinned instances a frame: Capabilities.MaxSkinnedInstances, between 1 and
		/// k_MaxSkinnedInstancesLimit.
		uint32_t GetSkinnedInstanceBudget() const;
		/// What this frame's scenes have taken of that budget so far.
		uint32_t GetSkinnedInstancesThisFrame() const;
		/// Engine-internal: the palettes this renderer drew this frame and the last, which a Scene
		/// reads for AnimatorComponent::CullingMode.
		std::shared_ptr<const Internal::SkinnedVisibility> GetSkinnedVisibility() const;

		/// The built-in unit box; owned by the renderer, never delete it.
		Mesh* GetBoxMesh() const { return m_BoxMesh; }
		/// The built-in unit-diameter sphere; owned by the renderer, never delete it.
		Mesh* GetSphereMesh() const { return m_SphereMesh; }

		/// A material lit like the default one but with its own emissive, Roughness and Specular
		/// (see MaterialParams) and an optional albedo texture in texture slot 0, whose colour
		/// multiplies the mesh colour; transparency comes from the mesh colour's alpha alone. An
		/// empty slot 0 draws white, and an empty sampler slot 0 uses the clamp sampler. Slot 0 is
		/// the only one the lit shader has: with a texture or sampler in any other slot the
		/// material is not drawn, and the renderer warns once. Shader and CullMode are set for you:
		/// lit materials draw both faces, so open meshes and mirrored entities still show. Any
		/// Renderer3D can draw it; the caller owns it and must delete it before the renderer that
		/// created it shuts down.
		[[nodiscard]] Material* CreateLitMaterial(MaterialParams params) const;

		/// How many texture slots a PBR material has (PbrTextureSlot::Count).
		static constexpr uint32_t k_PbrTextureSlots = 5;

		/// A metallic-roughness PBR material with its five texture slots (PbrTextureSlot) filled
		/// from params, white where a map is null. Same ownership as CreateLitMaterial. Returns
		/// nullptr and logs an error when it can't be made.
		[[nodiscard]] Material* CreatePbrMaterial(const PbrMaterialParams& params) const;

		/// The material meshes with no material of their own are drawn with, for changing their
		/// shared emissive, Roughness or Specular.
		Material* GetDefaultMaterial() const { return m_Material; }

		/// Debugging the cull. Off, every mesh is drawn and casts into every shadow view, as before
		/// culling existed: what is drawn must look the same either way. Frozen, each scene of a
		/// frame (by the order scenes begin) keeps culling against the camera it had when frozen,
		/// so a camera that flies out shows what the frozen view kept; distance culling measures
		/// from that camera too.
		///@{
		void SetCullingEnabled(bool enabled) { m_CullingEnabled = enabled; }
		bool IsCullingEnabled() const { return m_CullingEnabled; }
		void SetCullingFrozen(bool frozen);
		bool IsCullingFrozen() const { return m_CullingFrozen; }
		///@}

		/// Switches Renderer3DCapabilities::Instancing; takes effect at the next EndScene.
		void SetInstancingEnabled(bool enabled) { m_Params.Capabilities.Instancing = enabled; }
		/// Whether instancing is on; see SetInstancingEnabled.
		bool IsInstancingEnabled() const { return m_Params.Capabilities.Instancing; }

		/// Switches Renderer3DCapabilities::Volumetrics.
		void SetVolumetricsEnabled(bool enabled) { m_Params.Capabilities.Volumetrics = enabled; }
		/// Whether volumetric fog is allowed; see SetVolumetricsEnabled.
		bool IsVolumetricsEnabled() const { return m_Params.Capabilities.Volumetrics; }
		/// Sets Renderer3DCapabilities::VolumetricGrid, clamping x and y to 8..256 and z to 8..128.
		void SetVolumetricGrid(const glm::uvec3& grid);
		/// The clamped froxel grid; see SetVolumetricGrid.
		const glm::uvec3& GetVolumetricGrid() const { return m_Params.Capabilities.VolumetricGrid; }
		/// Sets Renderer3DCapabilities::VolumeRaymarchResolution.
		void SetVolumeRaymarchResolution(RaymarchResolution resolution) { m_Params.Capabilities.VolumeRaymarchResolution = resolution; }
		/// The Raymarch FogVolume resolution; see SetVolumeRaymarchResolution.
		RaymarchResolution GetVolumeRaymarchResolution() const { return m_Params.Capabilities.VolumeRaymarchResolution; }

		/// Instanced draws a frame, lit and shadow, across every scene this renderer runs. A draw
		/// whose shader takes DE_InstanceDraw as a uniform block writes a small volatile buffer
		/// that has room for this many writes a frame on Vulkan; the built-in shaders push it.
		/// Each mesh of a material books its draws before culling, in submission order, so
		/// culling never changes which fit. Meshes past it are batched on the CPU instead, and a
		/// MaterialParams::Instancing material's meshes are skipped, with a warning.
		static constexpr uint32_t k_MaxInstancedDrawsPerFrame = 512;

		/// Per-scene render statistics: reset each BeginScene, complete after EndScene. A scene
		/// begun while Renderer::IsFrameSkipped() draws nothing and leaves them as they were.
		struct Statistics
		{
			/// One indexed draw per non-empty batch, and one per instanced (mesh, batch) group, so
			/// a mesh in two batches (two materials or shadow settings) takes two; a material may
			/// take several.
			uint32_t DrawCalls = 0;
			/// Static meshes accepted this scene, culled ones included, and skinned meshes drawn.
			uint32_t SubmittedMeshes = 0;
			uint32_t DroppedMeshes = 0; ///< meshes larger than one draw
			/// Static meshes the camera's view left out (or past their cull distance); ShadowsOnly
			/// ones aren't counted.
			uint32_t CulledMeshes = 0;
			uint32_t CulledSkinned = 0; ///< skinned meshes the same
			/// Casting meshes, static and skinned, that reached no shadow view of a scene that drew
			/// shadows.
			uint32_t CulledCasters = 0;
			/// Vertices transformed and uploaded this scene, for the lit and shadow passes;
			/// instanced meshes aren't.
			uint32_t VertexCount = 0;
			uint32_t IndexCount = 0; ///< indices uploaded this scene
			/// Directional lights the scene was lit by, the default light included.
			uint32_t DirectionalLights = 0;
			uint32_t LocalLights = 0; ///< point and spot lights the scene was lit by
			/// Point and spot lights whose range can't reach anything in view.
			uint32_t CulledLights = 0;
			/// Directional lights past k_MaxDirectionalLights, point and spot lights past the
			/// budget.
			uint32_t DroppedLights = 0;
			/// Skinned meshes drawn, also counted in DrawCalls and SubmittedMeshes; not batched, so
			/// not in VertexCount/IndexCount.
			uint32_t SkinnedDraws = 0;
			/// Joint palettes put into the skin arena; a model's submeshes share one.
			uint32_t SkinnedInstances = 0;
			/// Skinned meshes of instances past MaxSkinnedInstances for the frame, or past 256 for
			/// a SkinData shader.
			uint32_t DroppedSkinnedDraws = 0;
			uint32_t SkinnedJoints = 0; ///< joint matrices the instances use
			uint32_t SkinPaletteBytes = 0; ///< the scene's skin arena upload
			/// Atlas tiles rendered (cascades and local light faces), 0 for a scene that casts
			/// nothing.
			uint32_t ShadowViews = 0;
			uint32_t ShadowCascades = 0; ///< of which cascades
			uint32_t ShadowedLights = 0; ///< point and spot lights drawn with a shadow
			/// Casting point and spot lights drawn without one: past MaxShadowedLocalLights, or no
			/// atlas room.
			uint32_t UnshadowedLights = 0;
			/// Drawn point and spot lights the budget fade dimmed (LightBudgetFade).
			uint32_t FadedLights = 0;
			/// Probes the GPU evaluates this scene; ones answered at once aren't counted.
			uint32_t ShadowProbes = 0;
			uint32_t ParticleEmitters = 0; ///< emitters simulated and drawn
			/// Their rings' slots, every one simulated and drawn as an instance.
			uint32_t ParticleSlots = 0;
			uint32_t ParticlesSpawned = 0; ///< Particles spawned this scene.
			uint32_t DroppedParticleSpawns = 0; ///< spawns past an emitter's ring this step
			/// One per run of emitters sharing a blend and sprite; not in DrawCalls.
			uint32_t ParticleDrawCalls = 0;
			uint32_t ShadowCasters = 0; ///< meshes drawn into the atlas, skinned ones included
			/// Instanced atlas draws, one per casting batch, instanced mesh and skinned mesh; not
			/// in DrawCalls.
			uint32_t ShadowDrawCalls = 0;
			/// Static and skinned meshes drawn in the translucent pass, also counted in
			/// SubmittedMeshes.
			uint32_t TranslucentMeshes = 0;
			/// Draws of the translucent pass, also counted in DrawCalls.
			uint32_t TranslucentDraws = 0;
			/// Static meshes submitted with a SurfaceOverride their lit material takes, culled ones
			/// included; ShadowsOnly ones aren't counted.
			uint32_t SurfaceOverrides = 0;
			/// Static meshes drawn instanced, lit or into the shadows; not in
			/// VertexCount/IndexCount.
			uint32_t InstancedMeshes = 0;
			uint32_t InstancedDraws = 0; ///< instanced lit draws, also counted in DrawCalls
			/// Instanced atlas draws, also counted in ShadowDrawCalls.
			uint32_t InstancedShadowDraws = 0;
			/// Meshes of a MaterialParams::Instancing material skipped past
			/// k_MaxInstancedDrawsPerFrame.
			uint32_t DroppedInstancedMeshes = 0;
			/// Baked meshes of submitted static batches, culled ones included; also in
			/// SubmittedMeshes, not in VertexCount/IndexCount.
			uint32_t StaticMeshes = 0;
			/// Lit draws of static batch cells, also counted in DrawCalls.
			uint32_t StaticDraws = 0;
			/// Static batch cells baked (or emptied) by this scene's submissions.
			uint32_t StaticCellsBaked = 0;
			/// Draws with a PBR material (CreatePbrMaterial), also counted in DrawCalls.
			uint32_t PbrDraws = 0;
			/// Where each cascade ends along the view.
			float ShadowCascadeEnds[k_MaxShadowCascades] = {};
			bool Fogged = false; ///< the scene was drawn with fog: SetFog, and a perspective camera
			bool Skybox = false; ///< the scene drew a sky: SetEnvironment with DrawSkybox, a ready Environment and a perspective camera
			uint32_t EnvironmentBakes = 0; ///< Environments this scene baked (1 the first time one is used)
			bool EnvironmentLighting = false; ///< PBR materials were lit by the environment instead of the flat ambient: a ready Environment with Lighting on
			uint32_t EnvironmentLightingCopies = 0; ///< Times this scene copied an environment's lighting into the renderer (1 whenever the environment or its bake changed)
			bool Volumetrics = false; ///< the scene was drawn with volumetric fog or a froxel FogVolume: Capabilities.Volumetrics, SetVolumetricFog or SubmitFogVolume, and a perspective camera
			bool VolumetricTemporal = false; ///< the scene's volumetric fog blended with the previous frame
			uint32_t VolumetricDispatches = 0; ///< compute dispatches the scene spent on the volume
			uint32_t FogVolumes = 0; ///< FogVolumes the scene kept after culling and the budget
			uint32_t RaymarchVolumes = 0; ///< of those, drawn in Raymarch mode
			uint32_t FogVolumesCulled = 0; ///< FogVolumes skipped because the view or the froxel volume's MaxDistance can't reach them
			uint32_t FogVolumesDropped = 0; ///< FogVolumes past Capabilities.MaxFogVolumes or the Raymarch limit
			uint32_t RaymarchFallbacks = 0; ///< Raymarch volumes drawn as Froxel because the render target has no post-process scene target
			uint32_t RaymarchDispatches = 0; ///< compute dispatches the scene spent on Raymarch volumes
		};

		/// The statistics of the latest scene; the reference stays valid and is rewritten by
		/// BeginScene.
		const Statistics& GetStatistics() const { return m_Statistics; }
		/// The renderer's budgets as they are now (see Renderer3DCapabilities).
		const Renderer3DCapabilities& GetCapabilities() const { return m_Params.Capabilities; }

		/// Point and spot lights a scene can draw: Capabilities.MaxLocalLights, capped at
		/// k_MaxLocalLights.
		uint32_t GetLocalLightBudget() const;

	private:
		explicit Renderer3D(const Renderer3DParams& params);
		void Initialize();

		// `bounds` null: EndScene works them out from the transform.
		void SubmitMeshWithBounds(const Mesh* mesh, const glm::mat4& transform, const glm::vec4& color, Material* material, ShadowCasting shadows,
			uint32_t shadowGroups, float cullDistance, const SurfaceOverride* surface, const AABB* bounds, bool bounded);

		void BeginSceneInternal(const glm::mat4& viewProjection, const glm::vec4& cameraPosition);
		void ResolveSceneLights();
		void ClearSceneLights();

		// std140, mirrored by SkyboxData in Skybox.glsl: binding 0 of the sky material.
		struct SkyboxData
		{
			glm::mat4 InverseViewProjection{ 1.0f };
			glm::vec4 Params{ 1.0f, 0.0f, 1.0f, 0.0f }; // x = cos and y = sin of the lookup's turn, z = intensity
			glm::vec4 Volume{ 0.0f }; // x = 1: the froxel volume fogs the sky, y = the texture coordinate of its last slice
			glm::vec4 Sun{ 0.0f }; // xyz = direction toward the sun, w = cosine of the disc's radius, 0 = no disc
			glm::vec4 SunDisc{ 0.0f }; // rgb = the disc's linear radiance, w = the horizon's y: the disc is clipped below it
		};
		static_assert(sizeof(SkyboxData) == 128, "SkyboxData must match the std140 block in Skybox.glsl");
		struct SkyboxEntry
		{
			Material* Sky = nullptr;
			uint32_t IdleScenes = 0;
			bool Used = false;
		};
		SceneEnvironment m_PendingEnvironment;
		bool m_EnvironmentWarned = false;
		std::unordered_map<uint64_t, SkyboxEntry> m_SkyboxMaterials;
		void ReleaseIdleSkyboxes();
		bool DrawSkybox(const SceneEnvironment& environment);
		void ApplyProceduralSun(const SceneEnvironment& environment);
		glm::vec4 m_SkySun{ 0.0f }; // xyz = the travel direction of the sun this scene's procedural sky shows, w = 1 when set
		bool PrepareEnvironmentLighting(const SceneEnvironment& environment);
		bool PrepareShadows();
		void EnsureShadowResources();
		void DrawShadowPass();
		void ResolveSkinnedBudget();

		// std140, mirrored by VolumetricData in VolumetricInject.glsl and VolumetricIntegrate.glsl.
		struct VolumetricData
		{
			glm::vec4 CameraPosition{ 0.0f };
			glm::vec4 FrustumRays[4] = {};    // the view rays through the corners at uv (0,0), (1,0), (0,1), (1,1), scaled to one unit of view depth
			glm::mat4 PreviousViewProjection{ 1.0f };
			glm::vec4 Grid{ 0.0f };           // xyz = size, w = 1 when the history is valid
			glm::vec4 Slices{ 0.0f };         // x = near, y = far, z = log2(far / near), w = jitter
			glm::vec4 Medium{ 0.0f };         // x = density, y = base height, z = height falloff, w = anisotropy
			glm::vec4 Albedo{ 0.0f };         // rgb = albedo, a = weight of the current frame
			glm::vec4 Intensities{ 0.0f };    // x = ambient, y = directional
			glm::vec4 ScatterTint{ 1.0f };    // rgb = in-scatter tint, a = sun light outside every cascade
		};
		static_assert(sizeof(VolumetricData) == 240 && offsetof(VolumetricData, PreviousViewProjection) == 80 && offsetof(VolumetricData, Grid) == 144 &&
			offsetof(VolumetricData, Intensities) == 208 && offsetof(VolumetricData, ScatterTint) == 224, "VolumetricData must match the std140 block in VolumetricInject.glsl");
		static constexpr float k_VolumetricHistoryBlend = 0.1f;

		// std140, mirrored by FogVolumeData in the volume shaders: one local volume of fog.
		struct FogVolumeRecord
		{
			glm::vec4 WorldToLocal[3] = {}; // affine rows taking a world point to the volume's local space: the box is [-1, 1]^3, the sphere the unit ball
			glm::vec4 Shape{ 0.0f };        // xyz = half extents in world units, w = 0 box, 1 sphere
			glm::vec4 Medium{ 0.0f };       // x = density, y = anisotropy, z = edge fade in world units, w = raymarch steps
			glm::vec4 Albedo{ 0.0f };       // rgb = albedo, a = ambient intensity
			glm::vec4 ScatterTint{ 1.0f };  // rgb = tint, a = sun light outside every cascade
			glm::vec4 Intensities{ 0.0f };  // x = directional, y = local lights, z = max in-scatter, w = flags (1 = noise)
			glm::vec4 HeightFade{ 0.0f };   // x = fade start, y = fade end, z = band bottom, w = band top
			glm::vec4 HeightBand{ 0.0f };   // x = band multiplier, y = softness
			glm::vec4 Fades{ 0.0f };        // near start, near end, far start, far end
			glm::vec4 FadeKeep{ 0.0f };     // near keep, far keep, noise min, noise max
			glm::vec4 Noise{ 0.0f };        // xyz = phase in noise tiles, w = 1 / scale
			glm::vec4 Drift{ 0.0f };        // xyz = world displacement of the noise since the history frame
		};
		static_assert(sizeof(FogVolumeRecord) == 224 && offsetof(FogVolumeRecord, Shape) == 48 && offsetof(FogVolumeRecord, Noise) == 192 && offsetof(FogVolumeRecord, Drift) == 208,
			"FogVolumeRecord must match the std140 layout of the volume shaders");
		// std140: binding 8 of the volume kernels.
		struct FogVolumeData
		{
			glm::vec4 Global{ 0.0f };             // x = global local-light intensity, y = global max in-scatter, z = debug view, w = 1 if any volume has noise
			glm::vec4 DirectionalScatter{ 1.0f }; // per directional light VolumetricIntensity
			glm::vec4 LocalScatter[8] = {};       // light slot s at [s / 4][s % 4]: that local light's VolumetricIntensity
			FogVolumeRecord Volumes[k_MaxFogVolumes] = {}; // froxel volumes first, then Raymarch ones
		};
		static_assert(sizeof(FogVolumeData) == 3744 && offsetof(FogVolumeData, LocalScatter) == 32 && offsetof(FogVolumeData, Volumes) == 160,
			"FogVolumeData must match the std140 layout of the volume shaders");
		// std140, mirrored by RaymarchData in VolumeRaymarch.glsl: binding 1 of the Raymarch kernel.
		struct RaymarchData
		{
			glm::mat4 InverseViewProjection{ 1.0f };
			glm::vec4 CameraPosition{ 0.0f };
			glm::vec4 Target{ 0.0f }; // xy = the low-resolution size, zw = the render target's size
			glm::vec4 Params{ 0.0f }; // x = the first Raymarch record, y = their count, z = the resolution divisor, w = 1 when the froxel volume is active
			glm::vec4 Reserved{ 0.0f };
		};
		static_assert(sizeof(RaymarchData) == 128 && offsetof(RaymarchData, Target) == 80 && offsetof(RaymarchData, Params) == 96, "RaymarchData must match the std140 block in VolumeRaymarch.glsl");
		// std140, mirrored by RaymarchCompositeData in VolumeRaymarchComposite.glsl.
		struct RaymarchCompositeData
		{
			glm::mat4 InverseViewProjection{ 1.0f };
			glm::vec4 CameraPosition{ 0.0f };
			glm::vec4 Low{ 0.0f }; // xy = the low-resolution size, zw = the render target's size over the divisor
		};
		static_assert(sizeof(RaymarchCompositeData) == 96, "RaymarchCompositeData must match the std140 block in VolumeRaymarchComposite.glsl");
		// Renderer3DVolumetrics.cpp
		void PrepareVolumetrics(bool perspective);
		void DispatchVolumetrics(Texture* shadowAtlas);
		bool EnsureVolumetricResources();
		void DestroyVolumetricResources();
		void InvalidateVolumetricHistory();
		bool CullFogVolumes(bool perspective);
		void PrepareFroxelFog(bool perspective, bool dropVolumeHistory);
		void PrepareRaymarch(bool perspective);
		void UploadRaymarchResources();
		void DrawVolumeRaymarch(Texture* shadowAtlas);
		bool EnsureRaymarchResources();
		void DestroyRaymarchResources();
		void ResetLightScatter();
		void DispatchVolumetricSlice(const glm::uvec3& grid);

		friend struct Internal::Renderer3DAccess;

	private:
		Renderer3DParams m_Params;
		Statistics m_Statistics;

		// A custom material reads the first four members (m_Layout); the lit shader also reads
		// ObjectIndex (m_LitLayout), its mesh's ObjectRecord.
		struct Vertex
		{
			glm::vec3 Position;
			glm::vec3 Normal;
			glm::vec4 Color;
			glm::vec2 TexCoord;
			uint32_t ObjectIndex;
		};
		static_assert(sizeof(Vertex) == 52, "the arena's vertex stride");

		struct DirectionalLightData
		{
			glm::vec4 Direction{ 0.0f };
			glm::vec4 Color{ 0.0f }; // rgb = colour x intensity
		};

		// A point light is a spot light whose cone factor is always 1: scale 0, offset 1.
		struct LocalLightData
		{
			glm::vec4 PositionRange{ 0.0f }; // xyz = world position, w = range
			glm::vec4 Color{ 0.0f };         // rgb = colour x intensity, w = cone scale
			glm::vec4 SpotDirection{ 0.0f }; // xyz = the way the cone points, w = cone offset
		};

		struct LocalLightCandidate
		{
			LocalLightData Data;
			float Brightness = 0.0f; // strongest colour channel x intensity
			float Score = 0.0f;
			float Nearness = 0.0f; // camera distance / range
			float Priority = 0.0f; // Score / (1 + Nearness), the budget fade's continuous rank
			bool CastShadows = false;
			float ShadowStrength = 1.0f;
			uint32_t ShadowCasterGroups = 0xFFFFFFFFu;
			uint32_t ShadowFaces = 1;    // 1: a spot light's single view; 6: a cube around the light
			float OuterConeAngle = 0.0f; // degrees, for a spot light's view
			float VolumetricIntensity = 1.0f;
		};

		// std140, mirrored by CameraData in Renderer3D_Lit.glsl. The first three members are a
		// frozen prefix that custom material shaders declare on their own: only ever append.
		struct CameraData
		{
			glm::mat4 ViewProjection{ 1.0f };
			glm::vec4 LightDirection{ 0.0f }; // the first directional light's direction, or the default light's
			glm::vec4 Ambient{ 0.0f };        // x = the scene's ambient as one value
			glm::vec4 CameraPosition{ 0.0f }; // w = 1: world position; w = 0: orthographic, xyz = towards the camera
			glm::vec4 AmbientColor{ 0.0f };   // rgb = colour x intensity
			glm::ivec4 LightCounts{ 0 };      // x = directional lights, y = point and spot lights
			DirectionalLightData DirectionalLights[k_MaxDirectionalLights];
			LocalLightData LocalLights[k_MaxLocalLights];
			glm::vec4 FogColor{ 0.0f };  // rgb = colour, a = max opacity
			glm::vec4 FogParams{ 0.0f }; // x = start, y = end, z = density, w = FogMode (0 = none)
			glm::vec4 EnvironmentParams{ 0.0f }; // x = 1: lit by the environment, y = intensity x lighting intensity, z/w = cos/sin of the lookup's turn
			glm::vec4 VolumeParams{ 0.0f };      // x = 1: this scene samples the volume, y = near, z = 1 / log2(far / near), w = far
			glm::vec4 VolumeGrid{ 0.0f };        // xyz = grid size, w = 1 / the first slice's far depth
		};
		static_assert(offsetof(CameraData, LightDirection) == 64 && offsetof(CameraData, Ambient) == 80,
			"the frozen prefix custom materials declare must not move");
		static_assert(offsetof(CameraData, CameraPosition) == 96 && offsetof(CameraData, AmbientColor) == 112 &&
			offsetof(CameraData, LightCounts) == 128 && offsetof(CameraData, DirectionalLights) == 144 &&
			sizeof(DirectionalLightData) == 32 && offsetof(CameraData, LocalLights) == 144 + k_MaxDirectionalLights * 32 &&
			sizeof(LocalLightData) == 48 && offsetof(CameraData, FogColor) == 144 + k_MaxDirectionalLights * 32 + k_MaxLocalLights * 48 &&
			offsetof(CameraData, FogParams) == offsetof(CameraData, FogColor) + 16 && offsetof(CameraData, EnvironmentParams) == offsetof(CameraData, FogParams) + 16 &&
			offsetof(CameraData, VolumeParams) == 1856 && offsetof(CameraData, VolumeGrid) == 1872 && sizeof(CameraData) == 1888,
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
		bool m_VolumetricFogWarned = false;
		VolumetricFog m_VolumetricFog;
		bool m_VolumetricFogSet = false;

		struct SubmittedFogVolume
		{
			FogVolume Settings;
			glm::mat4 World{ 1.0f };
		};
		static constexpr uint32_t k_MaxPendingFogVolumes = 256;
		std::vector<SubmittedFogVolume> m_FogVolumes;
		FogVolumeData m_FogVolumeData;
		uint32_t m_FogVolumeFroxelCount = 0;
		uint32_t m_FogVolumeRaymarchCount = 0;
		double m_FogVolumeTime = 0.0;
		double m_FogVolumeHistoryTime = 0.0;
		bool m_FogVolumeHistoryTimeValid = false;
		VolumetricDebugView m_VolumetricDebugView = VolumetricDebugView::None;
		bool m_FogVolumeWarned = false;
		bool m_FogVolumePendingWarned = false;
		bool m_FogVolumeBudgetWarned = false;
		bool m_RaymarchFallbackWarned = false;

		// The Raymarch FogVolume pass: a low-resolution scatter and view-distance pair per scene target
		// (keyed by the id of its scene depth copy, resized in place, dropped after k_RaymarchIdleFrames
		// unused), the kernel that fills them and the material that blends the result over the scene
		// target. m_RaymarchScatter and m_RaymarchDepth are the pair the last draw used.
		struct RaymarchTarget
		{
			Texture* Scatter = nullptr;
			Texture* Depth = nullptr;
			uint64_t DepthId = 0;
			uint64_t LastFrame = 0;
		};
		static constexpr uint64_t k_RaymarchIdleFrames = 120;
		RaymarchTarget* AcquireRaymarchTarget(uint64_t depthId, const glm::uvec2& low);
		std::vector<RaymarchTarget> m_RaymarchTargets;
		Texture* m_RaymarchScatter = nullptr;
		Texture* m_RaymarchDepth = nullptr;
		ComputePass* m_RaymarchPass = nullptr;
		Material* m_RaymarchComposite = nullptr;
		Internal::FullscreenShader* m_RaymarchCompositeShader = nullptr;
		GraphicsBuffer* m_RaymarchDataBuffer = nullptr;
		uint64_t m_RaymarchAtlasId = 0;
		uint64_t m_RaymarchBoundDepthId = 0;
		uint32_t m_RaymarchDivisor = 0;
		RaymarchData m_RaymarchData;
		RaymarchCompositeData m_RaymarchCompositeData;
		bool m_RaymarchScene = false;     // this scene draws Raymarch volumes
		bool m_FogVolumeUploaded = false; // m_FogVolumeBuffer already holds this scene's records
		bool m_RaymarchWarned = false;

		// The debug picture of one froxel slice (Internal::Renderer3DAccess::RequestVolumetricSlice): a
		// one-shot request the next scene that dispatches the volume answers into a texture of the grid's size.
		struct VolumetricSliceRequest
		{
			bool Active = false;
			bool Scatter = false;       // the scatter volume instead of the integrated one
			bool Alpha = false;         // alpha instead of in-scatter
			uint32_t Slice = 0;
			float Exposure = 1.0f;
		};
		VolumetricSliceRequest m_SliceRequest;
		Texture* m_VolumetricSlice = nullptr;
		ComputePass* m_VolumetricSlicePass = nullptr;
		GraphicsBuffer* m_VolumetricSliceBuffer = nullptr;
		uint64_t m_VolumetricSliceSourceId = 0;
		bool m_VolumetricSliceWarned = false;

		// std140, mirrored by MaterialData in Renderer3D_Lit.glsl: binding 1 of every lit material,
		// rebuilt from its MaterialParams each EndScene. Custom shaders bring their own layout.
		struct LitMaterialData
		{
			glm::vec4 EmissiveColor{ 0.0f }; // rgb = colour
			glm::vec4 Surface{ 0.0f };       // x = emissive strength, y = roughness, z = specular, w = fog target: 0 FogColor, 1 black, 2 white
		};

		void PrepareLitMaterial(Material* material) const;
		static void TransformVertices(const Mesh& mesh, const glm::mat4& transform, const glm::vec4& color, uint32_t object, Vertex* out);

		Shader* m_Shader = nullptr;
		Material* m_Material = nullptr; // built-in lit default material
		VertexLayout m_Layout;
		VertexLayout m_LitLayout;
		VertexLayout m_MeshLayout;       // a Mesh's own MeshVertex stream, for instanced draws
		VertexLayout m_MeshShadowLayout; // its positions alone, for instanced shadow draws

		// std430, mirrored by ObjectRecord in Renderer3D_Lit.glsl: a static mesh's SurfaceOverride. One
		// storage buffer the lit materials bind holds the static batches' records from 1 (they stay put),
		// then the scene's; record 0 is never read, so a vertex's ObjectIndex of 0 means the material's own
		// values.
		struct ObjectRecord
		{
			glm::vec4 Emissive{ 0.0f }; // rgb = colour, a = strength
			glm::vec4 Surface{ 0.0f };  // x = roughness, y = specular, z = metallic (PBR only)
		};
		static_assert(sizeof(ObjectRecord) == 32, "ObjectRecord must match the std430 struct in Renderer3D_Lit.glsl");
		std::vector<ObjectRecord> m_ObjectRecords;
		GraphicsBuffer* m_ObjectBuffer = nullptr;
		uint64_t m_ObjectCapacity = 0;
		void UploadObjectRecords();

		// Camera + lights, uploaded each EndScene and bound at binding 0 on every material the
		// renderer draws (Material::SetSceneUniformBuffer).
		GraphicsBuffer* m_SceneUniformBuffer = nullptr;

		// The translucent pass packs its meshes in sorted order before they join the scene's upload.
		struct MeshChunk
		{
			std::vector<Vertex> Vertices;
			std::vector<uint32_t> Indices;
		};

		struct MaterialBatch
		{
			bool Enqueued = false; // checked and, unless SkinnedOnly or Translucent, in m_DrawOrder for the scene in progress
			// Its shader skins (has a SkinData block), so its meshes draw with the default material instead.
			bool SkinnedOnly = false;
			// Its meshes go to the translucent pass, so the batch itself stays empty.
			bool Translucent = false;
			uint32_t IdleScenes = 0;
			bool Usable = false; // EndScene: its material can be drawn
			uint32_t Slot = ~0u; // EndScene: its m_DrawSlots entry; a ShadowsOnly batch's meshes go to the shadow-only slots
			Dingo::Material* Material = nullptr; // the key's material, set by EnqueueBatch for a batch it returns
		};
		// A material's meshes batch apart by how they cast, so a pass can take or leave a whole batch.
		struct BatchKey
		{
			Dingo::Material* Material = nullptr;
			ShadowCasting Shadows = ShadowCasting::On;
			uint32_t ShadowGroups = 1;
			bool operator==(const BatchKey&) const = default;
		};
		struct BatchKeyHash
		{
			size_t operator()(const BatchKey& key) const
			{
				const size_t cast = (static_cast<size_t>(key.ShadowGroups) << 8) | static_cast<size_t>(key.Shadows);
				return std::hash<const void*>()(key.Material) ^ (cast * 0x9e3779b97f4a7c15ull);
			}
		};
		std::unordered_map<BatchKey, MaterialBatch, BatchKeyHash> m_Batches;
		static constexpr uint32_t k_MaxIdleBatchScenes = 300;
		// EnqueueBatch's last answer, for the scene in progress: a run of submissions mostly shares one key.
		BatchKey m_LastBatchKey;
		MaterialBatch* m_LastBatch = nullptr;
		Dingo::Material* m_LastBatchMaterial = nullptr;

		// Batches in the order they were first submitted to this scene. Draw order has to
		// come from here, not from the map: unordered_map iteration follows pointer hashing,
		// so the same scene would submit its materials in a different order between runs -
		// invisible for depth-tested opaques, but not for anything blended.
		std::vector<BatchKey> m_DrawOrder;

		// A SubmitMesh call, kept until EndScene culls and transforms it: the fields every EndScene pass reads.
		struct StaticSubmission
		{
			AABB Bounds;                       // world bounds, padded for rounding; empty without vertices
			const Dingo::Mesh* Mesh = nullptr;
			MaterialBatch* Batch = nullptr;    // the batch it draws in (the default material's for a SkinnedOnly one); null for a translucent mesh
			// Set by the layout: the slot it draws in, its m_InstanceCopies entry and instance group.
			uint32_t Slot = ~0u;
			uint32_t Copies = ~0u;
			uint32_t Group = ~0u;
			uint32_t Chunk = ~0u;              // the m_SceneChunks entry it is transformed into, ~0u when culled
			uint32_t ShadowGroups = 1;
			ShadowCasting Shadows = ShadowCasting::On;
			bool Bounded = false;              // false for a non-affine transform, which is never culled
			bool PastDistance = false;
			bool Seen = false;                 // the camera sees it (or culling is off, or it is unbounded)
		};
		static_assert(sizeof(StaticSubmission) == 64, "StaticSubmission is one cache line");
		// The rest of a submission, read only by the bounds pass and for the meshes that draw; same index as m_StaticSubmissions.
		struct StaticSubmissionData
		{
			StaticSubmissionData(const glm::mat4& transform, const glm::vec4& color, Dingo::Material* material, float cullDistance)
				: Transform(transform), Color(color), Material(material), CullDistance(cullDistance) {}
			glm::mat4 Transform;
			glm::vec4 Color;
			Dingo::Material* Material;   // what it draws with
			float CullDistance;
			int32_t Translucent = -1;    // into m_TranslucentSubmissions
			uint32_t Object = 0;         // its m_ObjectRecords entry, 0 for none
			uint32_t Record = 0;         // its instance record when instanced
			uint32_t ChunkVertex = 0;    // where in its chunk
			uint32_t ChunkIndex = 0;
			bool BoundsGiven = false;    // the submitter set the submission's Bounds and Bounded
		};
		std::vector<StaticSubmission> m_StaticSubmissions;
		std::vector<StaticSubmissionData> m_StaticSubmissionData;

		// One draw's run of a slot's meshes in the scene's upload. A slot is a batch of the draw order,
		// or the shadow pass's batch of the casters the camera culled, one per caster mask.
		struct SceneChunk
		{
			uint32_t Slot = 0;
			uint32_t FirstVertex = 0; // in the scene's upload
			uint32_t FirstIndex = 0;
			uint32_t VertexCount = 0;
			uint32_t IndexCount = 0;
		};
		// The scene's draws, from the upload arena, a static batch cell's buffers or, instanced, from a
		// mesh's own buffers: the shadow pass needs them before the lit pass does.
		struct ChunkDraw
		{
			BatchKey Key;
			uint32_t FirstIndex = 0; // in the arena, or in VertexBuffer/IndexBuffer when set
			uint32_t IndexCount = 0;
			bool Translucent = false; // translucent casters, for the shadow pass only (no material)
			uint32_t Group = ~0u;     // an instanced draw: its m_InstanceGroups entry
			GraphicsBuffer* VertexBuffer = nullptr; // a static batch cell's; null = the arena
			GraphicsBuffer* IndexBuffer = nullptr;
		};
		std::vector<ChunkDraw> m_ChunkDraws;

		struct DrawSlot
		{
			BatchKey Key; // a shadow-only slot's has no material and ShadowCasting::ShadowsOnly
			uint32_t OpenChunk = ~0u;
			std::vector<uint32_t> Chunks;
			std::vector<uint32_t> Groups; // its instanced draws, after its chunks
			std::vector<ChunkDraw> StaticDraws; // static batch cells, before its chunks
			uint32_t LastGroup = ~0u;     // the group the slot's last submission joined, to skip the lookup
		};
		std::vector<SceneChunk> m_SceneChunks;
		std::vector<DrawSlot> m_DrawSlots;
		uint32_t m_UsedDrawSlots = 0;
		uint32_t AcquireDrawSlot(const BatchKey& key);
		// Places a mesh's vertices and indices at the end of the slot's open chunk, or a new one.
		void PlaceInSlot(uint32_t slot, uint32_t vertexCount, uint32_t indexCount, uint32_t& chunk, uint32_t& chunkVertex, uint32_t& chunkIndex);


		// -- Instancing --------------------------------------------------------

		// std430, mirrored by InstanceRecord in Instancing.glsl: one copy of a mesh.
		struct InstanceRecord
		{
			// Left unset, so a frame's resize costs nothing: TransformStaticSubmissions writes every record
			// the layout places.
			InstanceRecord() {}

			glm::vec4 ModelRows[3];
			glm::vec4 NormalColumns[3];
			glm::vec4 Color;
			glm::uvec4 Misc; // x = ObjectIndex
		};
		static_assert(sizeof(InstanceRecord) == 128, "InstanceRecord must match the std430 struct in Instancing.glsl");

		// A slot's copies of one mesh. Instanced, they take Count records from FirstRecord.
		struct InstanceGroup
		{
			uint32_t Slot = 0;
			const Dingo::Mesh* Mesh = nullptr;
			uint64_t MeshId = 0;
			uint32_t Count = 0;
			uint32_t FirstRecord = 0;
			uint32_t Placed = 0;
			bool Instanced = false;
			bool Dropped = false; // a forced material's lit slot, past the frame's instanced draws: there is no CPU path
		};
		// A batch's copies of one mesh, culled or not: whether they are instanced, in every slot they reach.
		struct InstanceCopies
		{
			Dingo::Material* Material = nullptr;
			const Dingo::Mesh* Mesh = nullptr;
			uint32_t Count = 0;
			// Booked before culling: its lit slot's draws (lit, and shadow when it casts), and the one draw
			// of the shadow-only slot its culled casters join.
			uint32_t LitWrites = 0;
			uint32_t CasterWrites = 0;
			bool Forced = false;
			bool Instanced = false;        // its lit slot's group
			bool CastersInstanced = false; // its shadow-only slot's group
			bool Dropped = false;
		};
		struct InstanceCopyKey
		{
			const MaterialBatch* Batch = nullptr;
			uint64_t MeshId = 0;
			bool operator==(const InstanceCopyKey&) const = default;
		};
		struct InstanceCopyKeyHash
		{
			size_t operator()(const InstanceCopyKey& key) const
			{
				return std::hash<const void*>()(key.Batch) ^ (key.MeshId * 0x9e3779b97f4a7c15ull);
			}
		};
		struct InstanceGroupKey
		{
			uint64_t MeshId = 0;
			uint32_t Slot = 0;
			bool operator==(const InstanceGroupKey&) const = default;
		};
		struct InstanceGroupKeyHash
		{
			size_t operator()(const InstanceGroupKey& key) const
			{
				return std::hash<uint64_t>()(key.MeshId * 0x9e3779b97f4a7c15ull + key.Slot);
			}
		};
		// A frame's key to index map, emptied by Clear() without freeing anything: a scene's thousands of
		// distinct meshes cost no allocation once the table has grown to hold them.
		template<typename K, typename Hash>
		class FrameLookup
		{
		public:
			void Clear()
			{
				m_Count = 0;
				if (++m_Stamp == 0)
				{
					for (Entry& entry : m_Entries)
						entry.Stamp = 0;
					m_Stamp = 1;
				}
			}
			// The index this frame gave `key`, or `index` given it now (second = true).
			std::pair<uint32_t, bool> TryEmplace(const K& key, uint32_t index)
			{
				if ((m_Count + 1) * 2 > m_Entries.size())
					Grow();
				const size_t mask = m_Entries.size() - 1;
				for (size_t slot = Home(key, mask);; slot = (slot + 1) & mask)
				{
					Entry& entry = m_Entries[slot];
					if (entry.Stamp != m_Stamp)
					{
						entry = Entry{ key, index, m_Stamp };
						++m_Count;
						return { index, true };
					}
					if (entry.Key == key)
						return { entry.Index, false };
				}
			}

		private:
			struct Entry
			{
				K Key{};
				uint32_t Index = 0;
				uint32_t Stamp = 0;
			};

			static size_t Home(const K& key, size_t mask)
			{
				uint64_t hash = static_cast<uint64_t>(Hash()(key));
				hash = (hash ^ (hash >> 31)) * 0xbf58476d1ce4e5b9ull;
				return static_cast<size_t>(hash ^ (hash >> 29)) & mask;
			}

			void Grow()
			{
				std::vector<Entry> entries((std::max)(m_Entries.size() * 2, size_t(64)));
				std::swap(entries, m_Entries);
				const size_t mask = m_Entries.size() - 1;
				for (const Entry& entry : entries)
				{
					if (entry.Stamp != m_Stamp)
						continue;
					size_t slot = Home(entry.Key, mask);
					while (m_Entries[slot].Stamp == m_Stamp)
						slot = (slot + 1) & mask;
					m_Entries[slot] = entry;
				}
			}

			std::vector<Entry> m_Entries;
			size_t m_Count = 0;
			uint32_t m_Stamp = 1;
		};

		// Whether a material's meshes can be drawn instanced: a lit material through its instanced twin,
		// a custom one with MaterialParams::Instancing and a shader declaring both blocks at bindings its
		// textures and samplers leave free.
		bool CanInstance(const Material* material) const;
		bool ForcesInstancing(const Material* material) const;
		// Groups the laid-out submissions by slot and mesh and picks the groups to instance; false when
		// nothing can be (instancing off and no material forcing it), leaving the submissions ungrouped.
		bool GroupInstances(bool shadows);
		void EnsureInstancingResources();
		void EnsureStaticBuffers(const Mesh* mesh);
		void UploadInstanceRecords();
		// Sets the draw's base in the material's push constants when its shader takes DE_InstanceDraw as
		// one, else writes it to the uniform block; GroupInstances keeps the frame's draws within
		// k_MaxInstancedDrawsPerFrame.
		void SetInstanceDraw(Material* material, uint32_t firstRecord, uint32_t views);
		Material* ResolveInstancedMaterial(Material* material);

		std::vector<InstanceGroup> m_InstanceGroups;
		std::vector<InstanceCopies> m_InstanceCopies;
		FrameLookup<InstanceCopyKey, InstanceCopyKeyHash> m_InstanceCopyLookup;
		FrameLookup<InstanceGroupKey, InstanceGroupKeyHash> m_InstanceGroupLookup;
		std::vector<InstanceRecord> m_InstanceRecords;
		GraphicsBuffer* m_InstanceBuffer = nullptr;
		uint64_t m_InstanceCapacity = 0;
		GraphicsBuffer* m_InstanceDrawBuffer = nullptr;
		Shader* m_InstancedShader = nullptr;
		Shader* m_InstancedShadowShader = nullptr;
		uint64_t m_InstanceDrawFrame = ~0ull;
		uint32_t m_InstanceDrawsThisFrame = 0;
		bool m_InstanceBudgetWarned = false;
		bool m_InstanceMaterialWarned = false;
		bool m_InstanceTranslucentWarned = false;

		// -- Culling -----------------------------------------------------------

		struct CullView
		{
			glm::mat4 ViewProjection{ 1.0f };
			glm::vec4 CameraPosition{ 0.0f };
		};
		// A shadow-casting light's views, so a caster tests a local light's range before its faces.
		struct ShadowCullLight
		{
			glm::vec3 Position{ 0.0f };
			float Reach = 0.0f; // the farthest any of its views reaches from Position; 0 for the cascades
			uint32_t CasterGroups = 0xFFFFFFFFu;
			uint32_t FirstView = 0;
			uint32_t ViewCount = 0;
		};

		CullView GetCullView();
		void PrepareSceneBounds(const CullView& view);
		void PrepareShadowCulling();
		bool ReachesShadowView(const AABB& bounds, uint32_t shadowGroups) const;
		bool CameraSees(const AABB& bounds, const Material* material) const;
		void LayoutStaticSubmissions(bool shadows);
		void CullSkinnedSubmissions(bool shadows);
		void TransformStaticSubmissions();

		// -- Static batches ----------------------------------------------------

		// The pending entries placed, loose or in a cell, and the dirty cells rebaked.
		void PrepareStaticBatch(Internal::StaticBatchData& data);
		void BakeStaticCell(Internal::StaticBatchData& data, Internal::StaticCell& cell);
		// After the lit slots are acquired: each submitted cell's draws into its slot, or as a culled caster.
		void LayoutStaticBatches(bool shadows);
		// The batch's material and shadow setting, resolved (and enqueued, checked as SubmitMesh does).
		MaterialBatch* EnqueueBatch(Material*& material, ShadowCasting shadows, uint32_t shadowGroups);
		bool BakesLoose(const StaticMesh& mesh) const;
		uint32_t DynamicObjectIndex(uint32_t object) const;

		std::shared_ptr<Internal::StaticBatchShared> m_StaticShared;
		std::vector<std::shared_ptr<StaticBatch>> m_SceneStaticBatches;
		std::vector<ChunkDraw> m_StaticShadowDraws;
		uint64_t m_StaticRecordsUploaded = ~0ull; // the shared records' revision in m_ObjectBuffer
		bool m_StaticOwnerWarned = false;

		bool m_CullingEnabled = true;
		bool m_CullingFrozen = false;
		std::vector<std::optional<CullView>> m_FrozenCullViews; // per scene of the frame
		std::array<glm::vec4, Frustum::PlaneCount> m_CullPlanes{};
		bool m_CullFarPlane = false; // the scene's target has a depth for the far plane to cull against
		std::vector<Frustum> m_ShadowCullViews;
		std::vector<ShadowCullLight> m_ShadowCullLights;

		// -- The upload arena --------------------------------------------------

		// One vertex and one (32-bit) index buffer that every scene of a frame appends to, grown to the
		// busiest frame. Indices count from the arena's start, so no draw needs a base vertex. A frame's
		// writes reach the GPU in order with its draws, so the next frame can start over at 0.
		GraphicsBuffer* m_ArenaVertexBuffer = nullptr;
		GraphicsBuffer* m_ArenaIndexBuffer = nullptr;
		uint64_t m_ArenaVertexCapacity = 0;
		uint64_t m_ArenaIndexCapacity = 0;
		uint64_t m_ArenaVertexUsed = 0;
		uint64_t m_ArenaIndexUsed = 0;
		uint64_t m_ArenaFrame = ~0ull;
		// Replaced buffers, kept until frames that drew from them are done.
		std::vector<std::pair<GraphicsBuffer*, uint64_t>> m_RetiredArenaBuffers;
		std::vector<Vertex> m_SceneVertices;   // the scene's upload, sized to the busiest scene
		std::vector<uint32_t> m_SceneIndices;
		uint32_t m_SceneVertexCount = 0;
		uint32_t m_SceneIndexCount = 0;
		uint32_t m_ArenaVertexBase = 0; // where this scene's upload starts in the arena
		uint32_t m_ArenaIndexBase = 0;
		static constexpr uint64_t k_ArenaRetireFrames = 8;
		// Room in the arena for this scene's upload, at most this much, from m_ArenaVertexBase/IndexBase.
		void ReserveArena(uint64_t vertices, uint64_t indices);
		// Appends a packed chunk to the scene's upload; returns its first index in the arena.
		uint32_t AppendToScene(const MeshChunk& chunk);
		void UploadScene();

		// A mesh of the translucent pass. A static one's vertices are transformed by EndScene into
		// m_TranslucentVertices, its indices counted from its first vertex.
		struct TranslucentSubmission
		{
			Dingo::Material* Material = nullptr;
			int32_t Static = -1; // into m_StaticSubmissions and m_StaticSubmissionData
			bool Culled = false;
			uint32_t FirstVertex = 0;
			uint32_t VertexCount = 0;
			uint32_t FirstIndex = 0;
			uint32_t IndexCount = 0;
			int32_t Skinned = -1; // into m_SkinnedSubmissions; -1 = static
			ShadowCasting Shadows = ShadowCasting::On;
			uint32_t ShadowGroups = 1;
			float SortKey = 0.0f; // larger is farther from the camera
		};
		// One draw of the translucent pass, far to near: a run of static meshes of one material that
		// lie back to back in the arena, or one skinned submission.
		struct TranslucentDraw
		{
			Dingo::Material* Material = nullptr;
			int32_t Skinned = -1;
			uint32_t Chunk = 0;      // into m_TranslucentChunks
			uint32_t FirstIndex = 0; // in its chunk, then in the arena
			uint32_t IndexCount = 0;
			uint32_t Meshes = 0;
		};

		float TranslucentSortKey(const glm::vec3& point) const;
		// The opaque path's rule for lit materials, checked once a material a scene.
		bool IsTranslucentDrawable(Dingo::Material* material);
		// After the opaque layout: sorts the scene's translucent meshes in view, packs them (and, for a
		// shadowed scene, the casting ones again in m_ChunkDraws) onto the end of the scene's upload.
		void PrepareTranslucentPass(bool shadows);
		void DrawTranslucentPass(Texture* shadowAtlas);

		std::vector<TranslucentSubmission> m_TranslucentSubmissions;
		std::vector<Vertex> m_TranslucentVertices;
		std::vector<uint32_t> m_TranslucentIndices;
		std::vector<MeshChunk> m_TranslucentChunks; // storage kept from busier scenes
		std::vector<TranslucentDraw> m_TranslucentDraws;
		std::vector<std::pair<Dingo::Material*, bool>> m_TranslucentMaterials; // checked this scene: drawable or not
		std::vector<uint32_t> m_TranslucentNearIndices; // PrepareTranslucentPass' scratch: a mesh's triangles facing the camera

		Mesh* m_BoxMesh = nullptr;
		Mesh* m_SphereMesh = nullptr;

		bool m_SceneActive = false;
		bool m_SceneSkipped = false; // begun in a Renderer::SkipFrame frame: submits nothing, and EndScene only clears the lights
		bool m_MeshOverflowWarned = false;

		// std140, mirrored by SkinData in Skinning.glsl without DE_SKIN_PALETTE: a custom shader's
		// palette, uploaded per draw only as far as its mesh's last joint.
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
			uint32_t PaletteBase = 0; // its first vec4 in the skin arena, for a live instance within the budget
		};

		// The vec4s an instance takes in the skin arena ahead of its joints: Model's four columns,
		// the normal matrix's three, Color. Mirrors Skinning.glsl's DE_SKIN_PALETTE layout.
		static constexpr uint32_t k_SkinPaletteHeader = 8;
		// The last SkinData upload in one pass, so a model's submeshes share it.
		struct SkinUploads
		{
			uint32_t Data = ~0u;
		};

		struct SkinnedSubmission
		{
			const Dingo::Mesh* Mesh = nullptr;
			Dingo::Material* Material = nullptr;
			uint32_t Instance = 0;
			ShadowCasting Shadows = ShadowCasting::On;
			uint32_t ShadowGroups = 1;
			bool Translucent = false; // drawn lit by the translucent pass
			float CullDistance = 0.0f;
			AABB Bounds;     // the rest pose's, placed and padded by half per axis (the casters' box); empty without vertices
			AABB CullBounds; // the rest box padded by half its longest side on every axis, which culling tests
			bool Bounded = false;
			bool PastDistance = false;
			bool CameraCulled = false;
			bool ShadowDrawn = false;
		};

		// A lit material's copy on the skinned or instanced shader, synced from it before every draw.
		// Keyed by Material::GetId, so a new material at a freed one's address gets a twin of its own;
		// released when idle, like the batches, since nothing reports a deleted material.
		struct SkinnedTwin
		{
			Material* Twin = nullptr;
			uint32_t IdleScenes = 0;
			bool Used = false;
		};
		using TwinMap = std::unordered_map<uint64_t, SkinnedTwin>;
		Material* GetTwin(TwinMap& twins, Material* source, Shader* shader, const char* suffix);
		static void ReleaseIdleTwins(TwinMap& twins);
		std::unordered_map<uint64_t, SkinnedTwin> m_InstancedTwins;

		void EnsureSkinningResources();
		Material* ResolveSkinnedMaterial(Material* material);
		void DrawSkinnedSubmissions();
		// Draws one lit and returns its draw count: 0 when it was skipped (its instance dropped, ShadowsOnly,
		// or an unusable lit material), 2 for a translucent pass's two-sided material (backFacesFirst).
		uint32_t DrawSkinnedSubmission(const SkinnedSubmission& submission, SkinUploads& uploads, Texture* shadowAtlas, bool backFacesFirst);
		// Binds the instance's palette to material and sets its draw constants, uploading a SkinData
		// block this pass hasn't yet.
		void BindSkinInstance(Material* material, uint32_t instance, SkinUploads& uploads);
		static bool IsSkinningShader(const Shader* shader);
		static bool UsesSkinPalette(const Shader* shader);
		static bool PushesInstanceDraw(const Shader* shader);
		// Its DE_SkinPalette or SkinData block sits at a binding the material leaves free.
		static bool HasUsableSkinBlock(const Material* material);

		// Made on the first skinned draw, so an app that never skins compiles no second lit program.
		Shader* m_SkinnedShader = nullptr;
		GraphicsBuffer* m_SkinPaletteBuffer = nullptr;
		uint64_t m_SkinPaletteCapacity = 0; // vec4s
		std::vector<glm::vec4> m_SkinPalette;
		// Made on the first SkinData shader's draw.
		GraphicsBuffer* m_SkinBuffer = nullptr;
		uint32_t m_SkinDataInstancesThisFrame = 0;
		std::vector<uint8_t> m_SkinnedInstanceSkinData; // per instance of the scene: a camera-pass draw takes a SkinData shader
		bool m_SkinDataBudgetWarned = false;
		VertexLayout m_SkinnedLayout;
		VertexLayout m_SkinnedShadowLayout;
		SkinData m_SkinData;
		// D3D11 drops a partial constant-buffer update on drivers without ConstantBufferPartialUpdate.
		bool m_FullSkinUploads = false;

		std::vector<SkinnedSubmission> m_SkinnedSubmissions;
		std::vector<SkinnedInstance> m_SkinnedInstances;
		std::vector<const glm::mat4*> m_UnskinnedPalettes; // the palettes of skinned meshes SubmitMesh drew in their rest pose, counted as seen
		std::vector<glm::mat4> m_SkinnedJoints; // every instance's palette, back to back
		std::unordered_map<uint64_t, SkinnedTwin> m_SkinnedTwins;

		std::vector<uint8_t> m_SkinnedInstanceDropped; // per instance of the scene: past the frame's budget, or SkinData's
		std::vector<uint8_t> m_SkinnedInstanceLive;    // per instance of the scene: a mesh of it is drawn in some pass
		uint64_t m_SkinnedFrameIndex = 0;
		uint32_t m_SkinnedInstancesThisFrame = 0;
		bool m_SkinnedBudgetWarned = false;
		bool m_SkinFallbackWarned = false;
		bool m_SkinnedOnlyWarned = false;
		bool m_SkinMaterialWarned = false;
		bool m_SkinDataWarned = false;

		void UploadSkinPalette();
		void UploadSkinData(const SkinnedInstance& instance);
		void EnsureSkinBuffers(const Mesh* mesh);

		// -- Shadows -----------------------------------------------------------

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
			glm::uvec4 Casters{ 0xFFFFFFFFu, 0u, 0u, 0u }; // x = the light's ShadowCasterGroups
		};
		struct ShadowViews
		{
			ShadowViewData Views[k_MaxShadowTiles];
		};
		static_assert(sizeof(ShadowViewData) == 96, "ShadowViews must match the std140 block in Renderer3D_Shadow.glsl");

		bool PrepareCascades(Internal::ShadowAtlasAllocator& allocator);
		void PrepareLocalShadows(Internal::ShadowAtlasAllocator& allocator);
		// Fills tile m_ShadowViewCount from a view-projection and the atlas square it renders into.
		void AddShadowTile(const glm::mat4& viewProjection, const glm::uvec2& corner, uint32_t size, const glm::vec4& params, uint32_t casterGroups);

		ShadowData m_ShadowData;
		ShadowViews m_ShadowViews;
		uint32_t m_ShadowViewCount = 0;
		int32_t m_ShadowLight = -1; // the directional light (submission index) that casts this scene
		float m_ShadowStrength = 1.0f;
		uint32_t m_ShadowCasterGroups = 0xFFFFFFFFu;
		bool m_SecondShadowLightWarned = false;

		std::unique_ptr<Internal::ParticleRenderer> m_Particles;
		std::shared_ptr<Internal::SkinnedVisibility> m_SkinnedVisibility;

		// -- Shadow probes -----------------------------------------------------

		struct ShadowProbe
		{
			ShadowProbeLight Light;
			glm::vec3 Point{ 0.0f };
			uint64_t Key = 0;
			float Clearance = 0.0f;
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

		// Where the scene's casters are, so a cascade's depth range reaches back to every one of them,
		// culled or not: exact for static meshes, as if every vertex were transformed.
		glm::vec3 m_CasterMin{ 0.0f };
		glm::vec3 m_CasterMax{ 0.0f };
		bool m_HasCasters = false;

		GraphicsBuffer* m_ShadowDataBuffer = nullptr;
		GraphicsBuffer* m_ShadowViewsBuffer = nullptr;
		Framebuffer* m_ShadowAtlas = nullptr;
		Texture* m_PlaceholderShadowAtlas = nullptr; // bound until the first scene casts
		Sampler* m_ShadowSampler = nullptr;          // comparison, linear: hardware 2 x 2 PCF
		// The active environment's lighting, copied in when its id or bake changes: materials bind these and never
		// the Environment's own textures, which come and go.
		Texture* m_IblSpecular = nullptr;
		GraphicsBuffer* m_IblIrradiance = nullptr;
		uint64_t m_IblSourceId = 0;
		uint64_t m_IblSourceGeneration = 0;

		// Froxel volumetric fog. The integrated texture is 1x1x1 (0,0,0,1) until a scene uses the volume,
		// then it is the grid, reinitialized in place so materials that bound it keep a valid binding. The
		// scatter pair, the passes and their sampler bindings are made by the first such scene.
		Texture* m_VolumetricIntegrated = nullptr;
		Texture* m_VolumetricScatter[2] = {};
		GraphicsBuffer* m_VolumetricDataBuffer = nullptr;
		GraphicsBuffer* m_FogVolumeBuffer = nullptr;
		Texture* m_VolumeNoise = nullptr;
		Sampler* m_VolumeNoiseSampler = nullptr;
		// Pass i writes Scatter[i] and reads Scatter[1 - i] as its history; Integrate[i] reads Scatter[i].
		ComputePass* m_VolumetricInject[2] = {};
		ComputePass* m_VolumetricIntegrate[2] = {};
		Texture* m_VolumetricAtlas = nullptr;
		uint64_t m_VolumetricAtlasId = 0;
		bool m_VolumetricWarned = false;
		bool m_VolumetricDispatchWarned = false;
		bool m_VolumetricScene = false;           // this scene dispatches the volume
		uint32_t m_VolumetricPass = 0;            // the pass pair this scene uses
		uint32_t m_VolumetricHistory = 0;         // the scatter texture the last temporal scene wrote
		uint64_t m_VolumetricHistoryFrame = ~0ull; // the frame of that scene; ~0 for none
		uint64_t m_VolumetricTemporalFrame = ~0ull; // the latest frame in which a scene used the history
		glm::mat4 m_VolumetricPreviousViewProjection{ 1.0f };
		glm::uvec3 m_VolumetricHistoryGrid{ 0 };
		glm::vec2 m_VolumetricHistoryRange{ 0.0f }; // near and far of the history
		VolumetricData m_VolumetricData = {};
		glm::vec4 m_SceneVolumeParams{ 0.0f }; // CameraData's VolumeParams and VolumeGrid for the scene's sky and particles, which draw after ClearSceneLights
		glm::vec4 m_SceneVolumeGrid{ 0.0f };
		Shader* m_ShadowShader = nullptr;
		Shader* m_SkinnedShadowShader = nullptr;
		// One per caster-group mask drawn, each with the mask in its ShadowCaster block.
		std::unordered_map<uint32_t, Material*> m_ShadowMaterials;
		std::unordered_map<uint32_t, Material*> m_SkinnedShadowMaterials;
		std::unordered_map<uint32_t, Material*> m_InstancedShadowMaterials;
		enum class ShadowVariant { Static, Skinned, Instanced };
		Material* GetShadowMaterial(ShadowVariant variant, uint32_t shadowGroups);
		void DestroyShadowMaterials();
	};

}
