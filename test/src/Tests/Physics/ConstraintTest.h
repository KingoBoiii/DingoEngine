#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"

#include <string>

namespace Dingo
{

	// Drives Physics3D constraints: every joint type in standalone worlds (limits, collide-connected,
	// the ignore bookkeeping, destruction, stale handles, rejected params), then the same through a
	// Scene with Constraint3DComponent (build at OnStart, duplicate, removal, SetActive). A pendulum
	// and a slider are left swinging for the eye, beside a motor-driven arm whose F-panel sliders
	// retune the Constraint3DComponent's motor live. The motor checks cover Hinge and SwingTwist motors in
	// standalone worlds and through a Scene. Check results show in the Properties panel and the log.
	class ConstraintTest : public GraphicsTest
	{
	public:
		ConstraintTest() = default;
		virtual ~ConstraintTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

		Texture* GetResult() override { return Renderer::GetSwapChainFramebuffer()->GetAttachment(0); }

	private:
		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }
		void RunTypeChecks();
		void RunPairChecks();
		void RunLifetimeChecks();
		void RunSceneChecks();
		void RunMotorChecks();
		void RunMotorSceneChecks();
		void RunGroupChecks();
		void RunGroupSceneChecks();
		void RunBreakChecks();
		void RunBreakSceneChecks();

	private:
		TestChecks m_Checks;

		Scene* m_Scene = nullptr;
		Mesh*  m_BoxMesh = nullptr;
		Entity m_Pendulum;
		Entity m_Anchor;
		Entity m_MotorJoint;
		Entity m_MotorArm;

		PerspectiveCamera m_Camera;
	};

}
