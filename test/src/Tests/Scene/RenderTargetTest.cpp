#include "RenderTargetTest.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <imgui.h>

#include <algorithm>
#include <climits>
#include <fstream>
#include <functional>
#include <cmath>
#include <cstdlib>
#include <format>

namespace Dingo
{

	namespace
	{
		constexpr uint32_t k_ProbeSize = 256;
		// The probe camera sees 10 world units top to bottom, so a unit is 25.6 px of the probe.
		constexpr float k_ProbeViewHeight = 10.0f;

		struct PixelBox
		{
			int MinX = INT_MAX;
			int MinY = INT_MAX;
			int MaxX = -1;
			int MaxY = -1;
			int Count = 0;

			int Width() const { return Count ? MaxX - MinX + 1 : 0; }
			int Height() const { return Count ? MaxY - MinY + 1 : 0; }
		};

		template<typename Match>
		PixelBox FindPixels(const TexturePixels& pixels, Match match)
		{
			PixelBox box;
			for (uint32_t y = 0; y < pixels.Height; ++y)
			{
				for (uint32_t x = 0; x < pixels.Width; ++x)
				{
					const uint8_t* rgba = &pixels.Data[(static_cast<size_t>(y) * pixels.Width + x) * 4];
					if (!match(rgba[0], rgba[1], rgba[2]))
						continue;

					box.MinX = (std::min)(box.MinX, static_cast<int>(x));
					box.MinY = (std::min)(box.MinY, static_cast<int>(y));
					box.MaxX = (std::max)(box.MaxX, static_cast<int>(x));
					box.MaxY = (std::max)(box.MaxY, static_cast<int>(y));
					box.Count++;
				}
			}
			return box;
		}

		std::vector<uint8_t> ReadImageFile(const std::filesystem::path& path, bool flipVertically)
		{
			uint32_t width = 0, height = 0, channels = 0;
			std::vector<uint8_t> bytes;
			if (const uint8_t* data = FileSystem::ReadImage(path, &width, &height, &channels, flipVertically, true))
			{
				bytes.assign(data, data + static_cast<size_t>(width) * height * 4);
				FileSystem::FreeImage(data);
			}
			return bytes;
		}

		Entity AddSprite(Scene& scene, const char* name, const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
		{
			Entity sprite = scene.CreateEntity(name);
			sprite.GetComponent<TransformComponent>() = TransformComponent(position, size);
			sprite.AddComponent<SpriteRendererComponent>(SpriteRendererComponent(color));
			return sprite;
		}

		// Fullscreen passes: the vertex stage is the engine's own, pulled in by #include.
		constexpr const char* k_FillShaderSource = R"(
#type vertex
#version 450
#include <DingoEngine/Fullscreen.glsl>

#type fragment
#version 450
layout(location = 0) in vec2 v_TexCoord;
layout(std140, binding = 0) uniform FillData { vec4 Color; };
layout(location = 0) out vec4 o_Color;
void main() { o_Color = Color; }
)";

		// Slot 0 of a material without a scene buffer: texture binding 1, sampler binding 2.
		constexpr const char* k_SampleShaderSource = R"(
#type vertex
#version 450
#include <DingoEngine/Fullscreen.glsl>

#type fragment
#version 450
layout(location = 0) in vec2 v_TexCoord;
layout(binding = 1) uniform texture2D u_Source;
layout(binding = 2) uniform sampler u_Sampler;
layout(location = 0) out vec4 o_Color;
void main() { o_Color = texture(sampler2D(u_Source, u_Sampler), v_TexCoord); }
)";

		constexpr float k_CompareReference = 0.999f;
		constexpr const char* k_CompareShaderSource = R"(
#type vertex
#version 450
#include <DingoEngine/Fullscreen.glsl>

#type fragment
#version 450
layout(location = 0) in vec2 v_TexCoord;
layout(binding = 1) uniform texture2D u_Depth;
layout(binding = 2) uniform samplerShadow u_Compare;
layout(location = 0) out vec4 o_Color;
void main() { o_Color = vec4(texture(sampler2DShadow(u_Depth, u_Compare), vec3(v_TexCoord, 0.999)), 0.0, 0.0, 1.0); }
)";

		constexpr uint32_t k_DepthSize = 64;

		bool Near(float a, float b, float tolerance)
		{
			return std::abs(a - b) <= tolerance;
		}

		Framebuffer* MakeColorTarget(const char* name, TextureFormat format, uint32_t width, uint32_t height)
		{
			return Framebuffer::Create(FramebufferParams()
				.SetDebugName(name)
				.SetWidth(static_cast<int32_t>(width))
				.SetHeight(static_cast<int32_t>(height))
				.AddAttachment({ format }));
		}

		Material* MakeFullscreenMaterial(const char* name, Shader* shader, BlendMode blend = BlendMode::Opaque)
		{
			return Material::Create(MaterialParams()
				.SetDebugName(name)
				.SetShader(shader)
				.SetCullMode(CullMode::None)
				.SetDepthTest(false)
				.SetDepthWrite(false)
				.SetBlendMode(blend));
		}

		// Draws a fullscreen material into target, keeping the caller's render target.
		void DrawFullscreen(Material* material, Framebuffer* target, const Viewport* viewport = nullptr)
		{
			Framebuffer* previous = Renderer::GetRenderTarget();
			Renderer::SetRenderTarget(target);
			if (viewport)
				Renderer::SetViewport(*viewport);
			Renderer::Draw(material, 3);
			Renderer::SetRenderTarget(previous);
		}

		Entity AddText(Scene& scene, const char* name, const std::string& text, Font* font, const glm::vec3& position, float size, const glm::vec4& color, float rotation = 0.0f)
		{
			Entity entity = scene.CreateEntity(name);
			TransformComponent& transform = entity.GetComponent<TransformComponent>();
			transform.Position = position;
			transform.Rotation = rotation;
			TextComponent& component = entity.AddComponent<TextComponent>();
			component.Text = text;
			component.Font = font;
			component.Size = size;
			component.Color = color;
			return entity;
		}
	}

	void RenderTargetTest::Initialize()
	{
		Renderer2DTest::Initialize();

		m_Checks.clear();
		m_Time = 0.0f;
		m_ProbeRequested = false;
		m_ProbePixels.clear();
		m_Alive = std::make_shared<int>(0);

		m_TempDirectory = std::filesystem::temp_directory_path() / "DingoRenderTargetTest";
		std::error_code error;
		std::filesystem::create_directories(m_TempDirectory, error);

		m_Font = Font::Create("assets/fonts/ArialBD.ttf");

		auto makeTarget = [](const char* name, int32_t width, int32_t height)
		{
			return Framebuffer::Create(FramebufferParams()
				.SetDebugName(name)
				.SetWidth(width)
				.SetHeight(height)
				.SetEnableDepth(true)
				.AddAttachment({ TextureFormat::RGBA8_UNORM }));
		};
		m_ProbeTarget = makeTarget("RenderTargetTest probe", k_ProbeSize, k_ProbeSize);
		m_TargetA = makeTarget("RenderTargetTest A", 640, 360);
		m_TargetB = makeTarget("RenderTargetTest B", 640, 360);

		Renderer3D& renderer3D = Application::Get().GetRenderer3D();
		m_SceneA = BuildShowScene("Render Target Test: A", { 0.32f, 0.20f, 0.14f, 1.0f }, renderer3D.GetBoxMesh(), { 0.95f, 0.55f, 0.20f, 1.0f }, m_SpinnerA);
		m_SceneB = BuildShowScene("Render Target Test: B", { 0.10f, 0.18f, 0.30f, 1.0f }, renderer3D.GetSphereMesh(), { 0.35f, 0.70f, 1.00f, 1.0f }, m_SpinnerB);
		BuildProbeScene();

		RunImageFileChecks();
		CreateGroundworkResources();
	}

	void RenderTargetTest::CreateGroundworkResources()
	{
		m_FormatTargets = {
			{ TextureFormat::RGBA8_UNORM, "RGBA8" },
			{ TextureFormat::RGBA16F, "RGBA16F" },
			{ TextureFormat::RGBA32F, "RGBA32F" },
			{ TextureFormat::R11G11B10F, "R11G11B10F" },
			{ TextureFormat::R8, "R8" },
			{ TextureFormat::R16F, "R16F" },
			{ TextureFormat::R32F, "R32F" },
		};
		for (FormatTarget& target : m_FormatTargets)
			target.Target = MakeColorTarget(target.Name, target.Format, 4, 4);

		m_DepthSource = Framebuffer::Create(FramebufferParams()
			.SetDebugName("RenderTargetTest depth source")
			.SetWidth(k_DepthSize)
			.SetHeight(k_DepthSize)
			.AddAttachment({ TextureFormat::RGBA8_UNORM })
			.SetDepthSampleable(true));
		m_DepthCopy = MakeColorTarget("RenderTargetTest depth copy", TextureFormat::R32F, k_DepthSize, k_DepthSize);
		m_DepthCompare = MakeColorTarget("RenderTargetTest depth compare", TextureFormat::R16F, k_DepthSize, k_DepthSize);
		m_ResizeTarget = MakeColorTarget("RenderTargetTest resized", TextureFormat::RGBA8_UNORM, 8, 8);
		m_ResizeCopy = MakeColorTarget("RenderTargetTest resize copy", TextureFormat::RGBA8_UNORM, 16, 16);
		m_ViewportTarget = MakeColorTarget("RenderTargetTest viewport", TextureFormat::RGBA8_UNORM, 16, 16);
		m_BlendTarget = MakeColorTarget("RenderTargetTest blend", TextureFormat::RGBA16F, 4, 4);

		m_FillShader = Shader::CreateFromSource("RenderTargetTestFill", k_FillShaderSource);
		m_SampleShader = Shader::CreateFromSource("RenderTargetTestSample", k_SampleShaderSource);
		m_CompareShader = Shader::CreateFromSource("RenderTargetTestCompare", k_CompareShaderSource);
		Check(m_FillShader->IsValid() && m_SampleShader->IsValid() && m_CompareShader->IsValid(),
			"inline shaders compile with #include <DingoEngine/Fullscreen.glsl> as their vertex stage");

		m_FillMaterial = MakeFullscreenMaterial("RenderTargetTestFill", m_FillShader);
		m_AddMaterial = MakeFullscreenMaterial("RenderTargetTestAdd", m_FillShader, BlendMode::Additive);
		m_AddMaterial->SetUniform(glm::vec4(0.25f, 0.125f, 1.5f, 0.0f));

		m_DepthCopyMaterial = MakeFullscreenMaterial("RenderTargetTestDepthCopy", m_SampleShader);
		m_DepthCopyMaterial->SetTexture(0, m_DepthSource->GetDepthAttachment());
		m_DepthCopyMaterial->SetSampler(0, Renderer::GetPointSampler());

		m_CompareSampler = Sampler::Create(SamplerParams().SetCompare(true).SetMinFilter(false).SetMagFilter(false).SetMipFilter(false));
		m_CompareMaterial = MakeFullscreenMaterial("RenderTargetTestCompare", m_CompareShader);
		m_CompareMaterial->SetTexture(0, m_DepthSource->GetDepthAttachment());
		m_CompareMaterial->SetSampler(0, m_CompareSampler);

		m_ResizeCopyMaterial = MakeFullscreenMaterial("RenderTargetTestResizeCopy", m_SampleShader);
		m_ResizeCopyMaterial->SetTexture(0, m_ResizeTarget->GetAttachment(0));
		m_ResizeCopyMaterial->SetSampler(0, Renderer::GetPointSampler());

		Check(m_DepthSource->GetDepthAttachment() && m_DepthSource->GetDepthAttachment()->GetParams().Format == TextureFormat::D32 && !m_ProbeTarget->GetDepthAttachment(),
			"FramebufferParams::SetDepthSampleable gives a D32 depth texture, and a plain depth target gives none");
		Check(m_ResizeTarget->GetWidth() == 8 && m_ResizeTarget->GetHeight() == 8 && m_ResizeTarget->GetAttachment(0)->GetWidth() == 8,
			"a framebuffer reports its size from creation");

		// A neighbour, #included by its path relative to the file, and a common file both stages include.
		const std::filesystem::path part = m_TempDirectory / "include-part.glsl";
		const std::filesystem::path common = m_TempDirectory / "include-common.glsl";
		const std::filesystem::path main = m_TempDirectory / "include-main.glsl";
		{
			std::ofstream(common) << "const vec4 k_Shared = vec4(1.0);\n";
			std::ofstream(part) << "layout(location = 0) out vec4 o_Color;\nvoid main() { o_Color = k_Shared; }\n";
			std::ofstream(main) << "#type vertex\n#version 450\n#include \"include-common.glsl\"\n#include <DingoEngine/Fullscreen.glsl>\n\n"
				"#type fragment\n#version 450\n#include \"include-common.glsl\"\n#include \"include-part.glsl\"\n#include \"include-part.glsl\"\n";
		}
		Shader* included = Shader::CreateFromFile("RenderTargetTestInclude", main);
		const std::vector<std::filesystem::path>& files = included->GetIncludedFiles();
		// Debug builds read <DingoEngine/...> from DE_ENGINE_SHADER_DIR, so the engine include is listed too.
		auto countOf = [&](const std::filesystem::path& wanted)
		{
			return std::ranges::count_if(files, [&](const std::filesystem::path& file)
			{
				std::error_code error;
				return std::filesystem::equivalent(file, wanted, error);
			});
		};
		const auto partCount = countOf(part);
		const auto commonCount = countOf(common);
		Check(included->IsValid() && partCount == 1 && commonCount == 1,
			std::format("a shader file #includes a neighbour by relative path, once per stage, every stage that asks gets it, and each file is listed once for hot-reload ({} and {} of {} file(s))", partCount, commonCount, files.size()));
		DestroyAndDelete(included);
	}

	void RenderTargetTest::ReadBack(Framebuffer* target, std::function<void(const TexturePixels&)> check)
	{
		const std::weak_ptr<int> alive = m_Alive;
		target->GetAttachment(0)->ReadPixels([alive, check = std::move(check)](const TexturePixels& pixels)
		{
			if (!alive.expired())
				check(pixels);
		});
	}

	void RenderTargetTest::RunGroundworkChecks()
	{
		// Every format clears and reads back; float formats keep a value past 1. Clears write alpha 1.
		for (const FormatTarget& target : m_FormatTargets)
		{
			const bool unorm = target.Format == TextureFormat::RGBA8_UNORM || target.Format == TextureFormat::R8;
			const glm::vec3 clear = unorm ? glm::vec3(0.25f, 0.5f, 0.75f) : glm::vec3(2.5f, 0.5f, 0.75f);
			Renderer::Clear(target.Target, glm::vec4(clear, 1.0f));

			const bool singleChannel = target.Format == TextureFormat::R8 || target.Format == TextureFormat::R16F || target.Format == TextureFormat::R32F;
			const char* name = target.Name;
			const TextureFormat format = target.Format;
			ReadBack(target.Target, [this, clear, unorm, singleChannel, name, format](const TexturePixels& pixels)
			{
				const glm::vec4 value = pixels.GetPixel(1, 2);
				const float tolerance = unorm ? 1.5f / 255.0f : 1e-3f;
				const bool red = Near(value.r, clear.r, tolerance);
				const bool rest = singleChannel || (Near(value.g, clear.g, tolerance) && Near(value.b, clear.b, tolerance));
				Check(pixels.Format == format && red && rest,
					std::format("{} clears to ({}, {}, {}) and reads back ({:.3f}, {:.3f}, {:.3f})", name, clear.r, clear.g, clear.b, value.r, value.g, value.b));
			});
		}

		// A box in front of the camera into a target whose depth can be sampled, then the depth copied
		// out through a point sampler and through a comparison sampler.
		Renderer3D& renderer3D = Application::Get().GetRenderer3D();
		Framebuffer* previous = Renderer::GetRenderTarget();
		Renderer::SetRenderTarget(m_DepthSource);
		const glm::mat4 viewProjection = glm::perspective(glm::radians(45.0f), 1.0f, 0.5f, 10.0f) * glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		renderer3D.BeginScene(viewProjection);
		renderer3D.Clear({ 0.0f, 0.0f, 0.0f, 1.0f });
		renderer3D.DrawBox(glm::scale(glm::mat4(1.0f), glm::vec3(1.2f)), { 1.0f, 1.0f, 1.0f, 1.0f });
		renderer3D.EndScene();
		Renderer::SetRenderTarget(previous);

		DrawFullscreen(m_DepthCopyMaterial, m_DepthCopy);
		ReadBack(m_DepthCopy, [this](const TexturePixels& pixels)
		{
			const float center = pixels.GetPixel(k_DepthSize / 2, k_DepthSize / 2).r;
			const float corner = pixels.GetPixel(1, 1).r;
			Check(center > 0.0f && center < 0.99f && corner == 1.0f,
				std::format("a sampleable depth attachment samples as depth: {:.4f} on the box, {:.4f} where it was cleared", center, corner));
		});

		DrawFullscreen(m_CompareMaterial, m_DepthCompare);
		ReadBack(m_DepthCompare, [this](const TexturePixels& pixels)
		{
			const float center = pixels.GetPixel(k_DepthSize / 2, k_DepthSize / 2).r;
			const float corner = pixels.GetPixel(1, 1).r;
			Check(center == 0.0f && corner == 1.0f,
				std::format("a comparison sampler is 1 where the reference {} is less than the depth and 0 where not ({} on the box, {} cleared)", k_CompareReference, center, corner));
		});

		// The material sampling m_ResizeTarget has a cached pass for its texture; after the resize the
		// same Texture holds new contents and the pass must rebind it.
		Renderer::Clear(m_ResizeTarget, { 1.0f, 0.0f, 0.0f, 1.0f });
		DrawFullscreen(m_ResizeCopyMaterial, m_ResizeCopy);
		Texture* before = m_ResizeTarget->GetAttachment(0);
		const uint32_t generation = before->GetGeneration();
		m_ResizeTarget->Resize(32, 24);
		Texture* after = m_ResizeTarget->GetAttachment(0);
		Check(after == before && after->GetGeneration() != generation && after->GetWidth() == 32 && after->GetHeight() == 24 && m_ResizeTarget->GetWidth() == 32,
			"Framebuffer::Resize keeps its attachment's Texture, with a new generation and the new size");
		Renderer::Clear(m_ResizeTarget, { 0.0f, 1.0f, 0.0f, 1.0f });
		DrawFullscreen(m_ResizeCopyMaterial, m_ResizeCopy);
		ReadBack(m_ResizeCopy, [this](const TexturePixels& pixels)
		{
			const glm::vec4 value = pixels.GetPixel(8, 8);
			Check(value.r < 0.1f && value.g > 0.9f,
				std::format("a material sampling a framebuffer resized under it draws the new contents ({:.2f}, {:.2f}, {:.2f})", value.r, value.g, value.b));
		});

		// Only the left half of the target is drawn.
		Renderer::Clear(m_ViewportTarget, { 0.0f, 0.0f, 0.0f, 1.0f });
		m_FillMaterial->SetUniform(glm::vec4(1.0f));
		const Viewport leftHalf{ 0.0f, 0.0f, 8.0f, 16.0f };
		DrawFullscreen(m_FillMaterial, m_ViewportTarget, &leftHalf);
		ReadBack(m_ViewportTarget, [this](const TexturePixels& pixels)
		{
			const float left = pixels.GetPixel(3, 8).r;
			const float right = pixels.GetPixel(12, 8).r;
			Check(left > 0.99f && right < 0.01f, std::format("Renderer::SetViewport limits a draw to its rectangle (left {:.2f}, right {:.2f})", left, right));
		});

		// A target of other formats, likely at the address of the one the material drew into before.
		Framebuffer* freed = MakeColorTarget("RenderTargetTest freed", TextureFormat::RGBA16F, 16, 16);
		DrawFullscreen(m_FillMaterial, freed);
		const uint64_t freedId = freed->GetId();
		DestroyAndDelete(freed);
		m_ReusedTarget = Framebuffer::Create(FramebufferParams()
			.SetDebugName("RenderTargetTest reused")
			.SetWidth(16)
			.SetHeight(16)
			.AddAttachment({ TextureFormat::RGBA8_UNORM })
			.SetEnableDepth(true));
		Check(m_ReusedTarget->GetId() != freedId, "a new framebuffer never takes a freed one's id");
		Renderer::Clear(m_ReusedTarget, { 0.0f, 0.0f, 0.0f, 1.0f });
		DrawFullscreen(m_FillMaterial, m_ReusedTarget);
		ReadBack(m_ReusedTarget, [this](const TexturePixels& pixels)
		{
			const float value = pixels.GetPixel(8, 8).r;
			Check(value > 0.99f, std::format("a material draws into a framebuffer made after the one it last drew into was freed ({:.2f})", value));
		});

		// Two additive draws of (0.25, 0.125, 1.5) over black.
		Renderer::Clear(m_BlendTarget, { 0.0f, 0.0f, 0.0f, 1.0f });
		DrawFullscreen(m_AddMaterial, m_BlendTarget);
		DrawFullscreen(m_AddMaterial, m_BlendTarget);
		ReadBack(m_BlendTarget, [this](const TexturePixels& pixels)
		{
			const glm::vec4 value = pixels.GetPixel(2, 2);
			Check(Near(value.r, 0.5f, 1e-3f) && Near(value.g, 0.25f, 1e-3f) && Near(value.b, 3.0f, 1e-3f),
				std::format("BlendMode::Additive adds, past 1 in RGBA16F ({:.3f}, {:.3f}, {:.3f})", value.r, value.g, value.b));
		});
	}

	void RenderTargetTest::DestroyGroundworkResources()
	{
		for (FormatTarget& target : m_FormatTargets)
			DestroyAndDelete(target.Target);
		m_FormatTargets.clear();

		DestroyAndDelete(m_FillMaterial);
		DestroyAndDelete(m_AddMaterial);
		DestroyAndDelete(m_DepthCopyMaterial);
		DestroyAndDelete(m_CompareMaterial);
		DestroyAndDelete(m_ResizeCopyMaterial);
		DestroyAndDelete(m_CompareSampler);
		DestroyAndDelete(m_FillShader);
		DestroyAndDelete(m_SampleShader);
		DestroyAndDelete(m_CompareShader);
		DestroyAndDelete(m_DepthSource);
		DestroyAndDelete(m_DepthCopy);
		DestroyAndDelete(m_DepthCompare);
		DestroyAndDelete(m_ResizeTarget);
		DestroyAndDelete(m_ResizeCopy);
		DestroyAndDelete(m_ViewportTarget);
		DestroyAndDelete(m_BlendTarget);
		DestroyAndDelete(m_ReusedTarget);
	}

	Scene* RenderTargetTest::BuildShowScene(const char* name, const glm::vec4& clearColor, Mesh* mesh, const glm::vec4& color, Entity& spinner)
	{
		Renderer3D& renderer3D = Application::Get().GetRenderer3D();
		Scene* scene = new Scene(name);
		scene->SetClearColor(clearColor);

		Entity camera = scene->CreateEntity("Camera");
		Transform3DComponent& cameraTransform = camera.AddComponent<Transform3DComponent>();
		cameraTransform.Position = { 0.0f, 1.2f, 3.6f };
		cameraTransform.Rotation = glm::quat(glm::radians(glm::vec3(-15.0f, 0.0f, 0.0f)));
		camera.AddComponent<CameraComponent>().Type = CameraComponent::ProjectionType::Perspective;

		DirectionalLightComponent& sun = scene->CreateEntity("Sun").AddComponent<DirectionalLightComponent>();
		sun.Intensity = 0.8f;
		sun.Ambient = 0.25f;

		Entity floor = scene->CreateEntity("Floor");
		floor.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, -0.65f, 0.0f }, { 6.0f, 0.1f, 6.0f }));
		floor.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer3D.GetBoxMesh(), { 0.45f, 0.48f, 0.44f, 1.0f }));

		spinner = scene->CreateEntity("Spinner");
		spinner.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, 0.05f, 0.0f }, glm::vec3(1.1f)));
		spinner.AddComponent<MeshRendererComponent>(MeshRendererComponent(mesh, color));
		return scene;
	}

	void RenderTargetTest::BuildProbeScene()
	{
		m_Probe = new Scene("Render Target Test: probe");
		m_Probe->SetClearColor({ 0.10f, 0.12f, 0.30f, 1.0f });

		Entity camera = m_Probe->CreateEntity("Camera");
		camera.AddComponent<CameraComponent>().OrthographicSize = k_ProbeViewHeight;

		// A square, read back to measure the aspect, and a marker in the top-left corner.
		AddSprite(*m_Probe, "Square", { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f }, { 1.0f, 0.0f, 0.0f, 1.0f });
		AddSprite(*m_Probe, "Marker", { -4.0f, 4.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f });
		if (!m_Font)
			return;

		// Yellow text under a cover with a higher z, magenta text over a panel with a lower one.
		AddText(*m_Probe, "Hidden", "HIDE", m_Font, { 1.2f, -3.3f, 0.0f }, 0.7f, { 1.0f, 1.0f, 0.0f, 1.0f });
		AddSprite(*m_Probe, "Cover", { 2.5f, -3.0f, 0.5f }, { 3.5f, 1.6f }, { 0.5f, 0.5f, 0.5f, 1.0f });
		AddSprite(*m_Probe, "Panel", { 3.0f, 3.2f, 0.8f }, { 3.2f, 1.4f }, { 0.2f, 0.2f, 0.2f, 1.0f });
		AddText(*m_Probe, "Shown", "TOP", m_Font, { 2.0f, 3.0f, 0.9f }, 0.7f, { 1.0f, 0.0f, 1.0f, 1.0f });

		// Cyan text turned to run up the screen.
		AddText(*m_Probe, "Turned", "IIIIIIIIIIII", m_Font, { -3.5f, -4.6f, 0.2f }, 0.7f, { 0.0f, 1.0f, 1.0f, 1.0f }, 90.0f);
	}

	void RenderTargetTest::RunImageFileChecks()
	{
		std::vector<uint8_t> pattern(3 * 2 * 4);
		for (size_t i = 0; i < pattern.size(); ++i)
			pattern[i] = static_cast<uint8_t>(i * 9 + 7);

		const std::filesystem::path straight = m_TempDirectory / "pattern.png";
		const std::filesystem::path flipped = m_TempDirectory / "pattern-flipped.png";
		const bool written = FileSystem::WriteImage(straight, 3, 2, 4, pattern.data()) && FileSystem::WriteImage(flipped, 3, 2, 4, pattern.data(), true);
		Check(written && ReadImageFile(straight, false) == pattern && ReadImageFile(flipped, true) == pattern,
			"FileSystem::WriteImage writes a PNG ReadImage reads back byte for byte, and its flip undoes ReadImage's");

		m_Container = Texture::CreateFromFile("assets/textures/container.jpg", "Container");
		if (!m_Container)
		{
			Check(false, "container.jpg loads for the SaveToFile check");
			return;
		}

		const std::filesystem::path saved = m_TempDirectory / "container.png";
		const std::weak_ptr<int> alive = m_Alive;
		m_Container->SaveToFile(saved, [this, alive, saved](bool ok)
		{
			if (alive.expired())
				return;

			Check(ok && ReadImageFile(saved, false) == ReadImageFile("assets/textures/container.jpg", false),
				"Texture::SaveToFile writes an image loaded from a file the way the file held it");
		});
	}

	void RenderTargetTest::CheckProbe(const TexturePixels& pixels)
	{
		m_ProbePixels = pixels.Data;
		Check(pixels.Width == k_ProbeSize && pixels.Height == k_ProbeSize && pixels.Data.size() == size_t(k_ProbeSize) * k_ProbeSize * 4,
			std::format("Texture::ReadPixels reads the probe framebuffer back ({}x{})", pixels.Width, pixels.Height));
		if (pixels.Data.empty())
			return;

		const uint8_t* edge = &pixels.Data[(static_cast<size_t>(k_ProbeSize / 2) * k_ProbeSize + (k_ProbeSize - 6)) * 4];
		Check(std::abs(edge[0] - 26) <= 2 && std::abs(edge[1] - 31) <= 2 && std::abs(edge[2] - 77) <= 2,
			std::format("SceneRenderer::Render(scene, target) clears the target to the scene's colour ({}, {}, {})", edge[0], edge[1], edge[2]));

		const PixelBox square = FindPixels(pixels, [](int r, int g, int b) { return r > 200 && g < 60 && b < 60; });
		Check(square.Count > 0 && std::abs(square.Width() - square.Height()) <= 2,
			std::format("a square sprite comes out square: the projection takes the target's aspect, not the window's ({}x{} px)", square.Width(), square.Height()));

		const PixelBox marker = FindPixels(pixels, [](int r, int g, int b) { return g > 200 && r < 60 && b < 60; });
		Check(marker.Count > 0 && marker.MaxY < int(k_ProbeSize / 4) && marker.MaxX < int(k_ProbeSize / 2),
			std::format("row 0 of a render target read back is its top: the top-left marker is in rows {}-{}", marker.MinY, marker.MaxY));

		const PixelBox hidden = FindPixels(pixels, [](int r, int g, int b) { return r > 170 && g > 170 && b < 100; });
		Check(hidden.Count == 0, std::format("text below a sprite with a higher z is hidden ({} px of it show)", hidden.Count));

		const PixelBox shown = FindPixels(pixels, [](int r, int g, int b) { return r > 170 && g < 90 && b > 170; });
		Check(shown.Count > 0, "text with a higher z than a sprite draws over it");

		const PixelBox turned = FindPixels(pixels, [](int r, int g, int b) { return r < 90 && g > 170 && b > 170; });
		Check(turned.Count > 0 && turned.Height() > 2 * turned.Width(),
			std::format("text turned 90 degrees runs up the screen ({}x{} px)", turned.Width(), turned.Height()));
	}

	void RenderTargetTest::Update(float deltaTime)
	{
		m_Time += deltaTime;
		m_SpinnerA.GetComponent<Transform3DComponent>().Rotation = glm::angleAxis(m_Time, glm::normalize(glm::vec3(0.3f, 1.0f, 0.0f)));
		m_SpinnerB.GetComponent<Transform3DComponent>().Rotation = glm::angleAxis(-0.7f * m_Time, glm::normalize(glm::vec3(1.0f, 0.4f, 0.0f)));

		SceneRenderer& sceneRenderer = Application::Get().GetSceneRenderer();
		sceneRenderer.Render(*m_SceneA, m_TargetA);
		sceneRenderer.Render(*m_SceneB, m_TargetB);
		sceneRenderer.Render(*m_Probe, m_ProbeTarget);

		if (!m_ProbeRequested && !Renderer::IsFrameSkipped())
		{
			m_ProbeRequested = true;
			RunGroundworkChecks();

			Texture* probe = m_ProbeTarget->GetAttachment(0);
			const std::weak_ptr<int> alive = m_Alive;
			probe->ReadPixels([this, alive](const TexturePixels& pixels)
			{
				if (!alive.expired())
					CheckProbe(pixels);
			});

			const std::filesystem::path png = m_TempDirectory / "probe.png";
			probe->SaveToFile(png, [this, alive, png](bool saved)
			{
				if (alive.expired())
					return;
				Check(saved && !m_ProbePixels.empty() && ReadImageFile(png, false) == m_ProbePixels,
					"Texture::SaveToFile writes a render target as it reads back, its first row at the top of the PNG");
			});
		}

		// A render target's first row is its top, so the quads take a negative height to show it upright.
		const float fade = 0.5f + 0.5f * std::sin(0.8f * m_Time);
		m_Renderer->BeginScene(m_ProjectionViewMatrix);
		m_Renderer->Clear(m_ClearColor);
		m_Renderer->DrawQuad({ 0.0f, 0.75f }, { 5.0f, -2.8125f }, m_TargetA->GetAttachment(0));
		m_Renderer->DrawQuad({ 0.0f, 0.75f }, { 5.0f, -2.8125f }, m_TargetB->GetAttachment(0), { 1.0f, 1.0f, 1.0f, fade });
		m_Renderer->DrawQuad({ -1.5f, -1.6f }, { 1.7f, -1.7f }, m_ProbeTarget->GetAttachment(0));
		if (m_Font)
		{
			m_Renderer->DrawText("Two scenes rendered into textures, crossfading", m_Font, glm::vec2(-2.5f, 2.25f), 0.16f);
			m_Renderer->DrawText("The probe, read back for the checks", m_Font, glm::vec2(-0.5f, -1.6f), 0.16f);
		}
		m_Renderer->EndScene();
	}

	void RenderTargetTest::Cleanup()
	{
		m_Alive.reset();

		delete m_Probe;
		m_Probe = nullptr;
		delete m_SceneA;
		m_SceneA = nullptr;
		delete m_SceneB;
		m_SceneB = nullptr;

		DestroyAndDelete(m_ProbeTarget);
		DestroyAndDelete(m_TargetA);
		DestroyAndDelete(m_TargetB);
		DestroyAndDelete(m_Container);
		DestroyAndDelete(m_Font);
		DestroyGroundworkResources();

		std::error_code error;
		std::filesystem::remove_all(m_TempDirectory, error);
		m_Checks.clear();

		Renderer2DTest::Cleanup();
	}

	void RenderTargetTest::ImGuiRender()
	{
		Renderer2DTest::ImGuiRender();

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

}
