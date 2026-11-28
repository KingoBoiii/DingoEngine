#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Dingo
{

	// The procedural sky (Environment::CreateProcedural). Every environment here has a 32 texel face unless noted;
	// the checks run a few frames in and read back what the GPU baked:
	//  1. the transmittance LUT at the ground looking straight up is the closed-form Rayleigh + Mie + ozone
	//     optical depth (1 %), and a sun below the horizon has no transmittance;
	//  2. with multiple scattering, Mie and ozone off and the sun and the view vertical, the zenith radiance is
	//     the closed-form single scattering E P_R(1) (1 - e^(-2 tau)) / 2 (2 %);
	//  3. the brightest texel of mip 1 at the sun's own elevation is within 9 degrees of the sun for four suns, and a sun turned
	//     90 degrees about +Y gives the same cube with its texels permuted;
	//  4. a scene re-bakes the sky only when its sun has moved RebakeAngle, once however many scenes share a frame;
	//  5. FollowSun follows the light that casts shadows, else the first light;
	//  6. the disc the skybox adds to the sun: its centre is the encoded sum of the cube and the disc, 0.7 of its
	//     radius follows the limb darkening, 1.5 of its radius is the cube alone;
	//  7. a sun at -3 degrees draws no disc, and a PBR sphere lit only by the tinted light reads black;
	//  8. SunDiscIntensity 0 and 1000 draw the same lit spheres and bake the same spherical harmonics;
	//  9. TintSun scales a lit sphere by the transmittance, and a froxel's in-scatter by its encoded value;
	// 10. a sun at -20 degrees leaves the upper hemisphere at NightColor;
	// 11. the Scene path (a SkyboxComponent with FollowSun and TintSun, a shadowed DirectionalLightComponent) draws the same as
//     SetEnvironment (1.5/255), and moving the light's direction re-bakes once;
// 12. SetSky keeps the old sky for a NaN or negative field, and CreateProcedural returns nullptr for a bad face
	//     size or sky (expect 5 errors and 1 warning logged);
	// 13. the Environment GPU timer over 30 re-bakes of a 512 texel face is logged.
	// The view shows four panes: a sunset with the disc and bloom, the six faces at noon, a sun sweeping the sky
	// with its light, and chrome and diffuse spheres under a tinted sun in froxel fog.
	class ProceduralSkyTest : public GraphicsTest
	{
	public:
		ProceduralSkyTest() = default;
		virtual ~ProceduralSkyTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

	private:
		enum class Stage
		{
			Waiting,
			Rebake,
			Follow,
			Scene,
			Cost,
			Done
		};

		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }
		void RunChecks();
		void CheckLut();
		void CheckZenith();
		void CheckSunPosition();
		void CheckDisc();
		void CheckHorizon();
		void CheckDiscIsolation();
		void CheckTint();
		void CheckFogTint(Environment* environment, const glm::vec3& travel, const glm::vec3& transmittance);
		void CheckNight();
		void CheckRejections();
		void StepRebake();
		void StepFollow();
		void StepScene();
		void StepCost();
		void DrawView();

		Environment* MakeEnvironment(const char* name, const ProceduralSky& sky, uint32_t faceSize = 32);
		Framebuffer* NewTarget(const char* name);
		Renderer3D::Statistics RenderShot(Framebuffer* target, const glm::mat4& viewProjection, const SceneEnvironment& sky, const std::function<void(Renderer3D&)>& submit = {});
		void ReadTargets(std::vector<Framebuffer*> targets, std::function<void(const std::vector<TexturePixels>&)> done);
		glm::mat4 ShotMatrix(const glm::vec3& direction, float aspect = 1.0f, float fov = 60.0f) const;
		void SubmitLight(Renderer3D& renderer, const glm::vec3& travel, bool shadowed = false);
		void SubmitSpheres(Renderer3D& renderer, bool both);

	private:
		TestChecks m_Checks;
		Stage m_Stage = Stage::Waiting;
		uint32_t m_Step = 0;
		uint32_t m_FramesBeforeChecks = 0;
		std::shared_ptr<int> m_Alive;
		Renderer3D* m_Renderer = nullptr;
		Material* m_Diffuse = nullptr;
		Material* m_Chrome = nullptr;
		std::vector<Environment*> m_Environments;
		std::vector<Framebuffer*> m_Targets;

		Environment* m_Rebake = nullptr;
		Environment* m_FollowEnvironment = nullptr;
		Environment* m_Cost = nullptr;
		Environment* m_SceneDirect = nullptr;
		Environment* m_SceneSky = nullptr;
		Framebuffer* m_SceneDirectTarget = nullptr;
		Framebuffer* m_SceneSkyTarget = nullptr;
		Environment* m_Sunset = nullptr;
		Environment* m_Noon = nullptr;
		Environment* m_Sweep = nullptr;
		Environment* m_Fogged = nullptr;

		Framebuffer* m_RebakeTarget = nullptr;
		Framebuffer* m_FollowTarget = nullptr;
		glm::vec3 m_RebakeSun{ 0.0f };
		uint32_t m_SceneBakes = 0;
		uint32_t m_CostPrepared = 0;
		float m_Time = 0.0f;
		uint32_t m_Width = 1280;
		uint32_t m_Height = 720;
	};

}
