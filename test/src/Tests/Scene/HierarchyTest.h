#pragma once
#include "Tests/GraphicsTest.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace Dingo
{

	// Parent-child transforms through a real Scene: a spinning sun -> planet -> moon chain with a
	// light riding the planet, a tank whose turret and barrel follow its hull, and bodies under
	// parents (a dynamic crate on a moving carrier, kinematic children of a spinning pivot and of a
	// MoveKinematic'd platform, a falling chain of dynamic boxes, a character controller on a moving
	// carrier), plus a 2D scene with the same cases on a 2D tank and Box2D bodies. Check results
	// show in the Properties panel and the log.
	class HierarchyTest : public GraphicsTest
	{
	public:
		explicit HierarchyTest(Renderer2D* renderer2D) : m_Renderer2D(renderer2D) {}
		virtual ~HierarchyTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

		Texture* GetResult() override { return Renderer::GetSwapChainFramebuffer()->GetAttachment(0); }

	private:
		void Check(bool condition, const std::string& name);
		void RunStructuralChecks();
		void RunLightProbe();
		void BuildScene();
		void Animate(float deltaTime);
		void TrackPhysics();
		void RunPhysicsChecks();
		static glm::vec3 PlatformPosition(float time);

		// The 2D section (HierarchyTest2D.cpp): its own Scene, drawn through Renderer2D.
		void RunStructuralChecks2D();
		void BuildScene2D();
		void Animate2D(float deltaTime);
		void TrackPhysics2D();
		void RunPhysicsChecks2D();
		void Render2D();
		static glm::vec2 Platform2DPosition(float time);
		// The muzzle's world position from the test's own arithmetic, not the engine's.
		glm::vec2 ExpectedMuzzlePosition() const;

		enum class View { Scene3D, Scene2D, Probe2D };

	private:
		struct CheckResult
		{
			std::string Name;
			bool Passed;
		};
		std::vector<CheckResult> m_Checks;

		Scene* m_Scene = nullptr;
		Mesh*  m_HullMesh = nullptr;
		Mesh*  m_TurretMesh = nullptr;
		Mesh*  m_BarrelMesh = nullptr;
		Mesh*  m_PlatformMesh = nullptr;

		Entity m_Sun;
		Entity m_Planet;
		Entity m_Hull;
		Entity m_Turret;
		Entity m_Carrier;
		Entity m_Crate;
		Entity m_Pivot;
		Entity m_Paddle;
		Entity m_Platform;
		Entity m_Rider;
		Entity m_WalkerCarrier;
		Entity m_Walker;
		std::vector<Entity> m_Fallers;

		float m_Time = 0.0f;
		bool  m_Animate = true;
		bool  m_LightProbeDone = false;
		bool  m_PhysicsChecksDone = false;

		glm::vec3 m_CrateStart{ 0.0f };
		glm::vec3 m_CarrierStart{ 0.0f };
		float m_MaxCrateBodyGap = 0.0f;
		float m_MaxPaddleBodyGap = 0.0f;
		float m_MaxPaddleAngleGap = 0.0f;
		float m_PaddleTravel = 0.0f;
		glm::vec3 m_LastPaddlePosition{ 0.0f };
		float m_MaxRiderGap = 0.0f;
		float m_MaxRiderAngleGap = 0.0f;
		float m_PlatformTravel = 0.0f;
		glm::vec3 m_LastPlatformPosition{ 0.0f };
		float m_MaxFallerGap = 0.0f;
		float m_MaxWalkerGap = 0.0f;
		float m_WalkerCarrierTravel = 0.0f;
		glm::vec3 m_WalkerStart{ 0.0f };
		glm::vec3 m_WalkerCarrierStart{ 0.0f };

		Renderer2D* m_Renderer2D = nullptr;
		Scene* m_Scene2D = nullptr;
		Font*  m_Font = nullptr;
		View   m_View = View::Scene3D;

		Entity m_Hull2D;
		Entity m_Turret2D;
		Entity m_Carrier2D;
		Entity m_Crate2D;
		Entity m_Pivot2D;
		Entity m_Paddle2D;
		Entity m_Platform2D;
		Entity m_Rider2D;
		std::vector<Entity> m_Fallers2D;

		glm::vec3 m_Crate2DStart{ 0.0f };
		float m_Carrier2DTravel = 0.0f;
		glm::vec3 m_Carrier2DStart{ 0.0f };
		float m_MaxCrate2DGap = 0.0f;
		float m_MaxPaddle2DGap = 0.0f;
		float m_MaxPaddle2DAngleGap = 0.0f;
		float m_MaxRider2DGap = 0.0f;
		float m_MaxRider2DAngleGap = 0.0f;
		float m_Platform2DTravel = 0.0f;
		float m_Paddle2DTravel = 0.0f;
		glm::vec3 m_LastPaddle2DPosition{ 0.0f };
		glm::vec3 m_LastPlatform2DPosition{ 0.0f };
		float m_MaxFaller2DGap = 0.0f;

		PerspectiveCamera m_Camera;
		float m_OrbitAngle = 25.0f;
		bool  m_AutoOrbit = true;
	};

}
