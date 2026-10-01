#pragma once
#include "Tests/GraphicsTest.h"

#include <vector>

namespace Dingo
{

	// Renderer3D's scene lighting on a fixed set of pillars: the default light, coloured point and
	// spot lights over a dark ambient, and more point lights than the budget allows. The camera is
	// fixed and nothing moves unless Animate is on, so frames are repeatable; start a mode with
	// --lighting=default|lights|overbudget|materials. The same lights go either straight to
	// Renderer3D or, with "Lights as entities" (--entities), through light components and
	// Scene::SubmitLights, and both paths should draw the same frame. The materials mode shows lit
	// materials instead: a row of spheres from smooth to rough, a glowing lamp with a light inside
	// it and a textured crate, with Specular (--specular=off) switching the highlights off.
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

	private:
		PerspectiveCamera m_Camera;
		Mode m_Mode = Mode::PointAndSpot;
		bool m_Animate = false;
		float m_Time = 0.0f;

		static constexpr int k_RoughnessSteps = 5;
		Material* m_RowMaterials[k_RoughnessSteps] = {};
		Material* m_LampMaterial = nullptr;
		Material* m_CrateMaterial = nullptr;
		bool m_Specular = true;

		bool m_UseEntities = false;
		Scene* m_Scene = nullptr;
		Mode m_LightEntitiesMode = Mode::DefaultLight;
		bool m_LightEntitiesBuilt = false;
		std::vector<Entity> m_LightEntities;
	};

}
