#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Dingo
{

	// Froxel volumetric fog. The checks run on a second renderer with a 32 x 18 x 16 grid, Temporal off
	// unless stated, into RGBA16F targets, after 10 frames. A camera at (0, 1, 0) looks down -Z at a 16:9
	// aspect, so a column's ray is known in closed form, and the volume's textures are read back slice by
	// slice (Internal::Renderer3DAccess):
	//  3. in a uniform ambient-only medium, slices D/2 and D-1 of the integrated volume are
	//     alpha = e^(-sigma z(k+1) |ray|) and rgb = A (1 - alpha) at every texel;
	//  4. with a height falloff, the centre column's top and bottom rows match the CPU walk of the same
	//     froxels and the true optical depth of the ray;
	//  5. a sun the camera looks toward scatters as the Henyey-Greenstein ratio of one looking away, and
	//     with g = 0 the inject value is sigma C / 4;
	//  6. a froxel in a caster's shadow has no sun in-scatter, ShadowStrength 0.5 halves it, and one
	//     outside the shadow is unchanged;
	//  7. after 30 still Temporal frames the volume is within 1/255 of Temporal off and consecutive frames
	//     differ by no more than that, a new frame's blend is 0.1 current and 0.9 history, and a frame
	//     with no temporal scene drops the history;
	//  8. of two scenes in one frame only the first is temporal, the second equals the Temporal off volume
	//     and leaves the first one's history alone;
	//  17. ScatterTint scales the in-scatter of the inject and integrated volumes per channel and leaves
	//      the transmittance alone;
	//  18. a sun that casts a shadow over the whole view, seen across the end of its last cascade, goes
	//      from dark to lit over the cascade's tail (CascadeBlend of its range) with no step at the end;
	//  19. OutsideShadowLight 0 darkens the sun past the last cascade over the same tail and 1 keeps it
	//      lit, and a sun that casts no shadow stays lit whatever it is;
	//  20. after Renderer3D::ResetVolumetricHistory, or a CameraComponent with ResetVolumetricHistory
	//      rendered by the SceneRenderer, the next frame has no history, the flag is cleared and the
	//      frame after blends again.
	// The applied fog is checked a step a frame, after the temporal checks, on a 32 x 18 RGBA16F target
	// where a pixel is a froxel column: a flat quad at the view depth of slice boundary z_b is read at
	// texel b - 1, so a surface S comes out as S T + L with T = e^(-sigma z_b |ray|) and L = A (1 - T).
	// S is each case's own render without fog:
	//  1. with Volumetrics off, on but never set, or set and cleared, a scene with distance fog is the same
	//     pixel for pixel, dispatches nothing and leaves an unused renderer's volume 1 x 1 x 1;
	//  2. lit and PBR quads are S T + L, and the lit one equals FogMode::Exponential of the same density
	//     and colour;
	//  9. additive keeps S T, multiply goes toward white, alpha lit and PBR take both, over an opaque quad
	//     that is fogged itself;
	//  10. alpha and additive particles are the same blends of their fogged colour;
	//  11. an Environment's sky is S T(Far) + L(Far);
	//  12. a custom shader that includes Volumetrics.glsl equals the CPU value;
	//  13. a FogComponent with Volumetric through Scene::SubmitLights equals SetVolumetricFog and replaces
	//      its distance fog, and one without it keeps the distance fog;
	//  14. a new grid keeps the integrated texture's pointer and id, and the lit, PBR and custom materials
	//      already drawn draw the same quad against it;
	//  15. an orthographic camera has no volume;
	//  16. SetVolumetricFog rejects invalid settings and accepts the rest.
	// The view shows four panels: sun shafts through a slatted wall, height fog over boxes with a lit and
	// a PBR sphere, one sun seen with Anisotropy -0.5, 0 and 0.8, and a dense uniform fog over spheres at
	// four distances with a translucent sphere, alpha and additive particles and the sky in it.
	class FroxelFogTest : public GraphicsTest
	{
	public:
		FroxelFogTest() = default;
		virtual ~FroxelFogTest() = default;

	public:
		struct Sun
		{
			glm::vec3 Direction{ 0.0f, -1.0f, 0.0f };
			glm::vec3 Color{ 1.0f };
			float Intensity = 1.0f;
			bool Casts = false;
			float Strength = 1.0f;
		};

		struct Scene
		{
			glm::vec3 Eye{ 0.0f, 1.0f, 0.0f };
			glm::vec3 AmbientColor{ 1.0f };
			float AmbientIntensity = 0.0f;
			std::vector<Sun> Suns;
			bool Slab = false;
			bool WideSlab = false;
			VolumetricFog Fog;
		};

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

	private:
		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }
		void RunChecks();
		void CheckUniform();
		void CheckHeightFalloff();
		void CheckPhase();
		void CheckShafts();
		void CheckTint();
		void CheckOutsideShadow();
		void RenderReference();
		void StepTemporal();
		void StepApply();
		void BuildApplySteps();
		void CheckOff();
		void CheckApplyUniform();
		void CheckTranslucent();
		void CheckParticles();
		void CheckSky();
		void CheckCustomShader();
		void CheckScenePath();
		void CheckOrthographic();
		void CheckInvalid();
		void CheckGridBefore();
		void CheckGridAfter();
		void DrawView();
		void LogCost();

		Renderer3D::Statistics RenderScene(const Scene& scene, Renderer3D* renderer = nullptr);
		Renderer3D::Statistics RenderApply(Renderer3D& renderer, const std::function<void(Renderer3D&)>& body, bool orthographic = false);
		void SubmitQuad(Renderer3D& renderer, float depth, const glm::vec4& color, Material* material = nullptr) const;
		void RenderGridQuads(const std::function<void(size_t)>& afterScene);
		Texture* Integrated() const;
		Texture* Scatter() const;

	private:
		enum class Stage
		{
			Immediate,
			Temporal,
			Apply,
			Done,
		};

		TestChecks m_Checks;
		bool m_ChecksDone = false;
		uint32_t m_FramesBeforeChecks = 0;
		std::shared_ptr<int> m_Alive;

		Renderer3D* m_Probe = nullptr;
		Renderer3D* m_Renderer = nullptr;
		Framebuffer* m_Target = nullptr;
		Mesh* m_Box = nullptr;
		Mesh* m_Sphere = nullptr;

		Renderer3D* m_Off = nullptr;
		Renderer3D* m_Idle = nullptr;
		Framebuffer* m_ApplyTarget = nullptr;
		Mesh* m_Quad = nullptr;
		Material* m_LitAdditive = nullptr;
		Material* m_LitMultiply = nullptr;
		Material* m_LitAlpha = nullptr;
		Material* m_Pbr = nullptr;
		Material* m_PbrAlpha = nullptr;
		Material* m_Custom = nullptr;
		Shader* m_CustomShader = nullptr;
		Environment* m_Environment = nullptr;
		Material* m_ViewPbr = nullptr;
		Material* m_ViewGlass = nullptr;
		ParticleEffect* m_ViewAlphaEffect = nullptr;
		ParticleEffect* m_ViewAdditiveEffect = nullptr;
		std::shared_ptr<ParticleEmitter> m_ViewAlphaEmitter;
		std::shared_ptr<ParticleEmitter> m_ViewAdditiveEmitter;
		bool m_ViewSkyReady = false;
		float m_ViewDelta = 0.0f;
		ParticleEffect* m_AlphaEffect = nullptr;
		ParticleEffect* m_AdditiveEffect = nullptr;
		std::shared_ptr<ParticleEmitter> m_AlphaEmitter;
		std::shared_ptr<ParticleEmitter> m_AdditiveEmitter;
		std::filesystem::path m_Folder;
		std::vector<std::function<void()>> m_ApplySteps;
		size_t m_ApplyStep = 0;
		std::vector<TexturePixels> m_GridBefore;
		Texture* m_IntegratedBefore = nullptr;
		uint64_t m_IntegratedIdBefore = 0;
		uint32_t m_IntegratedGenerationBefore = 0;

		uint32_t m_CostFrames = 0;
		bool m_CostLogged = false;

		Stage m_Stage = Stage::Immediate;
		uint32_t m_StageFrame = 0;
		std::vector<TexturePixels> m_Reference;
		std::vector<TexturePixels> m_Still;

		uint32_t m_Width = 1280;
		uint32_t m_Height = 720;
	};

}
