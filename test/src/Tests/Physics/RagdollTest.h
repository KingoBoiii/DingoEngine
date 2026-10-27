#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"

#include <string>

namespace Dingo
{

	// RagdollComponent on the Fox: the auto-built bodies, a limp ragdoll falling and dragging its
	// entity along beside one driven toward the animated pose, Blend 0 leaving the animation alone,
	// GetRagdollBody's ancestor fallback, and the bodies going with the component, the entity's
	// deactivation and the world. Also RigidBodyParams3D::Mass. The two foxes stay up for the eye.
	class RagdollTest : public GraphicsTest
	{
	public:
		RagdollTest() = default;
		virtual ~RagdollTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

		Texture* GetResult() override { return Renderer::GetSwapChainFramebuffer()->GetAttachment(0); }

	private:
		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }
		void RunMassChecks();
		void BuildScene();
		void RunSceneChecks();

		Entity AddFox(const std::string& name, const glm::vec3& position, bool animated);

	private:
		TestChecks m_Checks;

		Model* m_Fox = nullptr;
		float m_FoxScale = 1.0f;
		float m_FoxLift = 0.0f;

		Scene* m_Scene = nullptr;
		Entity m_Limp;
		Entity m_Driven;

		PerspectiveCamera m_Camera;
	};

}
