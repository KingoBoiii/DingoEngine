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
	// readbacks of its blending order, and surface overrides: how they batch, and a scene of overridden
	// meshes read back against the same scene drawn with a lit material per mesh, and instancing: the
	// default material's repeated meshes and a custom MaterialParams::Instancing material, each one
	// draw, and a frame's scenes each reading their own records, and static batches: a scene drawn
	// through one read back against SubmitMesh, rebakes, culling and a hot reload. --batch-alpha=<a>
	// also draws three overlapping translucent boxes of that alpha in front of the batched ones.
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
		void BuildSurfaceChecks();
		void BuildInstancingChecks();
		void BuildStaticChecks();
		// Reads every target back and hands the images to done, in the order given.
		void ReadAll(std::vector<Framebuffer*> targets, std::function<void(const std::vector<TexturePixels>&)> done);
		Framebuffer* CreateCheckTarget(uint32_t size);
		template<typename Submit>
		Renderer3D::Statistics RenderCheck(Framebuffer* target, const glm::mat4& viewProjection, Submit&& submit, Renderer3D* renderer = nullptr);
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
		std::vector<Material*> m_SurfaceMaterials;
		Shader* m_InstancedShader = nullptr;
		Shader* m_BlocklessShader = nullptr;
		Shader* m_PushedInstanceShader = nullptr;
		Material* m_InstancedMaterial = nullptr;
		Material* m_BlocklessMaterial = nullptr;
		Material* m_PushedInstanceMaterial = nullptr;
		Mesh* m_Quad = nullptr;
		Mesh* m_SkinnedQuad = nullptr;
		Mesh* m_ClosedQuads = nullptr;
		Mesh* m_SkinnedClosedQuads = nullptr;
		Mesh* m_IndicesOnly = nullptr;
		std::vector<Mesh*> m_BudgetQuads;
		std::vector<glm::mat4> m_Palette;
		std::vector<Framebuffer*> m_CheckTargets;
		std::vector<std::function<void()>> m_CheckSteps;
		size_t m_NextCheckStep = 0;
		std::shared_ptr<int> m_Alive;
		float m_ShowcaseAlpha = -1.0f;

		std::shared_ptr<StaticBatch> m_StaticBatch;
		std::shared_ptr<StaticBatch> m_ForeignBatch; // made by m_BatchRenderer, released after it shuts down
		std::vector<StaticMeshId> m_StaticIds;
		Model* m_Duck = nullptr;
	};

}
