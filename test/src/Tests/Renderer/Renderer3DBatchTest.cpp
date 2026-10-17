#include "Renderer3DBatchTest.h"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <cstdlib>
#include <format>
#include <numeric>
#include <optional>
#include <vector>

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

	// A closed mesh's unlit far side (black at half alpha) drawn before its lit near side (green at
	// half alpha). The near side first, the order the mesh lists them in, leaves (0, 0.25, 0).
	const glm::vec3 k_NearSideOverFarSide{ 0.0f, 0.5f, 0.0f };

	glm::mat4 CheckViewProjection()
	{
		return glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f) *
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	}

	constexpr uint32_t k_StaticBoxCount = 24;
	constexpr uint32_t k_BudgetQuadCount = Dingo::Renderer3D::k_MaxInstancedDrawsPerFrame + 8;
	constexpr uint32_t k_StaticCasterCount = 2;
	constexpr uint32_t k_StaticMeshCount = k_StaticBoxCount + k_StaticCasterCount;
	constexpr uint32_t k_MinShadowPixels = 100;

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

	// Places each copy from its instance record and draws half its colour, unlit. DE_NO_BLOCKS builds
	// the same stages without the instance blocks, which a material with Instancing can't draw.
	constexpr const char* k_InstancedShader = R"(
#type vertex
#version 450

layout(std140, binding = 0) uniform CameraData
{
	mat4 ViewProjection;
	vec4 LightDirection;
	vec4 Ambient;
};

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 0) out vec4 v_Color;

#ifdef DE_NO_BLOCKS
void main()
{
	gl_Position = ViewProjection * vec4(a_Position, 1.0);
	v_Color = vec4(1.0);
}
#else
#include <DingoEngine/Instancing.glsl>

void main()
{
	InstanceRecord instance = GetInstance(uint(gl_InstanceIndex));
	gl_Position = ViewProjection * vec4(InstanceWorldPosition(instance, a_Position), 1.0);
	v_Color = instance.Color;
}
#endif

#type fragment
#version 450

layout(location = 0) in vec4 v_Color;
layout(location = 0) out vec4 o_Color;

void main()
{
	o_Color = vec4(v_Color.rgb * 0.5, 1.0);
}
)";

	uint32_t CountDiffering(const Dingo::TexturePixels& a, const Dingo::TexturePixels& b, float& worst)
	{
		uint32_t differing = 0;
		worst = 0.0f;
		if (a.Width != b.Width || a.Height != b.Height)
			return ~0u;
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
	}

	// A light travelling away from the check camera, alone: a side facing the camera shows its colour,
	// a side facing away is black.
	void LightFromCamera(Dingo::Renderer3D& renderer)
	{
		renderer.SetAmbientLight(glm::vec3(0.0f), 0.0f);
		Dingo::DirectionalLight light;
		light.Direction = { 0.0f, 0.0f, -1.0f };
		renderer.SubmitLight(light);
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

		// The CPU batches' spilling is under test, and five boxes outgrowing a batch would be instanced.
		Renderer3DParams params;
		params.Capabilities.Instancing = false;
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
		for (uint32_t i = 0; i < k_BudgetQuadCount; ++i)
			m_BudgetQuads.push_back(Mesh::Create(vertices, indices));

		// The front and back of a unit box: a quad facing +z at z = 0.5 listed before one facing -z at
		// z = -0.5, both wound counter-clockwise seen from outside.
		const std::vector<MeshVertex> closedVertices = {
			{ { -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f,  1.0f }, { 0.0f, 0.0f } },
			{ {  0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f,  1.0f }, { 1.0f, 0.0f } },
			{ {  0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f,  1.0f }, { 1.0f, 1.0f } },
			{ { -0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f,  1.0f }, { 0.0f, 1.0f } },
			{ {  0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f } },
			{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f } },
			{ { -0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f } },
			{ {  0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f } },
		};
		const std::vector<uint32_t> closedIndices = { 0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4 };
		std::vector<SkinnedMeshVertex> closedSkinVertices;
		for (const MeshVertex& vertex : closedVertices)
			closedSkinVertices.push_back({ vertex.Position, vertex.Normal, vertex.TexCoord, glm::u16vec4(0), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f) });
		m_ClosedQuads = Mesh::Create(closedVertices, closedIndices);
		m_SkinnedClosedQuads = Mesh::CreateSkinned(closedVertices, closedSkinVertices, closedIndices);
		m_IndicesOnly = Mesh::Create({}, { 0, 1, 2 });
		m_Palette.assign(1, glm::mat4(1.0f));

		m_Alive = std::make_shared<int>(0);
		m_NextCheckStep = 0;
		BuildTranslucentChecks();
		BuildSurfaceChecks();
		BuildInstancingChecks();
		BuildStaticChecks();

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
	Renderer3D::Statistics Renderer3DBatchTest::RenderCheck(Framebuffer* target, const glm::mat4& viewProjection, Submit&& submit, Renderer3D* renderer)
	{
		Renderer3D& drawn = renderer ? *renderer : *m_CheckRenderer;
		Framebuffer* previous = Renderer::GetRenderTarget();
		Renderer::SetRenderTarget(target);
		drawn.BeginScene(viewProjection);
		drawn.Clear({ 0.0f, 0.0f, 0.0f, 1.0f });
		drawn.SetAmbientLight(glm::vec3(1.0f), 1.0f);
		submit(drawn);
		drawn.EndScene();
		Renderer::SetRenderTarget(previous);
		return drawn.GetStatistics();
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
			renderer.SubmitMesh(m_IndicesOnly, WallAt(-6.0f), k_HalfGreen, m_TranslucentA);
			renderer.SubmitMesh(m_Quad, WallAt(-9.0f), k_HalfGreen, m_TranslucentA);
			renderer.SubmitMesh(m_IndicesOnly, WallAt(-12.0f), glm::vec4(1.0f));
		}, [this](const Renderer3D::Statistics& stats)
		{
			Check(stats.TranslucentMeshes == 1 && stats.TranslucentDraws == 1 && stats.DrawCalls == 1 && stats.SubmittedMeshes == 1,
				std::format("a mesh with indices but no vertices is skipped, translucent or not ({})", Counts(stats)));
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
			Check(stats.SkinnedDraws == 3 && stats.SkinnedInstances == 1 && stats.TranslucentMeshes == 1 && stats.TranslucentDraws == 2 && stats.DrawCalls == 3,
				std::format("an instance's translucent skinned mesh draws in the translucent pass, one side at a time, and shares its palette ({}, {} instances)", Counts(stats), stats.SkinnedInstances));
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
			renderer.SubmitMesh(m_IndicesOnly, WallAt(-8.0f), k_HalfGreen, m_TranslucentA);
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
		probeStep("a translucent skinned mesh in front of a static one draws after it", k_GreenOverRed, CheckViewProjection(), [this](Renderer3D& renderer)
		{
			renderer.SubmitSkinnedMesh(m_SkinnedQuad, WallAt(-10.0f), m_Palette, k_HalfGreen, m_TranslucentA);
			renderer.SubmitMesh(m_Quad, WallAt(-12.0f), k_HalfRed, m_TranslucentA);
		});
		probeStep("an orthographic camera sorts translucent meshes along its view", k_GreenOverRed, glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, -50.0f, 50.0f), [this](Renderer3D& renderer)
		{
			renderer.SubmitMesh(m_Quad, WallAt(-10.0f), k_HalfGreen, m_TranslucentA);
			renderer.SubmitMesh(m_Quad, WallAt(-12.0f), k_HalfRed, m_TranslucentA);
		});

		const glm::mat4 mirrored = glm::scale(WallAt(-10.0f), { -1.0f, 1.0f, 1.0f });
		probeStep("a closed translucent mesh draws its far side before its near side", k_NearSideOverFarSide, CheckViewProjection(), [this](Renderer3D& renderer)
		{
			LightFromCamera(renderer);
			renderer.SubmitMesh(m_ClosedQuads, WallAt(-10.0f), k_HalfGreen, m_TranslucentA);
		});
		probeStep("a mirrored closed translucent mesh draws its far side before its near side", k_NearSideOverFarSide, CheckViewProjection(), [this, mirrored](Renderer3D& renderer)
		{
			LightFromCamera(renderer);
			renderer.SubmitMesh(m_ClosedQuads, mirrored, k_HalfGreen, m_TranslucentA);
		});
		probeStep("a closed translucent skinned mesh draws its far side before its near side", k_NearSideOverFarSide, CheckViewProjection(), [this](Renderer3D& renderer)
		{
			LightFromCamera(renderer);
			renderer.SubmitSkinnedMesh(m_SkinnedClosedQuads, WallAt(-10.0f), m_Palette, k_HalfGreen, m_TranslucentA);
		});
		probeStep("a mirrored closed translucent skinned mesh draws its far side before its near side", k_NearSideOverFarSide, CheckViewProjection(), [this, mirrored](Renderer3D& renderer)
		{
			LightFromCamera(renderer);
			renderer.SubmitSkinnedMesh(m_SkinnedClosedQuads, mirrored, m_Palette, k_HalfGreen, m_TranslucentA);
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

					auto compare = CountDiffering;
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

	void Renderer3DBatchTest::BuildInstancingChecks()
	{
		m_InstancedShader = Shader::Create(ShaderParams().SetName("BatchTestInstanced").SetSourceCode(k_InstancedShader));
		m_BlocklessShader = Shader::Create(ShaderParams().SetName("BatchTestBlockless").SetSourceCode(k_InstancedShader).AddDefine("DE_NO_BLOCKS"));
		Check(m_InstancedShader && m_InstancedShader->IsValid() && m_BlocklessShader && m_BlocklessShader->IsValid(), "the custom instanced shaders compile");
		if (!m_InstancedShader || !m_InstancedShader->IsValid() || !m_BlocklessShader || !m_BlocklessShader->IsValid())
			return;
		m_InstancedMaterial = Material::Create(MaterialParams().SetDebugName("BatchTest_Instanced").SetShader(m_InstancedShader).SetInstancing(true));
		m_BlocklessMaterial = Material::Create(MaterialParams().SetDebugName("BatchTest_Blockless").SetShader(m_BlocklessShader).SetInstancing(true));

		// Back to front, so the readback sees the last copy, and after as many boxes hidden behind them,
		// so the quads' records start past the boxes': a wrong instance index or base reads another colour.
		auto quads = [this](Renderer3D& renderer, uint32_t count, const glm::vec4& front, Material* material)
		{
			for (uint32_t i = 0; i < count; ++i)
				renderer.SubmitMesh(renderer.GetBoxMesh(), glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, -30.0f - 0.05f * static_cast<float>(i) }), glm::vec4(1.0f), material);
			for (uint32_t i = 0; i < count; ++i)
				renderer.SubmitMesh(m_Quad, WallAt(-6.0f - 0.01f * static_cast<float>(count - 1 - i)), i + 1 == count ? front : glm::vec4(1.0f), material);
		};

		m_CheckSteps.push_back([this, quads]
		{
			const glm::vec4 red{ 1.0f, 0.0f, 0.0f, 1.0f };
			Framebuffer* custom = CreateCheckTarget(k_CheckTargetSize);
			const Renderer3D::Statistics stats = RenderCheck(custom, CheckViewProjection(), [&](Renderer3D& renderer) { quads(renderer, 3, red, m_InstancedMaterial); });
			Check(stats.DrawCalls == 2 && stats.InstancedDraws == 2 && stats.InstancedMeshes == 6 && stats.VertexCount == 0,
				std::format("a material with Instancing draws three quads and three boxes in one instanced draw each ({} draws, {} instanced, {} meshes, {} vertices batched)",
					stats.DrawCalls, stats.InstancedDraws, stats.InstancedMeshes, stats.VertexCount));
			ReadCheck(custom, glm::vec3(0.5f, 0.0f, 0.0f), "a custom instanced shader places each copy from its record");

			const Renderer3D::Statistics blockless = RenderCheck(CreateCheckTarget(k_CheckTargetSize), CheckViewProjection(), [&](Renderer3D& renderer) { quads(renderer, 3, red, m_BlocklessMaterial); });
			Check(blockless.DrawCalls == 0 && blockless.InstancedMeshes == 0,
				std::format("a material with Instancing whose shader lacks the blocks draws nothing ({} draws)", blockless.DrawCalls));

			// Texture slot 4 binds at 10, the DE_InstanceDraw block's binding.
			Material* clashing = Material::Create(MaterialParams().SetDebugName("BatchTest_Clashing").SetShader(m_InstancedShader).SetInstancing(true));
			clashing->SetTexture(4, Renderer::GetWhiteTexture());
			const Renderer3D::Statistics clash = RenderCheck(CreateCheckTarget(k_CheckTargetSize), CheckViewProjection(), [&](Renderer3D& renderer) { quads(renderer, 3, red, clashing); });
			DestroyAndDelete(clashing);
			Check(clash.DrawCalls == 0 && clash.InstancedMeshes == 0,
				std::format("a material with Instancing whose texture takes an instance block's binding draws nothing ({} draws)", clash.DrawCalls));

			m_CheckRenderer->SetInstancingEnabled(false);
			const Renderer3D::Statistics forced = RenderCheck(CreateCheckTarget(k_CheckTargetSize), CheckViewProjection(), [&](Renderer3D& renderer) { quads(renderer, 3, red, m_InstancedMaterial); });
			m_CheckRenderer->SetInstancingEnabled(true);
			Check(forced.InstancedDraws == 2, std::format("a material with Instancing draws instanced with instancing off too ({} instanced draws)", forced.InstancedDraws));
		});

		m_CheckSteps.push_back([this, quads]
		{
			const glm::vec4 green{ 0.0f, 1.0f, 0.0f, 1.0f };
			Framebuffer* instanced = CreateCheckTarget(k_CheckTargetSize);
			Framebuffer* batched = CreateCheckTarget(k_CheckTargetSize);
			const Renderer3D::Statistics stats = RenderCheck(instanced, CheckViewProjection(), [&](Renderer3D& renderer) { quads(renderer, 10, green, nullptr); });
			m_CheckRenderer->SetInstancingEnabled(false);
			const Renderer3D::Statistics cpu = RenderCheck(batched, CheckViewProjection(), [&](Renderer3D& renderer) { quads(renderer, 10, green, nullptr); });
			m_CheckRenderer->SetInstancingEnabled(true);
			Check(stats.DrawCalls == 2 && stats.InstancedDraws == 2 && stats.InstancedMeshes == 20 && stats.VertexCount == 0 && cpu.InstancedMeshes == 0 && cpu.VertexCount == 40 + 240,
				std::format("ten quads and ten boxes with the default material are one instanced draw each, and batched with instancing off ({} instanced in {} draws; off: {} vertices)",
					stats.InstancedMeshes, stats.InstancedDraws, cpu.VertexCount));
			ReadCheck(instanced, glm::vec3(0.0f, 1.0f, 0.0f), "the default material's instanced copies take their records' colours");
			ReadCheck(batched, glm::vec3(0.0f, 1.0f, 0.0f), "the same copies batched on the CPU look the same");
		});

		// Three scenes in one frame. The second outgrows the renderer's first 1024 records, so the buffer
		// is replaced after the first drew; the third rewrites the records after the second drew.
		m_CheckSteps.push_back([this, quads]
		{
			const glm::vec4 blue{ 0.0f, 0.0f, 1.0f, 1.0f };
			const glm::vec4 red{ 1.0f, 0.0f, 0.0f, 1.0f };
			const glm::vec4 green{ 0.0f, 1.0f, 0.0f, 1.0f };
			Framebuffer* first = CreateCheckTarget(k_CheckTargetSize);
			Framebuffer* many = CreateCheckTarget(k_CheckTargetSize);
			Framebuffer* last = CreateCheckTarget(k_CheckTargetSize);
			RenderCheck(first, CheckViewProjection(), [&](Renderer3D& renderer) { quads(renderer, 10, blue, m_InstancedMaterial); });
			const Renderer3D::Statistics stats = RenderCheck(many, CheckViewProjection(), [&](Renderer3D& renderer) { quads(renderer, 1100, red, m_InstancedMaterial); });
			RenderCheck(last, CheckViewProjection(), [&](Renderer3D& renderer) { quads(renderer, 10, green, m_InstancedMaterial); });
			Check(stats.InstancedMeshes == 2200 && stats.InstancedDraws == 2, std::format("1100 quads and 1100 boxes are one instanced draw each ({} meshes, {} draws)", stats.InstancedMeshes, stats.InstancedDraws));
			ReadCheck(first, glm::vec3(0.0f, 0.0f, 0.5f), "a scene keeps its instances when a later scene of the frame outgrows the record buffer");
			ReadCheck(many, glm::vec3(0.5f, 0.0f, 0.0f), "a scene of 1100 instances keeps its own when a later scene of the frame rewrites the records");
			ReadCheck(last, glm::vec3(0.0f, 0.5f, 0.0f), "a scene after them reads its own instances");
		});

		// Distinct meshes behind the camera fill the budget before culling drops them, so the boxes in
		// view after them get none of it whether culling is on or off.
		auto pastBudget = [this](uint32_t quads, Material* material, bool cast)
		{
			return [this, quads, material, cast](Renderer3D& renderer)
			{
				if (cast)
				{
					DirectionalLight light;
					light.Direction = glm::normalize(glm::vec3(0.3f, -1.0f, -0.4f));
					light.CastShadows = true;
					renderer.SubmitLight(light);
				}
				for (uint32_t q = 0; q < quads; ++q)
				{
					for (uint32_t i = 0; i < 8; ++i)
						renderer.SubmitMesh(m_BudgetQuads[q], glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, 10.0f }), glm::vec4(1.0f), material);
				}
				for (uint32_t i = 0; i < 8; ++i)
					renderer.SubmitMesh(renderer.GetBoxMesh(), glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, -30.0f - 0.05f * static_cast<float>(i) }), glm::vec4(1.0f), material);
			};
		};
		auto culledAndNot = [this](std::function<void(Renderer3D&)> submit, std::function<void(const Renderer3D::Statistics&, bool)> check)
		{
			for (const bool culling : { true, false })
			{
				m_CheckSteps.push_back([this, submit, check, culling]
				{
					m_CheckRenderer->SetCullingEnabled(culling);
					const Renderer3D::Statistics stats = RenderCheck(CreateCheckTarget(k_CheckTargetSize), CheckViewProjection(), submit);
					m_CheckRenderer->SetCullingEnabled(true);
					check(stats, culling);
				});
			}
		};

		const uint32_t budget = Renderer3D::k_MaxInstancedDrawsPerFrame;
		culledAndNot(pastBudget(k_BudgetQuadCount, nullptr, false), [this, budget](const Renderer3D::Statistics& stats, bool culling)
		{
			const uint32_t instanced = culling ? 0 : budget * 8;
			const uint32_t batched = 8 * 24 + (culling ? 0 : (k_BudgetQuadCount - budget) * 8 * 4);
			Check(stats.InstancedMeshes == instanced && stats.VertexCount == batched,
				std::format("past the instanced budget, culled copies keep their place in it, so the boxes in view after them batch on the CPU (culling {}: {} instanced, {} vertices batched; expected {} and {})",
					culling ? "on" : "off", stats.InstancedMeshes, stats.VertexCount, instanced, batched));
		});
		culledAndNot(pastBudget(k_BudgetQuadCount, m_InstancedMaterial, false), [this, budget](const Renderer3D::Statistics& stats, bool culling)
		{
			const uint32_t dropped = 8 + (culling ? 0 : (k_BudgetQuadCount - budget) * 8);
			Check(stats.DroppedInstancedMeshes == dropped && stats.InstancedMeshes == (culling ? 0 : budget * 8),
				std::format("past the instanced budget, a material with Instancing skips the boxes in view after culled copies too (culling {}: {} dropped, expected {}; {} instanced)",
					culling ? "on" : "off", stats.DroppedInstancedMeshes, dropped, stats.InstancedMeshes));
		});
		// A caster books three draws: lit, shadow, and the shadow-only group its culled copies may join.
		const uint32_t casters = budget / 3;
		culledAndNot(pastBudget(casters, nullptr, true), [this, casters](const Renderer3D::Statistics& stats, bool culling)
		{
			Check(stats.VertexCount == 8 * 24 && stats.InstancedMeshes <= casters * 8 && (culling || (stats.InstancedMeshes == casters * 8 && stats.InstancedShadowDraws > 0)),
				std::format("{} casting meshes take the budget a caster needs, so the casting boxes after them batch on the CPU (culling {}: {} vertices batched, {} instanced, {} instanced shadow draws)",
					casters, culling ? "on" : "off", stats.VertexCount, stats.InstancedMeshes, stats.InstancedShadowDraws));
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

	void Renderer3DBatchTest::ReadAll(std::vector<Framebuffer*> targets, std::function<void(const std::vector<TexturePixels>&)> done)
	{
		auto images = std::make_shared<std::vector<TexturePixels>>(targets.size());
		auto remaining = std::make_shared<size_t>(targets.size());
		const std::weak_ptr<int> alive = m_Alive;
		for (size_t i = 0; i < targets.size(); ++i)
		{
			targets[i]->GetAttachment(0)->ReadPixels([alive, images, remaining, i, done](const TexturePixels& pixels)
			{
				if (alive.expired())
					return;
				(*images)[i] = pixels;
				if (--*remaining == 0)
					done(*images);
			});
		}
	}

	void Renderer3DBatchTest::BuildStaticChecks()
	{
		constexpr uint32_t k_StaticTargetSize = 128;
		constexpr float k_CellSize = 2.0f;

		// Boxes over several cells of a 2-unit grid, every third with its own surface, one only casting
		// and one casting nothing, in front of a wall that takes their shadows; a sun that casts.
		// Two more reach the picture only as shadows: a box beside the view, culled by the camera but
		// not by the sun's cascades, and a pillar behind the camera and high up the sun's way, which
		// alone sets how far toward the sun the cascades reach.
		struct Placed
		{
			glm::mat4 Transform;
			glm::vec4 Color;
			ShadowCasting Shadows;
			std::optional<SurfaceOverride> Surface;
		};
		auto staticScene = std::make_shared<std::vector<Placed>>();
		for (uint32_t i = 0; i < k_StaticBoxCount; ++i)
		{
			const float x = static_cast<float>(i % 6) * 1.3f - 3.2f;
			const float y = static_cast<float>(i / 6) * 1.1f - 1.8f;
			const float z = -6.0f - static_cast<float>((i * 7) % 5) * 0.9f;
			const glm::mat4 placed = glm::rotate(glm::translate(glm::mat4(1.0f), { x, y, z }), glm::radians(17.0f * static_cast<float>(i)), glm::normalize(glm::vec3(1.0f, 2.0f, 0.5f)));
			Placed box{ glm::scale(placed, glm::vec3(0.7f + 0.03f * static_cast<float>(i % 4))), glm::vec4(0.2f + 0.03f * static_cast<float>(i), 0.5f, 0.8f - 0.02f * static_cast<float>(i), 1.0f), ShadowCasting::On, std::nullopt };
			if (i % 3 == 0)
				box.Surface = k_Surfaces[i % k_SurfaceCount];
			if (i == 7)
				box.Shadows = ShadowCasting::ShadowsOnly;
			if (i == 11)
				box.Shadows = ShadowCasting::Off;
			staticScene->push_back(box);
		}
		const glm::vec4 casterColor(0.7f, 0.7f, 0.2f, 1.0f);
		staticScene->push_back({ glm::rotate(glm::scale(glm::translate(glm::mat4(1.0f), { 7.5f, 4.0f, -4.0f }), glm::vec3(1.6f)), glm::radians(30.0f), { 0.0f, 1.0f, 0.0f }), casterColor, ShadowCasting::On, std::nullopt });
		staticScene->push_back({ glm::scale(glm::translate(glm::mat4(1.0f), { 5.0f, 10.0f, 5.0f }), { 0.8f, 8.0f, 0.8f }), casterColor, ShadowCasting::On, std::nullopt });

		auto lights = [](Renderer3D& renderer)
		{
			renderer.SetAmbientLight(glm::vec3(1.0f), 0.3f);
			DirectionalLight sun;
			sun.Direction = { -0.3f, -0.6f, -1.0f };
			sun.Color = { 1.0f, 0.95f, 0.9f };
			sun.Intensity = 1.2f;
			sun.CastShadows = true;
			renderer.SubmitLight(sun);
			PointLight lamp;
			lamp.Position = { 0.5f, 3.5f, -5.0f };
			lamp.Color = { 0.6f, 0.8f, 1.0f };
			lamp.Intensity = 2.0f;
			lamp.Range = 10.0f;
			lamp.CastShadows = true;
			renderer.SubmitLight(lamp);
		};
		// Dynamic meshes with surfaces beside the batch's, so their records sit after the batch's.
		auto dynamicMeshes = [](Renderer3D& renderer)
		{
			renderer.SubmitMesh(renderer.GetBoxMesh(), glm::translate(glm::mat4(1.0f), { 2.5f, 2.2f, -7.0f }), glm::vec4(0.9f, 0.9f, 0.2f, 1.0f), nullptr, ShadowCasting::On, 1, 0.0f, &k_Surfaces[0]);
			renderer.SubmitMesh(renderer.GetSphereMesh(), glm::translate(glm::mat4(1.0f), { -2.5f, 2.2f, -7.0f }), glm::vec4(0.3f, 0.9f, 0.4f, 1.0f), nullptr, ShadowCasting::On, 1, 0.0f, &k_Surfaces[2]);
			renderer.SubmitMesh(renderer.GetBoxMesh(), WallAt(-12.0f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
		};
		auto describe = [](const Placed& box, Mesh* mesh)
		{
			StaticMesh desc = StaticMesh().SetMesh(mesh).SetTransform(box.Transform).SetColor(box.Color).SetShadows(box.Shadows);
			desc.Surface = box.Surface;
			return desc;
		};

		m_CheckSteps.push_back([this, staticScene, lights, dynamicMeshes, describe, k_StaticTargetSize, k_CellSize]
		{
			Framebuffer* direct = CreateCheckTarget(k_StaticTargetSize);
			Framebuffer* baked = CreateCheckTarget(k_StaticTargetSize);
			Framebuffer* updated = CreateCheckTarget(k_StaticTargetSize);
			Framebuffer* withoutCasters = CreateCheckTarget(k_StaticTargetSize);
			Framebuffer* sideOnly = CreateCheckTarget(k_StaticTargetSize);
			Framebuffer* pillarOnly = CreateCheckTarget(k_StaticTargetSize);
			// Baked meshes are never instanced, so the reference is the CPU batches.
			m_CheckRenderer->SetInstancingEnabled(false);
			RenderCheck(direct, CheckViewProjection(), [&](Renderer3D& renderer)
			{
				lights(renderer);
				for (const Placed& box : *staticScene)
					renderer.SubmitMesh(renderer.GetBoxMesh(), box.Transform, box.Color, nullptr, box.Shadows, 1, 0.0f, box.Surface ? &*box.Surface : nullptr);
				dynamicMeshes(renderer);
			});
			auto directWith = [&](Framebuffer* target, std::vector<size_t> indices)
			{
				RenderCheck(target, CheckViewProjection(), [&](Renderer3D& renderer)
				{
					lights(renderer);
					for (const size_t index : indices)
					{
						const Placed& box = (*staticScene)[index];
						renderer.SubmitMesh(renderer.GetBoxMesh(), box.Transform, box.Color, nullptr, box.Shadows, 1, 0.0f, box.Surface ? &*box.Surface : nullptr);
					}
					dynamicMeshes(renderer);
				});
			};
			std::vector<size_t> boxes(k_StaticBoxCount);
			std::iota(boxes.begin(), boxes.end(), size_t(0));
			std::vector<size_t> withSide = boxes;
			withSide.push_back(k_StaticBoxCount);
			std::vector<size_t> withPillar = boxes;
			withPillar.push_back(k_StaticBoxCount + 1);
			directWith(withoutCasters, boxes);
			directWith(sideOnly, withSide);
			directWith(pillarOnly, withPillar);
			m_CheckRenderer->SetInstancingEnabled(true);

			m_StaticBatch = m_CheckRenderer->CreateStaticBatch(StaticBatchParams().SetCellSize(k_CellSize));
			m_StaticIds.clear();
			for (const Placed& box : *staticScene)
				m_StaticIds.push_back(m_StaticBatch->Add(describe(box, m_CheckRenderer->GetBoxMesh())));
			const Renderer3D::Statistics bakedStats = RenderCheck(baked, CheckViewProjection(), [&](Renderer3D& renderer)
			{
				lights(renderer);
				renderer.SubmitStaticBatch(*m_StaticBatch);
				dynamicMeshes(renderer);
			});
			const uint32_t cells = m_StaticBatch->GetCellCount();

			StaticMesh recoloured = *m_StaticBatch->Get(m_StaticIds[5]);
			recoloured.Color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			m_StaticBatch->Update(m_StaticIds[5], recoloured);
			const Renderer3D::Statistics updatedStats = RenderCheck(updated, CheckViewProjection(), [&](Renderer3D& renderer)
			{
				lights(renderer);
				renderer.SubmitStaticBatch(*m_StaticBatch);
				dynamicMeshes(renderer);
			});

			Check(bakedStats.StaticMeshes == k_StaticMeshCount && bakedStats.StaticCellsBaked == cells && cells > 4 && bakedStats.ShadowViews > 4 && bakedStats.SurfaceOverrides == 10,
				std::format("a static batch bakes {} boxes into {} cells and submits them with their surfaces ({} static, {} baked, {} shadow views, {} overrides)",
					k_StaticMeshCount, cells, bakedStats.StaticMeshes, bakedStats.StaticCellsBaked, bakedStats.ShadowViews, bakedStats.SurfaceOverrides));
			Check(bakedStats.CulledMeshes >= k_StaticCasterCount,
				std::format("the box beside the view and the pillar behind the camera are culled by the camera ({} culled, {} culled from the shadows)", bakedStats.CulledMeshes, bakedStats.CulledCasters));
			Check(updatedStats.StaticCellsBaked == 1, std::format("changing one mesh's colour rebakes its cell alone ({} baked)", updatedStats.StaticCellsBaked));
			const std::optional<glm::ivec3> recolouredCell = m_StaticBatch->GetMeshCell(m_StaticIds[5]);
			Check(m_StaticBatch->GetLastBakedCells().size() == 1 && recolouredCell && m_StaticBatch->GetLastBakedCells()[0] == *recolouredCell,
				std::format("the one cell a changed mesh rebaked is the cell the batch reports for it ({} baked cells listed)", m_StaticBatch->GetLastBakedCells().size()));
			Check(m_StaticBatch->GetCellSize() == k_CellSize && m_CheckRenderer->CreateStaticBatch(StaticBatchParams().SetCellSize(-1.0f))->GetCellSize() == 32.0f,
				"a batch reports its cell size, and 32 for a bad one");
			ReadAll({ direct, baked, updated, withoutCasters, sideOnly, pillarOnly }, [this](const std::vector<TexturePixels>& images)
			{
				float worst = 0.0f;
				float changedWorst = 0.0f;
				float shadowWorst = 0.0f;
				const uint32_t differing = CountDiffering(images[0], images[1], worst);
				const uint32_t changed = CountDiffering(images[1], images[2], changedWorst);
				const uint32_t shadowBoth = CountDiffering(images[0], images[3], shadowWorst);
				const uint32_t shadowSide = CountDiffering(images[4], images[3], shadowWorst);
				const uint32_t shadowPillar = CountDiffering(images[5], images[3], shadowWorst);
				Check(differing == 0,
					std::format("a lit, shadowed scene through a static batch matches it through SubmitMesh ({} px differ, by up to {:.0f}/255)", differing, worst * 255.0f));
				Check(changed > 0 && changed != ~0u, std::format("a rebaked cell draws its mesh's new colour ({} px changed)", changed));
				Check(shadowSide >= k_MinShadowPixels && shadowSide != ~0u && shadowPillar >= k_MinShadowPixels && shadowPillar != ~0u && shadowBoth >= shadowSide && shadowBoth >= shadowPillar,
					std::format("the camera-culled box and the pillar each shadow the picture ({} and {} px, {} with both)", shadowSide, shadowPillar, shadowBoth));
			});
		});

		m_CheckSteps.push_back([this, lights, k_StaticTargetSize]
		{
			if (!m_StaticBatch)
				return;
			const Renderer3D::Statistics again = RenderCheck(CreateCheckTarget(k_StaticTargetSize), CheckViewProjection(), [&](Renderer3D& renderer)
			{
				lights(renderer);
				renderer.SubmitStaticBatch(*m_StaticBatch);
			});
			Check(again.StaticCellsBaked == 0 && again.StaticMeshes == k_StaticMeshCount && again.VertexCount == 0,
				std::format("a batch submitted again unchanged bakes nothing and transforms no vertex ({} baked, {} static, {} vertices)", again.StaticCellsBaked, again.StaticMeshes, again.VertexCount));
			Check(m_StaticBatch->GetLastBakedCells().empty(), std::format("an unchanged submission leaves no baked cells listed ({} listed)", m_StaticBatch->GetLastBakedCells().size()));

			m_StaticBatch->Remove(m_StaticIds[0]);
			Check(!m_StaticBatch->GetMeshCell(m_StaticIds[0]) && !m_StaticBatch->GetMeshCell(k_InvalidStaticMesh), "a removed or invalid id has no cell");
			const StaticMeshId distant = m_StaticBatch->Add(StaticMesh().SetMesh(m_CheckRenderer->GetBoxMesh()).SetTransform(glm::translate(glm::mat4(1.0f), { 200.0f, 0.0f, -7.0f })));
			const StaticMeshId glass = m_StaticBatch->Add(StaticMesh().SetMesh(m_Quad).SetTransform(WallAt(-5.5f)).SetColor(k_HalfGreen).SetMaterial(m_TranslucentA));
			const Renderer3D::Statistics changed = RenderCheck(CreateCheckTarget(k_StaticTargetSize), CheckViewProjection(), [&](Renderer3D& renderer)
			{
				lights(renderer);
				renderer.SubmitStaticBatch(*m_StaticBatch);
			});
			Check(changed.StaticMeshes == k_StaticMeshCount && changed.StaticCellsBaked == 2 && changed.CulledMeshes >= k_StaticCasterCount + 1,
				std::format("removing a mesh and adding one far off screen rebakes two cells, and the far one is culled ({} static, {} baked, {} culled)",
					changed.StaticMeshes, changed.StaticCellsBaked, changed.CulledMeshes));
			const std::optional<glm::ivec3> distantCell = m_StaticBatch->GetMeshCell(distant);
			const std::vector<glm::ivec3>& lastBaked = m_StaticBatch->GetLastBakedCells();
			Check(distantCell && *distantCell == glm::ivec3(100, 0, -4) && lastBaked.size() == 2 && std::find(lastBaked.begin(), lastBaked.end(), *distantCell) != lastBaked.end() && !m_StaticBatch->GetMeshCell(glass),
				std::format("the far mesh's cell is among the two rebaked, and the translucent mesh drawn loose has no cell ({} listed)", lastBaked.size()));
			Check(changed.TranslucentMeshes == 1 && changed.SubmittedMeshes == k_StaticMeshCount + 1,
				std::format("a translucent mesh in a static batch draws in the translucent pass ({} translucent, {} submitted)", changed.TranslucentMeshes, changed.SubmittedMeshes));
			m_StaticBatch->Remove(distant);
			m_StaticBatch->Remove(glass);

			const std::shared_ptr<StaticBatch> other = m_CheckRenderer->CreateStaticBatch();
			Check(m_CheckRenderer->OwnsStaticBatch(*other) && !m_BatchRenderer->OwnsStaticBatch(*other), "a static batch belongs to the renderer that made it");
			m_ForeignBatch = m_BatchRenderer->CreateStaticBatch();
			m_ForeignBatch->Add(StaticMesh().SetMesh(m_Quad).SetTransform(glm::scale(glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, -6.0f }), glm::vec3(4.0f))).SetColor({ 0.2f, 0.2f, 0.25f, 1.0f }));
			const Renderer3D::Statistics foreign = RenderCheck(CreateCheckTarget(k_StaticTargetSize), CheckViewProjection(), [&](Renderer3D& renderer)
			{
				renderer.SubmitStaticBatch(*m_ForeignBatch);
			});
			Check(foreign.StaticMeshes == 0 && foreign.DrawCalls == 0, std::format("another renderer's static batch draws nothing ({} draws)", foreign.DrawCalls));
		});

		// A hot reload gives the duck's mesh a new id with the same vertices: its cell rebakes and the
		// picture stays.
		m_CheckSteps.push_back([this, lights, k_StaticTargetSize]
		{
			m_Duck = Model::LoadFromFile("models/Duck/Duck.gltf");
			Check(m_Duck && m_Duck->GetSubMeshCount() > 0, "the duck loads for the static batch's reload check");
			if (!m_Duck || m_Duck->GetSubMeshCount() == 0)
				return;

			const Mesh* first = m_Duck->GetSubMeshes().front().Mesh;
			const glm::vec3 size = first->GetBoundsMax() - first->GetBoundsMin();
			const float scale = 3.0f / (std::max)({ size.x, size.y, size.z, 1e-3f });
			const glm::mat4 placed = glm::translate(glm::scale(glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, -6.0f }), glm::vec3(scale)),
				-0.5f * (first->GetBoundsMin() + first->GetBoundsMax()));
			const std::shared_ptr<StaticBatch> ducks = m_CheckRenderer->CreateStaticBatch();
			for (const SubMesh& submesh : m_Duck->GetSubMeshes())
				ducks->Add(StaticMesh().SetMesh(submesh.Mesh).SetTransform(placed).SetColor({ 0.9f, 0.8f, 0.1f, 1.0f }));
			Framebuffer* before = CreateCheckTarget(k_StaticTargetSize);
			Framebuffer* after = CreateCheckTarget(k_StaticTargetSize);
			RenderCheck(before, CheckViewProjection(), [&](Renderer3D& renderer) { lights(renderer); renderer.SubmitStaticBatch(*ducks); });
			const uint64_t bakes = ducks->GetBakeCount();
			const bool reloaded = m_Duck->Reload();
			const Renderer3D::Statistics stats = RenderCheck(after, CheckViewProjection(), [&](Renderer3D& renderer) { lights(renderer); renderer.SubmitStaticBatch(*ducks); });
			Check(reloaded && stats.StaticCellsBaked >= 1 && ducks->GetBakeCount() > bakes,
				std::format("a hot-reloaded mesh rebakes its cell ({} baked)", stats.StaticCellsBaked));
			ReadAll({ before, after }, [this](const std::vector<TexturePixels>& images)
			{
				float worst = 0.0f;
				const uint32_t differing = CountDiffering(images[0], images[1], worst);
				uint32_t lit = 0;
				for (uint32_t y = 0; y < images[0].Height; ++y)
					for (uint32_t x = 0; x < images[0].Width; ++x)
						lit += images[0].GetPixel(x, y).r > 0.05f ? 1 : 0;
				Check(differing == 0 && lit > 50, std::format("the rebaked duck draws as before ({} px differ, {} px of duck)", differing, lit));
			});
		});

		// m_BatchRenderer's 40-vertex batches: five 24-vertex boxes in one cell (none left of x = 0, which
		// would be another cell) cannot share a range.
		m_CheckSteps.push_back([this]
		{
			Framebuffer* direct = CreateCheckTarget(64);
			Framebuffer* baked = CreateCheckTarget(64);
			auto place = [](uint32_t i) { return glm::scale(glm::translate(glm::mat4(1.0f), { static_cast<float>(i) * 0.8f, 0.0f, -6.0f }), glm::vec3(0.7f)); };
			auto tint = [](uint32_t i) { return glm::vec4(0.2f + 0.15f * static_cast<float>(i), 0.9f - 0.15f * static_cast<float>(i), 0.5f, 1.0f); };

			const std::shared_ptr<StaticBatch> boxes = m_BatchRenderer->CreateStaticBatch();
			for (uint32_t i = 0; i < k_BoxCount; ++i)
				boxes->Add(StaticMesh().SetMesh(m_BatchRenderer->GetBoxMesh()).SetTransform(place(i)).SetColor(tint(i)));
			const Renderer3D::Statistics directStats = RenderCheck(direct, CheckViewProjection(), [&](Renderer3D& renderer)
			{
				for (uint32_t i = 0; i < k_BoxCount; ++i)
					renderer.SubmitMesh(renderer.GetBoxMesh(), place(i), tint(i));
			}, m_BatchRenderer);
			const Renderer3D::Statistics bakedStats = RenderCheck(baked, CheckViewProjection(), [&](Renderer3D& renderer) { renderer.SubmitStaticBatch(*boxes); }, m_BatchRenderer);
			Check(boxes->GetCellCount() == 1 && bakedStats.StaticMeshes == k_BoxCount && bakedStats.StaticDraws == k_BoxCount && directStats.DrawCalls == k_BoxCount,
				std::format("five boxes baked into one cell split into one range each at 40 vertices ({} cells, {} meshes, {} static draws, {} direct draws)",
					boxes->GetCellCount(), bakedStats.StaticMeshes, bakedStats.StaticDraws, directStats.DrawCalls));
			ReadAll({ direct, baked }, [this](const std::vector<TexturePixels>& images)
			{
				float worst = 0.0f;
				const uint32_t differing = CountDiffering(images[0], images[1], worst);
				uint32_t lit = 0;
				for (uint32_t y = 0; y < images[0].Height; ++y)
					for (uint32_t x = 0; x < images[0].Width; ++x)
						lit += images[0].GetPixel(x, y).g > 0.05f ? 1 : 0;
				Check(differing == 0 && lit > 20,
					std::format("the split ranges draw as the same boxes through SubmitMesh ({} px differ, by up to {:.0f}/255, {} px of boxes)", differing, worst * 255.0f, lit));
			});
		});

		// A bake between two scenes of a frame moves the scene's own records along the object buffer.
		m_CheckSteps.push_back([this]
		{
			m_StaticBatch.reset();
			const glm::vec4 black{ 0.0f, 0.0f, 0.0f, 1.0f };
			auto glowing = [](const glm::vec3& color) { return SurfaceOverride().SetEmissiveColor(color).SetEmissiveStrength(0.5f); };
			const SurfaceOverride blue = glowing({ 0.0f, 0.0f, 1.0f });
			const SurfaceOverride red = glowing({ 1.0f, 0.0f, 0.0f });

			const std::shared_ptr<StaticBatch> walls = m_CheckRenderer->CreateStaticBatch();
			const StaticMeshId back = walls->Add(StaticMesh().SetMesh(m_Quad).SetTransform(WallAt(-8.0f)).SetColor(black));
			Framebuffer* first = CreateCheckTarget(k_CheckTargetSize);
			Framebuffer* second = CreateCheckTarget(k_CheckTargetSize);
			Framebuffer* third = CreateCheckTarget(k_CheckTargetSize);
			RenderCheck(first, CheckViewProjection(), [&](Renderer3D& renderer)
			{
				renderer.SubmitStaticBatch(*walls);
				renderer.SubmitMesh(m_Quad, WallAt(-6.0f), black, nullptr, ShadowCasting::On, 1, 0.0f, &blue);
			});
			walls->Update(back, StaticMesh().SetMesh(m_Quad).SetTransform(WallAt(-8.0f)).SetColor(black).SetSurface(glowing({ 0.0f, 1.0f, 0.0f })));
			const Renderer3D::Statistics stats = RenderCheck(second, CheckViewProjection(), [&](Renderer3D& renderer)
			{
				renderer.SubmitStaticBatch(*walls);
				renderer.SubmitMesh(m_Quad, WallAt(-6.0f), black, nullptr, ShadowCasting::On, 1, 0.0f, &red);
			});
			RenderCheck(third, CheckViewProjection(), [&](Renderer3D& renderer) { renderer.SubmitStaticBatch(*walls); });
			Check(stats.StaticCellsBaked == 1 && stats.SurfaceOverrides == 2, std::format("a static mesh given a surface between scenes rebakes ({} baked, {} overrides)", stats.StaticCellsBaked, stats.SurfaceOverrides));
			ReadCheck(first, glm::vec3(0.0f, 0.0f, 0.5f), "a scene keeps its own surface when a later scene's bake adds a static record");
			ReadCheck(second, glm::vec3(0.5f, 0.0f, 0.0f), "a scene's own surface reads past the static records added before it");
			ReadCheck(third, glm::vec3(0.0f, 0.5f, 0.0f), "a baked surface override draws");
		});
	}

	void Renderer3DBatchTest::Update(float deltaTime)
	{
		Renderer::Clear(m_ClearColor);

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
		// Baked here, so its cells are still alive when this renderer shuts down before the batch is released.
		if (m_ForeignBatch)
			m_BatchRenderer->SubmitStaticBatch(*m_ForeignBatch);

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
		m_StaticBatch.reset();
		m_StaticIds.clear();
		delete m_Duck;
		m_Duck = nullptr;
		for (Framebuffer*& target : m_CheckTargets)
			DestroyAndDelete(target);
		m_CheckTargets.clear();

		delete m_Quad;
		delete m_SkinnedQuad;
		delete m_ClosedQuads;
		delete m_SkinnedClosedQuads;
		delete m_IndicesOnly;
		for (Mesh* quad : m_BudgetQuads)
			delete quad;
		m_BudgetQuads.clear();
		m_Quad = nullptr;
		m_SkinnedQuad = nullptr;
		m_ClosedQuads = nullptr;
		m_SkinnedClosedQuads = nullptr;
		m_IndicesOnly = nullptr;

		if (m_CheckRenderer)
		{
			DestroyAndDelete(m_TranslucentA);
			DestroyAndDelete(m_TranslucentB);
			for (Material*& material : m_SurfaceMaterials)
				DestroyAndDelete(material);
			m_SurfaceMaterials.clear();
			DestroyAndDelete(m_InstancedMaterial);
			DestroyAndDelete(m_BlocklessMaterial);
			DestroyAndDelete(m_InstancedShader);
			DestroyAndDelete(m_BlocklessShader);
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
		m_ForeignBatch.reset();
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
