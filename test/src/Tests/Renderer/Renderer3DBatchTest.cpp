#include "Renderer3DBatchTest.h"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <format>

namespace
{
	// A box is 24 verts / 36 indices (see Renderer3D.h): MaxVertices = 40 fits exactly one
	// box per batch (two would need 48), so k_BoxCount boxes force k_BoxCount batches for
	// the same material instead of any of them being dropped.
	constexpr uint32_t k_BoxCount = 5;
}

namespace Dingo
{

	void Renderer3DBatchTest::Check(bool condition, const std::string& name)
	{
		m_Checks.push_back({ name, condition });
		if (condition)
			DE_INFO("[PASS] {}", name);
		else
			DE_ERROR("[FAIL] {}", name);
	}

	void Renderer3DBatchTest::Initialize()
	{
		m_Checks.clear();
		m_ChecksDone = false;

		Renderer3DParams params;
		params.Capabilities.MaxVertices = 40;
		params.Capabilities.MaxIndices = 1000; // not the constraint under test — see k_BoxCount
		m_BatchRenderer = Renderer3D::Create(params);

		m_Camera = PerspectiveCamera(45.0f, m_AspectRatio, 0.1f, 100.0f);
		m_Camera.SetPosition({ 0.0f, 2.0f, 10.0f });
		m_Camera.SetTarget({ 0.0f, 0.0f, 0.0f });
	}

	void Renderer3DBatchTest::RunChecks()
	{
		const Renderer3D::Statistics& stats = m_BatchRenderer->GetStatistics();

		Check(stats.SubmittedMeshes == k_BoxCount,
			std::format("every box that fits alone in a batch is submitted (SubmittedMeshes = {})", stats.SubmittedMeshes));
		Check(stats.DrawCalls == k_BoxCount,
			std::format("one material spills into {} batches instead of dropping (DrawCalls = {})", k_BoxCount, stats.DrawCalls));
		Check(stats.DroppedMeshes == 1,
			std::format("only the mesh bigger than a whole batch is dropped (DroppedMeshes = {})", stats.DroppedMeshes));
	}

	void Renderer3DBatchTest::Update(float deltaTime)
	{
		Renderer::Clear(m_ClearColor);

		m_BatchRenderer->SetDirectionalLight({ -0.4f, -1.0f, -0.35f }, 0.35f);
		m_BatchRenderer->BeginScene(m_Camera);

		for (uint32_t i = 0; i < k_BoxCount; i++)
		{
			const float x = (static_cast<float>(i) - 0.5f * (k_BoxCount - 1)) * 1.5f;
			const glm::mat4 transform = glm::translate(glm::mat4(1.0f), { x, 0.0f, 0.0f });
			m_BatchRenderer->DrawBox(transform, { 0.35f, 0.55f, 0.80f, 1.0f });
		}

		// The built-in sphere (~289 verts, see Renderer3D.h) is bigger than the 40-vertex
		// batch on its own: the one case SubmitMesh must still drop.
		m_BatchRenderer->SubmitMesh(m_BatchRenderer->GetSphereMesh(), glm::mat4(1.0f), { 1.0f, 0.0f, 0.0f, 1.0f });

		m_BatchRenderer->EndScene();

		if (!m_ChecksDone && !Renderer::IsFrameSkipped())
		{
			m_ChecksDone = true;
			RunChecks();
		}
	}

	void Renderer3DBatchTest::Cleanup()
	{
		if (m_BatchRenderer)
		{
			m_BatchRenderer->Shutdown();
			delete m_BatchRenderer;
			m_BatchRenderer = nullptr;
		}
	}

	void Renderer3DBatchTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
		m_Camera.SetAspectRatio(m_AspectRatio);
	}

	void Renderer3DBatchTest::ImGuiRender()
	{
		GraphicsTest::ImGuiRender();
		ImGui::Separator();

		const Renderer3D::Statistics& stats = m_BatchRenderer->GetStatistics();
		ImGui::Text("Draw calls : %u", stats.DrawCalls);
		ImGui::Text("Submitted  : %u", stats.SubmittedMeshes);
		ImGui::Text("Dropped    : %u", stats.DroppedMeshes);

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			ImGui::TextColored(check.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(0.9f, 0.3f, 0.3f, 1.0f),
				"[%s] %s", check.Passed ? "PASS" : "FAIL", check.Name.c_str());
		}
	}

}
