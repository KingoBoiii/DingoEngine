#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Dingo
{

	// Image-based lighting. The test writes three 512 x 256 RGBE panoramas: a uniform one of radiance 1, an
	// analytic one L(dir) = a + b y + c (3 y^2 - 1) with a different tint per channel, and one that changes
	// with the azimuth, L(dir) = 1 + 0.6 x. Offscreen shots of single spheres, read back at the centre pixel
	// (the sphere's normal and view vector are then exact), check by readback:
	//  1. the BRDF LUT at nine texels matches DE_IntegrateBrdf of Ibl.glsl run on the CPU;
	//  2. the spherical harmonics of the uniform environment: c0 Y00 = 1 and every other coefficient is 0;
	//  3. the spherical harmonics of the analytic environment reproduce the closed-form irradiance / pi
	//     in seven directions within 1.5 %;
	//  4. the prefiltered specular cube: the uniform one is 1 on every face of every mip, mip 0 of the
	//     analytic one is its function, and mip 5 matches a CPU Monte Carlo of the same integral within 3 %;
	//  5. a white furnace: PBR spheres in the uniform environment equal diffuse + F0 A + B from the LUT, and
	//     a dielectric stays within the CPU-computed gap of the flat AmbientColor 1 (which uses EnvBRDFApprox);
	//  6. the analytic environment lights a diffuse sphere from +Y and -Y as its closed-form irradiance, and
	//     a chrome sphere's centre equals the sky pixel it reflects times A + B;
	//  7. Intensity and LightingIntensity scale the lighting as one linear multiplier, 0 is black, and
	//     Lighting = false is the flat ambient pixel for pixel;
	//  8. Rotation turns the lighting with the sky: a chrome sphere keeps matching the sky it reflects;
	//  9. a lit (non-PBR) sphere draws the same with and without the environment;
	// 10. two environments in alternating scenes of one frame copy only when the environment changes, and
	//     each scene is lit by its own;
	// 11. an AssetManager-loaded .hdr rewritten and reloaded keeps its Environment, bakes again, copies the
	// lighting again and lights a diffuse sphere with the new radiance;
	// 12. an Environment deleted after use leaves no lighting behind, and a new one lights correctly;
	// 13. a material occlusion of 0.5 scales the diffuse term and the specular occlusion as the shader's
	//     Lagarde term says;
	// 14. skinned, instanced, translucent and static-batch spheres match the plain draw of the same sphere;
	// 15. an orthographic camera is lit by the environment too, without drawing the sky;
	// 16. a Scene with a SkyboxComponent and a PBR MeshRendererComponent matches the direct Renderer3D path
	//     within one 8-bit step, Visible = false keeps the lighting without the sky, and Lighting = false
	//     falls back to the flat ambient.
	// The view shows four panels: a roughness x metallic grid under the analytic sky, the same grid in the
	// furnace, a diffuse and a chrome sphere, and a normal-mapped occluded sphere lit by the environment
	// (left) and by the flat ambient (right).
	class IblTest : public GraphicsTest
	{
	public:
		IblTest() = default;
		virtual ~IblTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

	private:
		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }
		void RunChecks();
		void CheckLut();
		void CheckShUniform();
		void CheckShAnalytic();
		void CheckPrefilterUniform();
		void CheckPrefilterAnalytic();
		void CheckFurnace();
		void CheckAnalyticLighting();
		void CheckIntensity();
		void CheckRotation();
		void CheckLitUnchanged();
		void CheckTwoEnvironments();
		void CheckHotReload();
		void CheckDeleted();
		void CheckOcclusion();
		void CheckPaths();
		void CheckOrthographic();
		void CheckSceneDriven();
		void DrawView();

		Framebuffer* NewTarget(const char* name, uint32_t size);
		glm::mat4 ShotMatrix(const glm::vec3& direction) const;
		Renderer3D::Statistics RenderShot(Framebuffer* target, const glm::mat4& viewProjection, const SceneEnvironment* sky, const std::function<void(Renderer3D&)>& submit);
		void ReadTargets(std::vector<Framebuffer*> targets, std::function<void(const std::vector<TexturePixels>&)> done);
		Material* CreatePbr(const PbrMaterialParams& params);
		Material* CreatePbr(const glm::vec3& base, float metallic, float roughness);
		Renderer3D::Statistics ShotOfSphere(Framebuffer* target, const glm::vec3& direction, const SceneEnvironment* sky, Material* material, float ambient = 0.3f);

	private:
		TestChecks m_Checks;
		bool m_ChecksDone = false;
		uint32_t m_FramesBeforeChecks = 0;
		std::shared_ptr<int> m_Alive;
		std::filesystem::path m_Folder;
		std::filesystem::path m_UniformFile;
		std::filesystem::path m_AnalyticFile;

		Environment* m_Uniform = nullptr;
		Environment* m_Analytic = nullptr;
		Environment* m_Azimuth = nullptr;
		Renderer3D* m_Renderer = nullptr;

		Mesh* m_Sphere = nullptr;
		Mesh* m_SkinnedSphere = nullptr;
		std::vector<glm::mat4> m_Palette;
		std::shared_ptr<StaticBatch> m_StaticBatch;
		std::vector<Framebuffer*> m_Targets;
		std::vector<Material*> m_Materials;
		std::vector<Texture*> m_Textures;
		std::vector<Material*> m_GridMaterials;
		Material* m_DiffuseMaterial = nullptr;
		Material* m_ChromeMaterial = nullptr;
		Material* m_BumpMaterial = nullptr;

		uint32_t m_Width = 1280;
		uint32_t m_Height = 720;
	};

}
