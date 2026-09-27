#pragma once
#include "Tests/GraphicsTest.h"

#include <string>
#include <vector>

namespace Dingo
{

	// A material that outgrows a Renderer3D batch must spill into another instead of dropping
	// meshes. Uses its own Renderer3D with a tiny MaxVertices so no other test is affected.
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
		void Check(bool condition, const std::string& name);
		void RunChecks();

	private:
		struct CheckResult
		{
			std::string Name;
			bool Passed;
		};
		std::vector<CheckResult> m_Checks;

		Renderer3D* m_BatchRenderer = nullptr;
		PerspectiveCamera m_Camera;
		bool m_ChecksDone = false;
	};

}
