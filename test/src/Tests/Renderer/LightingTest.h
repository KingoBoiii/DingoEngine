#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace Dingo
{

	// Renderer3D's scene lighting on a fixed set of pillars: the default light, coloured point and
	// spot lights over a dark ambient, and more point lights than the budget allows. The camera is
	// fixed and nothing moves unless Animate is on, so frames are repeatable; start a mode with
	// --lighting=default|lights|overbudget|materials. The same lights go either straight to
	// Renderer3D or, with "Lights as entities" (--entities), through light components and
	// Scene::SubmitLights, and both paths draw the same frame to within float rounding. The
	// materials mode shows lit materials instead: a row of spheres from smooth to rough, a glowing
	// lamp with a light inside it and a crate whose texture loads asynchronously, so it reaches a
	// material that has already drawn, with Specular (--specular=off) switching the highlights off.
	// On every start it also runs PASS/FAIL checks of the light bookkeeping (counts, culling, the
	// budget, the default light) on private Renderer3Ds, one scene per frame and apart from the
	// modes above, then checks GetLightAttenuation and the light components' ToLight. --post draws the
	// scene through the post chain's default Soft tone curve, so what clips without it rolls off.
	class LightingTest : public GraphicsTest
	{
	public:
		LightingTest() = default;
		virtual ~LightingTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

	private:
		enum class Mode : int
		{
			DefaultLight,
			PointAndSpot,
			OverBudget,
			Materials
		};

		struct Lighting
		{
			bool UsesDefaultLight = true;
			glm::vec3 AmbientColor{ 0.0f };
			float AmbientIntensity = 0.0f;
			std::vector<DirectionalLight> DirectionalLights;
			std::vector<PointLight> PointLights;
			std::vector<SpotLight> SpotLights;
		};

		Lighting DescribeLighting() const;
		void SubmitLights(Renderer3D& renderer, const Lighting& lighting) const;
		void DrawScene(Renderer3D& renderer) const;
		void DrawMaterialsScene(Renderer3D& renderer);

		void BuildScene();
		void BuildLightEntities(const Lighting& lighting);
		void UpdateLightEntities(const Lighting& lighting);

		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }
		void BuildCheckSteps();
		void RunNextCheckStep();

	private:
		PerspectiveCamera m_Camera;
		Mode m_Mode = Mode::PointAndSpot;
		bool m_Animate = false;
		bool m_PostProcess = false; // --post: the scene through the post chain's default Soft curve
		float m_Time = 0.0f;

		static constexpr int k_RoughnessSteps = 5;
		Material* m_RowMaterials[k_RoughnessSteps] = {};
		Material* m_LampMaterial = nullptr;
		Material* m_CrateMaterial = nullptr;
		AssetHandle m_CrateTexture;
		bool m_Specular = true;

		bool m_UseEntities = false;
		Scene* m_Scene = nullptr;
		Mode m_LightEntitiesMode = Mode::DefaultLight;
		bool m_LightEntitiesBuilt = false;
		std::vector<Entity> m_LightEntities;

		TestChecks m_Checks;

		// Each EndScene writes the renderer's volatile scene buffer and Vulkan only allows a few
		// writes per frame, so a frame runs one step and a step renders one scene.
		std::vector<std::function<void()>> m_CheckSteps;
		size_t m_NextCheckStep = 0;
		Renderer3D* m_CheckRenderer = nullptr;
		Renderer3D* m_BudgetCheckRenderer = nullptr;
	};

}
