#pragma once
#include "Tests/GraphicsTest.h"

#include <vector>

namespace Dingo
{

	// Renderer3D's scene lighting on a fixed set of pillars: the default light, coloured point and
	// spot lights over a dark ambient, and more point lights than the budget allows. The camera is
	// fixed and nothing moves unless Animate is on, so frames are repeatable; start a mode with
	// --lighting=default|lights|overbudget. The same lights go either straight to Renderer3D or,
	// with "Lights as entities" (--entities), through light components and Scene::SubmitLights,
	// and both paths should draw the same frame.
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
			OverBudget
		};

		struct Lighting
		{
			bool UsesDefaultLight = true;
			glm::vec3 AmbientColor{ 0.0f };
			float AmbientIntensity = 0.0f;
			std::vector<PointLight> PointLights;
			std::vector<SpotLight> SpotLights;
		};

		Lighting DescribeLighting() const;
		void SubmitLights(Renderer3D& renderer, const Lighting& lighting) const;
		void DrawScene(Renderer3D& renderer) const;

		void BuildScene();
		void BuildLightEntities(const Lighting& lighting);
		void UpdateLightEntities(const Lighting& lighting);

	private:
		PerspectiveCamera m_Camera;
		Mode m_Mode = Mode::PointAndSpot;
		bool m_Animate = false;
		float m_Time = 0.0f;

		bool m_UseEntities = false;
		Scene* m_Scene = nullptr;
		Mode m_LightEntitiesMode = Mode::DefaultLight;
		bool m_LightEntitiesBuilt = false;
		std::vector<Entity> m_LightEntities;
	};

}
