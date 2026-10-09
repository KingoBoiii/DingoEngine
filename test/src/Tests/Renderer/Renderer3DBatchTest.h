#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Dingo
{

	// A material that outgrows a Renderer3D batch must spill into another instead of dropping
	// meshes. Uses its own Renderer3D with a tiny MaxVertices so no other test is affected.
	// Then, one step a frame on a second renderer, the translucent pass: how it batches, and
	// readbacks of its blending order. --batch-alpha=<a> also draws three overlapping translucent
	// boxes of that alpha in front of the batched ones.
	class Renderer3DBatchTest : public GraphicsTest
	{
	public:
		Renderer3DBatchTest() = default;
		virtual ~Renderer3DBatchTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

	private:
		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }
		void RunChecks();
		void BuildTranslucentChecks();
		Framebuffer* CreateCheckTarget();
		template<typename Submit>
		Renderer3D::Statistics RenderCheck(Framebuffer* target, const glm::mat4& viewProjection, Submit&& submit);
		void ReadCheck(Framebuffer* target, const glm::vec3& expected, const std::string& name);
		void DrawTranslucentShowcase();

	private:
		TestChecks m_Checks;

		Renderer3D* m_BatchRenderer = nullptr;
		PerspectiveCamera m_Camera;
		bool m_ChecksDone = false;

		Renderer3D* m_CheckRenderer = nullptr;
		Material* m_TranslucentA = nullptr;
		Material* m_TranslucentB = nullptr;
		Mesh* m_Quad = nullptr;
		Mesh* m_SkinnedQuad = nullptr;
		std::vector<glm::mat4> m_Palette;
		std::vector<Framebuffer*> m_CheckTargets;
		std::vector<std::function<void()>> m_CheckSteps;
		size_t m_NextCheckStep = 0;
		std::shared_ptr<int> m_Alive;
		float m_ShowcaseAlpha = -1.0f;
	};

}
