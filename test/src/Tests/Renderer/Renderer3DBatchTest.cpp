#include "Renderer3DBatchTest.h"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <cstdlib>
#include <format>

namespace
{
	// A box is 24 verts / 36 indices (see Renderer3D.h): MaxVertices = 40 fits exactly one
	// box per batch (two would need 48), so k_BoxCount boxes force k_BoxCount batches for
	// the same material instead of any of them being dropped.
	constexpr uint32_t k_BoxCount = 5;

	constexpr uint32_t k_CheckTargetSize = 16;
	constexpr float k_CheckTolerance = 2.5f / 255.0f;

	const glm::vec4 k_HalfRed{ 1.0f, 0.0f, 0.0f, 0.5f };
	const glm::vec4 k_HalfGreen{ 0.0f, 1.0f, 0.0f, 0.5f };

	// Red at half alpha over black, then green at half alpha over that: what drawing the far red
	// quad before the near green one leaves. The other order leaves (0.5, 0.25, 0).
	const glm::vec3 k_GreenOverRed{ 0.25f, 0.5f, 0.0f };

	glm::mat4 CheckViewProjection()
	{
		return glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f) *
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	}

	// A quad filling the check camera's view (or the orthographic one's) at depth z.
	glm::mat4 WallAt(float z)
	{
		return glm::scale(glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, z }), { 30.0f, 30.0f, 1.0f });
	}

	std::string Counts(const Dingo::Renderer3D::Statistics& stats)
	{
		return std::format("translucent {} meshes in {} draws, {} draw calls, {} submitted, {} skinned",
			stats.TranslucentMeshes, stats.TranslucentDraws, stats.DrawCalls, stats.SubmittedMeshes, stats.SkinnedDraws);
	}
}

namespace Dingo
{

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

		m_CheckRenderer = Renderer3D::Create();
		m_TranslucentA = m_CheckRenderer->CreateLitMaterial(MaterialParams().SetDebugName("BatchTest_TranslucentA").SetTranslucent(true));
		m_TranslucentB = m_CheckRenderer->CreateLitMaterial(MaterialParams().SetDebugName("BatchTest_TranslucentB").SetTranslucent(true));

		// A unit quad facing +z, as a mesh and as a skinned mesh bound to one joint.
		const std::vector<MeshVertex> vertices = {
			{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },
			{ {  0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
			{ {  0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
			{ { -0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
		};
		const std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };
		std::vector<SkinnedMeshVertex> skinVertices;
		for (const MeshVertex& vertex : vertices)
			skinVertices.push_back({ vertex.Position, vertex.Normal, vertex.TexCoord, glm::u16vec4(0), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f) });
		m_Quad = Mesh::Create(vertices, indices);
		m_SkinnedQuad = Mesh::CreateSkinned(vertices, skinVertices, indices);
		m_Palette.assign(1, glm::mat4(1.0f));

		m_Alive = std::make_shared<int>(0);
		m_NextCheckStep = 0;
		BuildTranslucentChecks();

		m_ShowcaseAlpha = -1.0f;
		if (auto alpha = Application::Get().GetCommandLineArgs().Get("batch-alpha"); alpha && !alpha->empty())
			m_ShowcaseAlpha = glm::clamp(std::strtof(std::string(*alpha).c_str(), nullptr), 0.0f, 1.0f);
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

	Framebuffer* Renderer3DBatchTest::CreateCheckTarget()
	{
		Framebuffer* target = Framebuffer::Create(FramebufferParams()
			.SetDebugName("BatchTest_Check")
			.SetWidth(static_cast<int32_t>(k_CheckTargetSize))
			.SetHeight(static_cast<int32_t>(k_CheckTargetSize))
			.SetEnableDepth(true)
			.AddAttachment({ TextureFormat::RGBA8_UNORM }));
		m_CheckTargets.push_back(target);
		return target;
	}

	// Lit by a white ambient alone, so a mesh shows its own colour.
	template<typename Submit>
	Renderer3D::Statistics Renderer3DBatchTest::RenderCheck(Framebuffer* target, const glm::mat4& viewProjection, Submit&& submit)
	{
		Framebuffer* previous = Renderer::GetRenderTarget();
		Renderer::SetRenderTarget(target);
		m_CheckRenderer->BeginScene(viewProjection);
		m_CheckRenderer->Clear({ 0.0f, 0.0f, 0.0f, 1.0f });
		m_CheckRenderer->SetAmbientLight(glm::vec3(1.0f), 1.0f);
		submit(*m_CheckRenderer);
		m_CheckRenderer->EndScene();
		Renderer::SetRenderTarget(previous);
		return m_CheckRenderer->GetStatistics();
	}

	void Renderer3DBatchTest::ReadCheck(Framebuffer* target, const glm::vec3& expected, const std::string& name)
	{
		const std::weak_ptr<int> alive = m_Alive;
		target->GetAttachment(0)->ReadPixels([this, alive, expected, name](const TexturePixels& pixels)
		{
			if (alive.expired())
				return;

			const glm::vec3 pixel = pixels.Data.empty() ? glm::vec3(-1.0f) : glm::vec3(pixels.GetPixel(pixels.Width / 2, pixels.Height / 2));
			const bool matches = glm::all(glm::lessThanEqual(glm::abs(pixel - expected), glm::vec3(k_CheckTolerance)));
			Check(matches, std::format("{} (expected {:.3f} {:.3f} {:.3f}, read {:.3f} {:.3f} {:.3f})",
				name, expected.r, expected.g, expected.b, pixel.r, pixel.g, pixel.b));
		});
	}

	void Renderer3DBatchTest::BuildTranslucentChecks()
	{
		m_CheckSteps.clear();

		auto statsStep = [this](std::function<void(Renderer3D&)> submit, std::function<void(const Renderer3D::Statistics&)> check)
		{
			m_CheckSteps.push_back([this, submit = std::move(submit), check = std::move(check)]
			{
				check(RenderCheck(CreateCheckTarget(), CheckViewProjection(), submit));
			});
		};

		statsStep([this](Renderer3D& renderer)
		{
			renderer.SubmitMesh(m_Quad, WallAt(-6.0f), k_HalfGreen, m_TranslucentA);
			renderer.SubmitMesh(m_Quad, WallAt(-12.0f), k_HalfGreen, m_TranslucentA);
			renderer.SubmitMesh(m_Quad, WallAt(-9.0f), k_HalfGreen, m_TranslucentA);
		}, [this](const Renderer3D::Statistics& stats)
		{
			Check(stats.TranslucentMeshes == 3 && stats.TranslucentDraws == 1 && stats.DrawCalls == 1 && stats.SubmittedMeshes == 3,
				std::format("three meshes of one translucent material, submitted out of depth order, draw in one draw ({})", Counts(stats)));
		});

		statsStep([this](Renderer3D& renderer)
		{
			renderer.SubmitMesh(m_Quad, WallAt(-6.0f), k_HalfGreen, m_TranslucentA);
			renderer.SubmitMesh(m_Quad, WallAt(-9.0f), k_HalfGreen, m_TranslucentB);
			renderer.SubmitMesh(m_Quad, WallAt(-12.0f), k_HalfGreen, m_TranslucentA);
		}, [this](const Renderer3D::Statistics& stats)
		{
			Check(stats.TranslucentMeshes == 3 && stats.TranslucentDraws == 3 && stats.DrawCalls == 3,
				std::format("two translucent materials alternating in depth take three draws ({})", Counts(stats)));
		});

		statsStep([this](Renderer3D& renderer)
		{
			renderer.SubmitMesh(m_Quad, WallAt(-9.0f), k_HalfGreen, m_TranslucentA);
			renderer.SubmitMesh(m_Quad, WallAt(-12.0f), glm::vec4(1.0f));
		}, [this](const Renderer3D::Statistics& stats)
		{
			Check(stats.TranslucentMeshes == 1 && stats.TranslucentDraws == 1 && stats.DrawCalls == 2 && stats.SubmittedMeshes == 2,
				std::format("an opaque mesh batches apart from a translucent one ({})", Counts(stats)));
		});

		statsStep([this](Renderer3D& renderer)
		{
			renderer.SubmitMesh(m_Quad, WallAt(-9.0f), k_HalfGreen, m_TranslucentA, ShadowCasting::ShadowsOnly);
		}, [this](const Renderer3D::Statistics& stats)
		{
			Check(stats.TranslucentMeshes == 0 && stats.DrawCalls == 0 && stats.SubmittedMeshes == 1,
				std::format("a translucent mesh that only casts draws nothing lit ({})", Counts(stats)));
		});

		statsStep([this](Renderer3D& renderer)
		{
			renderer.SubmitSkinnedMesh(m_SkinnedQuad, WallAt(-12.0f), m_Palette, glm::vec4(1.0f));
			renderer.SubmitSkinnedMesh(m_SkinnedQuad, WallAt(-12.0f), m_Palette, glm::vec4(1.0f), m_TranslucentA);
		}, [this](const Renderer3D::Statistics& stats)
		{
			Check(stats.SkinnedDraws == 2 && stats.SkinnedInstances == 1 && stats.TranslucentMeshes == 1 && stats.TranslucentDraws == 1 && stats.DrawCalls == 2,
				std::format("an instance's translucent skinned mesh draws in the translucent pass and shares its palette ({}, {} instances)", Counts(stats), stats.SkinnedInstances));
		});

		auto probeStep = [this](const char* name, const glm::vec3& expected, const glm::mat4& viewProjection, std::function<void(Renderer3D&)> submit)
		{
			m_CheckSteps.push_back([this, name, expected, viewProjection, submit = std::move(submit)]
			{
				Framebuffer* target = CreateCheckTarget();
				RenderCheck(target, viewProjection, submit);
				ReadCheck(target, expected, name);
			});
		};

		probeStep("two translucent meshes submitted near first blend far to near", k_GreenOverRed, CheckViewProjection(), [this](Renderer3D& renderer)
		{
			renderer.SubmitMesh(m_Quad, WallAt(-10.0f), k_HalfGreen, m_TranslucentA);
			renderer.SubmitMesh(m_Quad, WallAt(-12.0f), k_HalfRed, m_TranslucentA);
		});
		probeStep("a translucent mesh submitted before an opaque one behind it blends over it", glm::vec3(0.5f, 0.5f, 0.0f), CheckViewProjection(), [this](Renderer3D& renderer)
		{
			renderer.SubmitMesh(m_Quad, WallAt(-10.0f), k_HalfGreen, m_TranslucentA);
			renderer.SubmitMesh(m_Quad, WallAt(-12.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
		});
		probeStep("an opaque mesh in front hides a translucent one behind it", glm::vec3(1.0f, 0.0f, 0.0f), CheckViewProjection(), [this](Renderer3D& renderer)
		{
			renderer.SubmitMesh(m_Quad, WallAt(-12.0f), k_HalfGreen, m_TranslucentA);
			renderer.SubmitMesh(m_Quad, WallAt(-10.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
		});
		probeStep("a translucent skinned mesh sorts with the static ones", k_GreenOverRed, CheckViewProjection(), [this](Renderer3D& renderer)
		{
			renderer.SubmitMesh(m_Quad, WallAt(-10.0f), k_HalfGreen, m_TranslucentA);
			renderer.SubmitSkinnedMesh(m_SkinnedQuad, WallAt(-12.0f), m_Palette, k_HalfRed, m_TranslucentA);
		});
		probeStep("an orthographic camera sorts translucent meshes along its view", k_GreenOverRed, glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, -50.0f, 50.0f), [this](Renderer3D& renderer)
		{
			renderer.SubmitMesh(m_Quad, WallAt(-10.0f), k_HalfGreen, m_TranslucentA);
			renderer.SubmitMesh(m_Quad, WallAt(-12.0f), k_HalfRed, m_TranslucentA);
		});
	}

	void Renderer3DBatchTest::DrawTranslucentShowcase()
	{
		const glm::vec4 colors[] = { { 1.0f, 0.2f, 0.2f, m_ShowcaseAlpha }, { 0.2f, 1.0f, 0.2f, m_ShowcaseAlpha }, { 0.2f, 0.4f, 1.0f, m_ShowcaseAlpha } };
		m_CheckRenderer->BeginScene(m_Camera);
		for (int i = 0; i < 3; ++i)
		{
			const glm::vec3 position{ -0.8f + 0.8f * static_cast<float>(i), 0.6f, 2.0f - 1.0f * static_cast<float>(i) };
			m_CheckRenderer->SubmitMesh(m_CheckRenderer->GetBoxMesh(), glm::scale(glm::translate(glm::mat4(1.0f), position), glm::vec3(1.2f)), colors[i], m_TranslucentA);
		}
		m_CheckRenderer->EndScene();
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

		if (m_ShowcaseAlpha >= 0.0f)
			DrawTranslucentShowcase();

		if (Renderer::IsFrameSkipped())
			return;

		if (!m_ChecksDone)
		{
			m_ChecksDone = true;
			RunChecks();
		}
		else if (m_NextCheckStep < m_CheckSteps.size())
		{
			m_CheckSteps[m_NextCheckStep++]();
		}
	}

	void Renderer3DBatchTest::Cleanup()
	{
		m_Alive.reset();
		m_CheckSteps.clear();
		for (Framebuffer*& target : m_CheckTargets)
			DestroyAndDelete(target);
		m_CheckTargets.clear();

		delete m_Quad;
		delete m_SkinnedQuad;
		m_Quad = nullptr;
		m_SkinnedQuad = nullptr;

		if (m_CheckRenderer)
		{
			DestroyAndDelete(m_TranslucentA);
			DestroyAndDelete(m_TranslucentB);
			m_CheckRenderer->Shutdown();
			delete m_CheckRenderer;
			m_CheckRenderer = nullptr;
		}

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
