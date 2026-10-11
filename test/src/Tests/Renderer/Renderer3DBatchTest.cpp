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

	// One per box of the surface checks; the last is the default material's own surface.
	const Dingo::SurfaceOverride k_Surfaces[] = {
		Dingo::SurfaceOverride().SetEmissiveColor({ 1.0f, 0.3f, 0.0f }).SetEmissiveStrength(0.6f),
		Dingo::SurfaceOverride().SetRoughness(0.2f).SetSpecular(1.5f),
		Dingo::SurfaceOverride().SetEmissiveColor({ 0.0f, 0.4f, 1.0f }).SetEmissiveStrength(1.0f).SetRoughness(0.8f).SetSpecular(0.5f),
		Dingo::SurfaceOverride(),
	};
	constexpr uint32_t k_SurfaceCount = sizeof(k_Surfaces) / sizeof(k_Surfaces[0]);
	constexpr uint32_t k_SurfaceTargetSize = 96;

	glm::mat4 SurfaceBoxAt(uint32_t index)
	{
		const float x = (static_cast<float>(index) - 0.5f * static_cast<float>(k_SurfaceCount - 1)) * 1.6f;
		const glm::mat4 placed = glm::translate(glm::mat4(1.0f), { x, 0.0f, -7.0f });
		return glm::rotate(glm::rotate(placed, glm::radians(35.0f), { 0.0f, 1.0f, 0.0f }), glm::radians(25.0f), { 1.0f, 0.0f, 0.0f });
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
		for (const SurfaceOverride& surface : k_Surfaces)
		{
			m_SurfaceMaterials.push_back(m_CheckRenderer->CreateLitMaterial(MaterialParams()
				.SetDebugName("BatchTest_Surface")
				.SetEmissiveColor(surface.EmissiveColor)
				.SetEmissiveStrength(surface.EmissiveStrength)
				.SetRoughness(surface.Roughness)
				.SetSpecular(surface.Specular)));
		}

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
		BuildSurfaceChecks();

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

	Framebuffer* Renderer3DBatchTest::CreateCheckTarget(uint32_t size)
	{
		Framebuffer* target = Framebuffer::Create(FramebufferParams()
			.SetDebugName("BatchTest_Check")
			.SetWidth(static_cast<int32_t>(size))
			.SetHeight(static_cast<int32_t>(size))
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
				check(RenderCheck(CreateCheckTarget(k_CheckTargetSize), CheckViewProjection(), submit));
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
				Framebuffer* target = CreateCheckTarget(k_CheckTargetSize);
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

	void Renderer3DBatchTest::BuildSurfaceChecks()
	{
		m_CheckSteps.push_back([this]
		{
			const Renderer3D::Statistics stats = RenderCheck(CreateCheckTarget(k_CheckTargetSize), CheckViewProjection(), [this](Renderer3D& renderer)
			{
				for (uint32_t i = 0; i < k_SurfaceCount; ++i)
					renderer.SubmitMesh(m_Quad, WallAt(-6.0f - static_cast<float>(i)), glm::vec4(1.0f), nullptr, ShadowCasting::On, 1, 0.0f, &k_Surfaces[i]);
				renderer.SubmitMesh(m_Quad, WallAt(-12.0f), glm::vec4(1.0f));
			});
			Check(stats.DrawCalls == 1 && stats.SurfaceOverrides == k_SurfaceCount && stats.SubmittedMeshes == k_SurfaceCount + 1,
				std::format("meshes of the default material with their own surfaces draw in one draw ({} draw calls, {} overrides, {} submitted)",
					stats.DrawCalls, stats.SurfaceOverrides, stats.SubmittedMeshes));
		});

		// The same boxes three ways: a lit material each, the default material with their surfaces as
		// overrides, and the default material alone (which must differ, or the comparison proves nothing).
		m_CheckSteps.push_back([this]
		{
			auto lights = [](Renderer3D& renderer)
			{
				renderer.SetAmbientLight(glm::vec3(1.0f), 0.3f);
				DirectionalLight sun;
				sun.Direction = { -0.3f, -0.6f, -1.0f };
				sun.Color = { 1.0f, 0.95f, 0.9f };
				sun.Intensity = 1.2f;
				renderer.SubmitLight(sun);
				PointLight lamp;
				lamp.Position = { 0.5f, 1.5f, -4.0f };
				lamp.Color = { 0.6f, 0.8f, 1.0f };
				lamp.Intensity = 2.0f;
				lamp.Range = 8.0f;
				renderer.SubmitLight(lamp);
			};
			const glm::vec4 color{ 0.25f, 0.3f, 0.35f, 1.0f };
			Framebuffer* materials = CreateCheckTarget(k_SurfaceTargetSize);
			Framebuffer* overrides = CreateCheckTarget(k_SurfaceTargetSize);
			Framebuffer* plain = CreateCheckTarget(k_SurfaceTargetSize);
			RenderCheck(materials, CheckViewProjection(), [&](Renderer3D& renderer)
			{
				lights(renderer);
				for (uint32_t i = 0; i < k_SurfaceCount; ++i)
					renderer.SubmitMesh(renderer.GetBoxMesh(), SurfaceBoxAt(i), color, m_SurfaceMaterials[i]);
			});
			const Renderer3D::Statistics stats = RenderCheck(overrides, CheckViewProjection(), [&](Renderer3D& renderer)
			{
				lights(renderer);
				for (uint32_t i = 0; i < k_SurfaceCount; ++i)
					renderer.SubmitMesh(renderer.GetBoxMesh(), SurfaceBoxAt(i), color, nullptr, ShadowCasting::On, 1, 0.0f, &k_Surfaces[i]);
			});
			RenderCheck(plain, CheckViewProjection(), [&](Renderer3D& renderer)
			{
				lights(renderer);
				for (uint32_t i = 0; i < k_SurfaceCount; ++i)
					renderer.SubmitMesh(renderer.GetBoxMesh(), SurfaceBoxAt(i), color);
			});
			Check(stats.DrawCalls == 1, std::format("{} boxes with surface overrides draw in one draw (DrawCalls = {})", k_SurfaceCount, stats.DrawCalls));

			auto images = std::make_shared<std::vector<TexturePixels>>();
			const std::weak_ptr<int> alive = m_Alive;
			for (Framebuffer* target : { materials, overrides, plain })
			{
				target->GetAttachment(0)->ReadPixels([this, alive, images](const TexturePixels& pixels)
				{
					if (alive.expired())
						return;
					images->push_back(pixels);
					if (images->size() < 3)
						return;

					auto compare = [](const TexturePixels& a, const TexturePixels& b, float& worst)
					{
						uint32_t differing = 0;
						worst = 0.0f;
						for (uint32_t y = 0; y < a.Height; ++y)
						{
							for (uint32_t x = 0; x < a.Width; ++x)
							{
								const glm::vec4 delta = glm::abs(a.GetPixel(x, y) - b.GetPixel(x, y));
								const float largest = (std::max)((std::max)(delta.r, delta.g), (std::max)(delta.b, delta.a));
								worst = (std::max)(worst, largest);
								differing += largest > 0.0f ? 1 : 0;
							}
						}
						return differing;
					};
					const std::vector<TexturePixels>& read = *images;
					const bool sized = read[0].Width == k_SurfaceTargetSize && read[1].Width == k_SurfaceTargetSize && read[2].Width == k_SurfaceTargetSize;
					float worst = 0.0f;
					float plainWorst = 0.0f;
					const uint32_t differing = sized ? compare(read[0], read[1], worst) : ~0u;
					const uint32_t plainDiffering = sized ? compare(read[0], read[2], plainWorst) : 0;
					Check(sized && differing == 0,
						std::format("boxes with surface overrides match the same boxes with a lit material each ({} px differ, by up to {:.0f}/255)", differing, worst * 255.0f));
					Check(plainDiffering > 0 && plainWorst > 8.0f / 255.0f,
						std::format("the surfaces change the picture ({} px differ from the default material's, by up to {:.0f}/255)", plainDiffering, plainWorst * 255.0f));
				});
			}
		});

		// Three scenes in one frame. The second outgrows the renderer's 256 records, so its buffer is
		// replaced after the first drew; the third rewrites its record 1 after the second drew.
		m_CheckSteps.push_back([this]
		{
			const glm::vec4 black{ 0.0f, 0.0f, 0.0f, 1.0f };
			auto glowing = [](const glm::vec3& color) { return SurfaceOverride().SetEmissiveColor(color).SetEmissiveStrength(0.5f); };
			const SurfaceOverride blue = glowing({ 0.0f, 0.0f, 1.0f });
			const SurfaceOverride red = glowing({ 1.0f, 0.0f, 0.0f });
			const SurfaceOverride green = glowing({ 0.0f, 1.0f, 0.0f });
			constexpr uint32_t k_Hidden = 299;

			Framebuffer* first = CreateCheckTarget(k_CheckTargetSize);
			Framebuffer* many = CreateCheckTarget(k_CheckTargetSize);
			Framebuffer* last = CreateCheckTarget(k_CheckTargetSize);
			RenderCheck(first, CheckViewProjection(), [&](Renderer3D& renderer)
			{
				renderer.SubmitMesh(m_Quad, WallAt(-6.0f), black, nullptr, ShadowCasting::On, 1, 0.0f, &blue);
			});
			const Renderer3D::Statistics stats = RenderCheck(many, CheckViewProjection(), [&](Renderer3D& renderer)
			{
				renderer.SubmitMesh(m_Quad, WallAt(-6.0f), black, nullptr, ShadowCasting::On, 1, 0.0f, &red);
				for (uint32_t i = 0; i < k_Hidden; ++i)
					renderer.SubmitMesh(m_Quad, WallAt(-7.0f - 0.01f * static_cast<float>(i)), black, nullptr, ShadowCasting::On, 1, 0.0f, &green);
			});
			RenderCheck(last, CheckViewProjection(), [&](Renderer3D& renderer)
			{
				renderer.SubmitMesh(m_Quad, WallAt(-6.0f), black, nullptr, ShadowCasting::On, 1, 0.0f, &green);
			});
			Check(stats.SurfaceOverrides == k_Hidden + 1 && stats.DrawCalls == 1,
				std::format("{} overridden meshes in one scene draw in one draw ({} overrides, {} draw calls)", k_Hidden + 1, stats.SurfaceOverrides, stats.DrawCalls));
			ReadCheck(first, glm::vec3(0.0f, 0.0f, 0.5f), "a scene keeps its surfaces when a later scene of the frame outgrows the record buffer");
			ReadCheck(many, glm::vec3(0.5f, 0.0f, 0.0f), "a scene of 300 surfaces keeps its own when a later scene of the frame rewrites the records");
			ReadCheck(last, glm::vec3(0.0f, 0.5f, 0.0f), "a scene after them reads its own surface");
		});

		const SurfaceOverride glow = SurfaceOverride().SetEmissiveColor({ 0.0f, 1.0f, 0.0f }).SetEmissiveStrength(0.5f);
		m_CheckSteps.push_back([this, glow]
		{
			Framebuffer* target = CreateCheckTarget(k_CheckTargetSize);
			RenderCheck(target, CheckViewProjection(), [this, glow](Renderer3D& renderer)
			{
				renderer.SubmitMesh(m_Quad, WallAt(-10.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.5f), m_TranslucentA, ShadowCasting::On, 1, 0.0f, &glow);
			});
			ReadCheck(target, glm::vec3(0.0f, 0.25f, 0.0f), "a translucent mesh takes its surface override");
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
			for (Material*& material : m_SurfaceMaterials)
				DestroyAndDelete(material);
			m_SurfaceMaterials.clear();
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
