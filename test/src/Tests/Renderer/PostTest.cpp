#include "PostTest.h"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <format>

namespace Dingo
{

	namespace
	{
		constexpr const char* k_GradientShaderSource = R"(
#type vertex
#version 450
#include <DingoEngine/Fullscreen.glsl>

#type fragment
#version 450
layout(location = 0) in vec2 v_TexCoord;
layout(std140, binding = 0) uniform GradientData { vec4 Base; }; // rgb = colour, w = the value at the right edge
layout(location = 0) out vec4 o_Color;
void main() { o_Color = vec4(Base.rgb * (v_TexCoord.x * Base.w), 1.0); }
)";

		// A square of Value, HalfSize across from the centre in UV, on black.
		constexpr const char* k_SpotShaderSource = R"(
#type vertex
#version 450
#include <DingoEngine/Fullscreen.glsl>

#type fragment
#version 450
layout(location = 0) in vec2 v_TexCoord;
layout(std140, binding = 0) uniform SpotData { vec4 Spot; }; // x = half size in UV, y = value
layout(location = 0) out vec4 o_Color;
void main()
{
	bool inside = all(lessThan(abs(v_TexCoord - 0.5), vec2(Spot.x)));
	o_Color = vec4(vec3(inside ? Spot.y : 0.0), 1.0);
}
)";

		constexpr const char* k_OperatorNames[] = { "None", "Soft", "ACES", "Neutral" };
		constexpr uint32_t k_SpotSize = 64;
		constexpr float k_SpotHalfSize = 4.0f / 64.0f; // an 8 x 8 px square
		constexpr uint32_t k_SceneWidth = 320;
		constexpr uint32_t k_SceneHeight = 240;

		Framebuffer* MakeTarget(const char* name, uint32_t width, uint32_t height, bool depth)
		{
			return Framebuffer::Create(FramebufferParams()
				.SetDebugName(name)
				.SetWidth(static_cast<int32_t>(width))
				.SetHeight(static_cast<int32_t>(height))
				.SetEnableDepth(depth)
				.AddAttachment({ TextureFormat::RGBA8_UNORM }));
		}

		PostProcessSettings WithOperator(PostProcessSettings settings, ToneMapOperator op)
		{
			settings.Enabled = true;
			settings.Tone.Operator = op;
			return settings;
		}

		const glm::vec3 k_RoomFloorOpen{ 1.8f, 0.0f, 1.8f };
		const glm::vec3 k_RoomWallFar{ -3.0f, 1.5f, -1.9f };
		const glm::vec3 k_RoomCrease{ 1.5f, 0.0f, -1.8f };
		const glm::vec3 k_FloatingBox{ -1.5f, 1.5f, -0.9f };
		const glm::vec4 k_WallColor{ 0.8f, 0.75f, 0.7f, 1.0f };

		glm::ivec2 ToPixel(const glm::mat4& viewProjection, const glm::vec3& point, uint32_t width, uint32_t height)
		{
			const glm::vec4 clip = viewProjection * glm::vec4(point, 1.0f);
			const glm::vec2 ndc = glm::vec2(clip) / clip.w;
			return { static_cast<int>((ndc.x * 0.5f + 0.5f) * width), static_cast<int>((0.5f - ndc.y * 0.5f) * height) };
		}

		glm::vec4 PixelAt(const std::vector<uint8_t>& data, uint32_t width, glm::ivec2 pixel)
		{
			const uint32_t height = static_cast<uint32_t>(data.size() / (static_cast<size_t>(width) * 4));
			pixel = glm::clamp(pixel, glm::ivec2(0), glm::ivec2(static_cast<int>(width) - 1, static_cast<int>(height) - 1));
			const size_t offset = (static_cast<size_t>(pixel.y) * width + pixel.x) * 4;
			return glm::vec4(data[offset], data[offset + 1], data[offset + 2], data[offset + 3]) / 255.0f;
		}

		// The value the gradient holds at the centre of column x.
		float GradientValue(uint32_t x, uint32_t width, float max)
		{
			return (static_cast<float>(x) + 0.5f) / static_cast<float>(width) * max;
		}
	}

	void PostTest::Initialize()
	{
		m_Checks.clear();
		m_ChecksDone = false;
		m_Alive = std::make_shared<int>(0);

		m_Camera = PerspectiveCamera(45.0f, m_AspectRatio, 0.1f, 100.0f);
		m_Camera.SetPosition({ 0.0f, 3.0f, 9.0f });
		m_Camera.SetTarget({ 0.0f, 0.8f, 0.0f });

		m_Settings = PostProcessSettings();
		m_Settings.Enabled = true;

		const ApplicationCommandLineArgs& args = Application::Get().GetCommandLineArgs();
		if (auto op = args.Get("tonemap"))
		{
			for (int i = 0; i < k_OperatorCount; ++i)
			{
				std::string name = k_OperatorNames[i];
				std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				if (*op == name)
					m_Settings.Tone.Operator = static_cast<ToneMapOperator>(i);
			}
		}
		if (auto exposure = args.Get("exposure"))
			m_Settings.Tone.Exposure = std::strtof(std::string(*exposure).c_str(), nullptr);
		if (args.Get("post-off"))
			m_Settings.Enabled = false;
		if (auto mode = args.Get("post"); (mode && *mode == "bloom") || args.Get("bloom"))
			m_Settings.Bloom.Enabled = true;
		if (auto mode = args.Get("post"); mode && *mode == "ao")
		{
			m_ShowRoom = true;
			m_Settings.AmbientOcclusion.Enabled = true;
		}

		m_LampMaterial = Application::Get().GetRenderer3D().CreateLitMaterial(MaterialParams()
			.SetDebugName("PostTestLamp")
			.SetEmissiveColor({ 1.0f, 0.75f, 0.4f })
			.SetEmissiveStrength(3.0f));

		for (int i = 0; i < k_OperatorCount; ++i)
			m_Strips[i] = MakeTarget(std::format("PostTest strip {}", k_OperatorNames[i]).c_str(), k_StripWidth, 1, false);
		m_HueStrip = MakeTarget("PostTest hue strip", k_StripWidth, 1, false);

		m_GradientShader = Shader::CreateFromSource("PostTestGradient", k_GradientShaderSource);
		m_GradientMaterial = Material::Create(MaterialParams()
			.SetDebugName("PostTestGradient")
			.SetShader(m_GradientShader)
			.SetCullMode(CullMode::None)
			.SetDepthTest(false)
			.SetDepthWrite(false)
			.SetBlendMode(BlendMode::Opaque));

		m_FlatBloom = MakeTarget("PostTest flat bloom", k_StripWidth, 1, false);
		m_FlatPlain = MakeTarget("PostTest flat plain", k_StripWidth, 1, false);
		m_SpotBloom = MakeTarget("PostTest spot bloom", k_SpotSize, k_SpotSize, false);
		m_SpotPlain = MakeTarget("PostTest spot plain", k_SpotSize, k_SpotSize, false);
		m_SpotShader = Shader::CreateFromSource("PostTestSpot", k_SpotShaderSource);
		m_SpotMaterial = Material::Create(MaterialParams()
			.SetDebugName("PostTestSpot")
			.SetShader(m_SpotShader)
			.SetCullMode(CullMode::None)
			.SetDepthTest(false)
			.SetDepthWrite(false)
			.SetBlendMode(BlendMode::Opaque));
		m_SpotMaterial->SetUniform(glm::vec4(k_SpotHalfSize, 8.0f, 0.0f, 0.0f));

		m_SceneOff = MakeTarget("PostTest scene off", k_SceneWidth, k_SceneHeight, true);
		m_SceneNone = MakeTarget("PostTest scene None", k_SceneWidth, k_SceneHeight, true);
		m_SceneDisabled = MakeTarget("PostTest scene disabled", k_SceneWidth, k_SceneHeight, true);
		m_RoomPlain = MakeTarget("PostTest room plain", k_SceneWidth, k_SceneHeight, true);
		m_RoomOccluded = MakeTarget("PostTest room occluded", k_SceneWidth, k_SceneHeight, true);
	}

	PerspectiveCamera PostTest::RoomCamera(float aspect) const
	{
		PerspectiveCamera camera(45.0f, aspect, 0.1f, 100.0f);
		camera.SetPosition({ 0.5f, 2.0f, 5.0f });
		camera.SetTarget({ 0.0f, 0.8f, -1.0f });
		return camera;
	}

	void PostTest::DrawRoom(Renderer3D& renderer) const
	{
		DirectionalLight sun;
		sun.Direction = { -0.3f, -1.0f, -0.6f };
		sun.Intensity = 0.45f;
		renderer.SubmitLight(sun);
		renderer.SetAmbientLight(glm::vec3(1.0f), 0.35f);

		auto box = [](const glm::vec3& center, const glm::vec3& size) { return glm::scale(glm::translate(glm::mat4(1.0f), center), size); };
		renderer.SubmitMesh(renderer.GetBoxMesh(), box({ 0.0f, -0.05f, 0.0f }, { 8.0f, 0.1f, 6.0f }), { 0.8f, 0.8f, 0.8f, 1.0f });
		renderer.SubmitMesh(renderer.GetBoxMesh(), box({ 0.0f, 2.0f, -2.0f }, { 8.0f, 4.0f, 0.2f }), k_WallColor);
		renderer.SubmitMesh(renderer.GetBoxMesh(), box({ 0.5f, 0.4f, 0.0f }, glm::vec3(0.8f)), { 0.6f, 0.7f, 0.9f, 1.0f });
		renderer.SubmitMesh(renderer.GetBoxMesh(), box(k_FloatingBox, glm::vec3(0.8f)), { 0.9f, 0.6f, 0.5f, 1.0f });
	}

	void PostTest::DrawRoomInto(Framebuffer* target, const PostProcessSettings& settings)
	{
		Renderer3D& renderer = Application::Get().GetRenderer3D();
		Framebuffer* previous = Renderer::GetRenderTarget();
		if (target)
			Renderer::SetRenderTarget(target);

		const float aspect = target ? static_cast<float>(target->GetWidth()) / static_cast<float>(target->GetHeight()) : m_AspectRatio;
		const PerspectiveCamera camera = RoomCamera(aspect);
		PostProcessStack& post = Renderer::GetPostProcessStack();
		post.Begin(settings, camera.GetProjectionMatrix());
		renderer.BeginScene(camera);
		renderer.Clear(m_ClearColor);
		DrawRoom(renderer);
		renderer.EndScene();
		post.End();

		if (target)
			Renderer::SetRenderTarget(previous);
	}

	void PostTest::DrawScene(Renderer3D& renderer) const
	{
		DirectionalLight sun;
		sun.Direction = { -0.3f, -1.0f, -0.5f };
		sun.Intensity = 0.7f;
		renderer.SubmitLight(sun);
		renderer.SetAmbientLight({ 0.6f, 0.7f, 1.0f }, 0.15f);

		const glm::vec3 lightColors[] = { { 1.0f, 0.5f, 0.2f }, { 0.3f, 0.6f, 1.0f }, { 0.4f, 1.0f, 0.4f } };
		for (int i = 0; i < 3; ++i)
		{
			PointLight light;
			light.Position = { (static_cast<float>(i) - 1.0f) * 3.0f, 1.2f, 1.2f };
			light.Color = lightColors[i];
			light.Intensity = 3.5f;
			light.Range = 5.0f;
			renderer.SubmitLight(light);
		}

		renderer.SubmitMesh(renderer.GetBoxMesh(), glm::scale(glm::translate(glm::mat4(1.0f), { 0.0f, -0.05f, 0.0f }), { 14.0f, 0.1f, 8.0f }), { 0.8f, 0.8f, 0.8f, 1.0f });
		for (int i = 0; i < 5; ++i)
		{
			const glm::mat4 pillar = glm::scale(glm::translate(glm::mat4(1.0f), { (static_cast<float>(i) - 2.0f) * 2.0f, 1.0f, -0.5f }), { 0.6f, 2.0f, 0.6f });
			renderer.SubmitMesh(renderer.GetBoxMesh(), pillar, { 0.9f, 0.85f, 0.8f, 1.0f });
		}
		renderer.SubmitMesh(renderer.GetSphereMesh(), glm::scale(glm::translate(glm::mat4(1.0f), { 0.0f, 2.6f, 1.5f }), glm::vec3(0.5f)), { 1.0f, 1.0f, 1.0f, 1.0f }, m_LampMaterial);
	}

	void PostTest::DrawSceneInto(Framebuffer* target, const PostProcessSettings& settings)
	{
		Renderer3D& renderer = Application::Get().GetRenderer3D();
		PostProcessStack& post = Renderer::GetPostProcessStack();

		Framebuffer* previous = Renderer::GetRenderTarget();
		if (target)
			Renderer::SetRenderTarget(target);

		PerspectiveCamera camera = m_Camera;
		if (target)
			camera.SetAspectRatio(static_cast<float>(target->GetWidth()) / static_cast<float>(target->GetHeight()));

		post.Begin(settings, camera.GetProjectionMatrix());
		renderer.BeginScene(camera);
		renderer.Clear(m_ClearColor);
		DrawScene(renderer);
		renderer.EndScene();
		post.End();

		if (target)
			Renderer::SetRenderTarget(previous);
	}

	void PostTest::DrawGradient(Framebuffer* target, const glm::vec3& base, float max, const PostProcessSettings& settings)
	{
		PostProcessStack& post = Renderer::GetPostProcessStack();
		Framebuffer* previous = Renderer::GetRenderTarget();
		Renderer::SetRenderTarget(target);

		post.Begin(settings);
		m_GradientMaterial->SetUniform(glm::vec4(base, max));
		Renderer::Draw(m_GradientMaterial, 3);
		post.End();

		Renderer::SetRenderTarget(previous);
	}

	void PostTest::DrawSpot(Framebuffer* target, const PostProcessSettings& settings)
	{
		PostProcessStack& post = Renderer::GetPostProcessStack();
		Framebuffer* previous = Renderer::GetRenderTarget();
		Renderer::SetRenderTarget(target);

		post.Begin(settings);
		Renderer::Draw(m_SpotMaterial, 3);
		post.End();

		Renderer::SetRenderTarget(previous);
	}

	void PostTest::RunChecks()
	{
		const std::weak_ptr<int> alive = m_Alive;
		auto readBack = [alive](Framebuffer* target, std::function<void(const TexturePixels&)> check)
		{
			target->GetAttachment(0)->ReadPixels([alive, check = std::move(check)](const TexturePixels& pixels)
			{
				if (!alive.expired())
					check(pixels);
			});
		};

		const PostProcessSettings defaults = PostProcessSettings();
		const float knee = defaults.Tone.Knee;
		const float white = defaults.Tone.WhitePoint;

		for (int op = 0; op < k_OperatorCount; ++op)
		{
			readBack(m_Strips[op], [this, op, knee, white](const TexturePixels& pixels)
			{
				bool rising = pixels.Width == k_StripWidth;
				for (uint32_t x = 1; x < pixels.Width && rising; ++x)
					rising = pixels.GetPixel(x, 0).r >= pixels.GetPixel(x - 1, 0).r;
				Check(rising, std::format("the {} curve rises from left to right across a 0..{} gradient", k_OperatorNames[op], k_GradientMax));

				float worst = 0.0f;
				bool reachesWhite = true;
				for (uint32_t x = 0; x < pixels.Width; ++x)
				{
					const float input = GradientValue(x, pixels.Width, k_GradientMax);
					const float output = pixels.GetPixel(x, 0).r;
					if (op == static_cast<int>(ToneMapOperator::None))
						worst = std::max(worst, std::abs(output - std::min(input, 1.0f)));
					else if (op == static_cast<int>(ToneMapOperator::Soft))
					{
						if (input <= knee)
							worst = std::max(worst, std::abs(output - input));
						else if (input >= white)
							reachesWhite = reachesWhite && output == 1.0f;
					}
				}

				if (op == static_cast<int>(ToneMapOperator::None))
					Check(worst <= 1.01f / 255.0f, std::format("None is the identity up to 1 and clips past it (worst {:.4f})", worst));
				else if (op == static_cast<int>(ToneMapOperator::Soft))
				{
					Check(worst <= 1.01f / 255.0f, std::format("Soft is the identity up to its knee {} (worst {:.4f}, at most 1/255)", knee, worst));
					Check(reachesWhite, std::format("Soft maps its white point {} and anything brighter to 1", white));
				}
			});
		}

		readBack(m_HueStrip, [this, knee](const TexturePixels& pixels)
		{
			float worst = 0.0f;
			for (uint32_t x = 0; x < pixels.Width; ++x)
			{
				const glm::vec4 color = pixels.GetPixel(x, 0);
				const float input = GradientValue(x, pixels.Width, k_GradientMax);
				if (input <= knee || color.r < 0.5f)
					continue;
				worst = std::max({ worst, std::abs(color.g / color.r - 0.5f), std::abs(color.b / color.r - 0.25f) });
			}
			Check(worst < 0.015f, std::format("Soft keeps an overbright (1, 0.5, 0.25) gradient's hue (worst ratio error {:.4f})", worst));
		});

		readBack(m_FlatPlain, [this](const TexturePixels& pixels) { m_FlatPlainPixels = pixels.Data; });
		readBack(m_FlatBloom, [this](const TexturePixels& pixels)
		{
			Check(!pixels.Data.empty() && pixels.Data == m_FlatPlainPixels, "bloom adds nothing to a gradient that stays inside 0..1 (the default threshold is 1)");
		});
		readBack(m_SpotPlain, [this](const TexturePixels& pixels)
		{
			const float outside = pixels.GetPixel(k_SpotSize / 2 + 12, k_SpotSize / 2).r;
			const float inside = pixels.GetPixel(k_SpotSize / 2, k_SpotSize / 2).r;
			Check(outside == 0.0f && inside == 1.0f, std::format("without bloom a square at 8 stays inside its edge ({:.3f} 8 px outside, {:.3f} inside)", outside, inside));
		});
		readBack(m_SpotBloom, [this](const TexturePixels& pixels)
		{
			const float near = pixels.GetPixel(k_SpotSize / 2 + 8, k_SpotSize / 2).r;
			const float far = pixels.GetPixel(k_SpotSize / 2 + 12, k_SpotSize / 2).r;
			const float left = pixels.GetPixel(k_SpotSize / 2 - 13, k_SpotSize / 2).r;
			Check(near > 0.0f && far > 0.0f && near >= far && std::abs(far - left) <= 2.0f / 255.0f,
				std::format("with bloom a square at 8 glows past its edge, fading with distance and alike on both sides ({:.3f} 4 px out, {:.3f} 8 px out, {:.3f} on the left)", near, far, left));
		});

		readBack(m_SceneOff, [this](const TexturePixels& pixels) { m_SceneOffPixels = pixels.Data; });
		readBack(m_SceneNone, [this](const TexturePixels& pixels)
		{
			int worst = 0;
			uint32_t differing = 0;
			const size_t count = std::min(pixels.Data.size(), m_SceneOffPixels.size());
			for (size_t i = 0; i < count; ++i)
			{
				const int difference = std::abs(static_cast<int>(pixels.Data[i]) - static_cast<int>(m_SceneOffPixels[i]));
				worst = std::max(worst, difference);
				differing += difference > 0 ? 1 : 0;
			}
			Check(count > 0 && count == m_SceneOffPixels.size() && worst <= 1,
				std::format("the scene through the chain with None is within 1/255 of the scene without it (worst {}/255, {} channels differ)", worst, differing));
		});
		readBack(m_SceneDisabled, [this](const TexturePixels& pixels)
		{
			Check(!pixels.Data.empty() && pixels.Data == m_SceneOffPixels, "a Begin with the chain disabled draws exactly what drawing without it does");
		});

		const glm::mat4 room = RoomCamera(static_cast<float>(k_SceneWidth) / static_cast<float>(k_SceneHeight)).GetViewProjectionMatrix();
		const glm::ivec2 open = ToPixel(room, k_RoomFloorOpen, k_SceneWidth, k_SceneHeight);
		const glm::ivec2 crease = ToPixel(room, k_RoomCrease, k_SceneWidth, k_SceneHeight);
		const glm::ivec2 boxCenter = ToPixel(room, k_FloatingBox, k_SceneWidth, k_SceneHeight);
		const int wallStart = ToPixel(room, k_RoomWallFar, k_SceneWidth, k_SceneHeight).x;
		readBack(m_RoomPlain, [this](const TexturePixels& pixels) { m_RoomPlainPixels = pixels.Data; });
		readBack(m_RoomOccluded, [this, open, crease, boxCenter, wallStart](const TexturePixels& pixels)
		{
			if (pixels.Data.size() != m_RoomPlainPixels.size() || pixels.Data.empty())
			{
				Check(false, "the ambient occlusion room reads back");
				return;
			}

			const glm::vec4 openOn = PixelAt(pixels.Data, k_SceneWidth, open);
			const glm::vec4 openOff = PixelAt(m_RoomPlainPixels, k_SceneWidth, open);
			Check(glm::all(glm::lessThanEqual(glm::abs(openOn - openOff), glm::vec4(2.01f / 255.0f))),
				std::format("ambient occlusion leaves the open floor alone ({:.3f} against {:.3f})", openOn.g, openOff.g));

			const glm::vec4 creaseOn = PixelAt(pixels.Data, k_SceneWidth, crease);
			const glm::vec4 creaseOff = PixelAt(m_RoomPlainPixels, k_SceneWidth, crease);
			Check(creaseOn.g < 0.95f * creaseOff.g, std::format("ambient occlusion darkens the crease where floor meets wall ({:.3f} against {:.3f})", creaseOn.g, creaseOff.g));

			// Along the floating box's row, every pixel that shows the wall in the plain frame, right up to
			// the box's silhouette.
			const glm::vec4 wallReference = PixelAt(m_RoomPlainPixels, k_SceneWidth, { wallStart, boxCenter.y });
			int wallPixels = 0, darkened = 0;
			for (int x = wallStart; x < boxCenter.x; ++x)
			{
				const glm::vec4 off = PixelAt(m_RoomPlainPixels, k_SceneWidth, { x, boxCenter.y });
				if (glm::any(glm::greaterThan(glm::abs(off - wallReference), glm::vec4(4.0f / 255.0f))))
					break;
				++wallPixels;
				const glm::vec4 on = PixelAt(pixels.Data, k_SceneWidth, { x, boxCenter.y });
				darkened += on.g < off.g - 2.01f / 255.0f ? 1 : 0;
			}
			Check(wallPixels > 10 && darkened == 0,
				std::format("no halo: the wall beside a box floating before it isn't darkened, up to its silhouette ({} of {} pixels darker)", darkened, wallPixels));
		});
	}

	void PostTest::Update(float deltaTime)
	{
		const bool runChecks = !m_ChecksDone && !Renderer::IsFrameSkipped();
		if (runChecks)
		{
			m_ChecksDone = true;

			PostProcessSettings disabled;
			disabled.Enabled = false;
			Framebuffer* previous = Renderer::GetRenderTarget();
			Renderer::SetRenderTarget(m_SceneOff);
			{
				Renderer3D& renderer = Application::Get().GetRenderer3D();
				PerspectiveCamera camera = m_Camera;
				camera.SetAspectRatio(static_cast<float>(k_SceneWidth) / static_cast<float>(k_SceneHeight));
				renderer.BeginScene(camera);
				renderer.Clear(m_ClearColor);
				DrawScene(renderer);
				renderer.EndScene();
			}
			Renderer::SetRenderTarget(previous);
			DrawSceneInto(m_SceneNone, WithOperator(PostProcessSettings(), ToneMapOperator::None));
			DrawSceneInto(m_SceneDisabled, disabled);

			PostProcessSettings plain = WithOperator(PostProcessSettings(), ToneMapOperator::Soft);
			PostProcessSettings bloom = plain;
			bloom.Bloom.Enabled = true;
			DrawGradient(m_FlatPlain, glm::vec3(1.0f), 1.0f, plain);
			DrawGradient(m_FlatBloom, glm::vec3(1.0f), 1.0f, bloom);
			DrawSpot(m_SpotPlain, plain);
			DrawSpot(m_SpotBloom, bloom);

			PostProcessSettings roomPlain = WithOperator(PostProcessSettings(), ToneMapOperator::None);
			PostProcessSettings roomOccluded = roomPlain;
			roomOccluded.AmbientOcclusion.Enabled = true;
			DrawRoomInto(m_RoomPlain, roomPlain);
			DrawRoomInto(m_RoomOccluded, roomOccluded);
		}

		if (m_ShowRoom)
			DrawRoomInto(nullptr, m_Settings);
		else
			DrawSceneInto(nullptr, m_Settings);

		// The strips take the panel's exposure, knee and white point; the checks above them run on the
		// defaults, on the first frame.
		const PostProcessSettings stripSettings = runChecks ? PostProcessSettings() : m_Settings;
		for (int op = 0; op < k_OperatorCount; ++op)
			DrawGradient(m_Strips[op], glm::vec3(1.0f), k_GradientMax, WithOperator(stripSettings, static_cast<ToneMapOperator>(op)));
		DrawGradient(m_HueStrip, { 1.0f, 0.5f, 0.25f }, k_GradientMax, WithOperator(stripSettings, ToneMapOperator::Soft));

		if (runChecks)
			RunChecks();

		Renderer2D& renderer2D = Application::Get().GetRenderer2D();
		renderer2D.BeginScene(glm::ortho(-m_AspectRatio, m_AspectRatio, -1.0f, 1.0f, -1.0f, 1.0f));
		for (int i = 0; i <= k_OperatorCount; ++i)
		{
			Texture* strip = (i < k_OperatorCount ? m_Strips[i] : m_HueStrip)->GetAttachment(0);
			renderer2D.DrawQuad({ 0.0f, -0.62f - 0.075f * static_cast<float>(i) }, { 1.6f * m_AspectRatio, 0.06f }, strip);
		}
		renderer2D.EndScene();
	}

	void PostTest::Cleanup()
	{
		m_Alive.reset();

		for (Framebuffer*& strip : m_Strips)
			DestroyAndDelete(strip);
		DestroyAndDelete(m_HueStrip);
		DestroyAndDelete(m_SceneOff);
		DestroyAndDelete(m_SceneNone);
		DestroyAndDelete(m_SceneDisabled);
		DestroyAndDelete(m_RoomPlain);
		DestroyAndDelete(m_RoomOccluded);
		m_RoomPlainPixels.clear();
		DestroyAndDelete(m_GradientMaterial);
		DestroyAndDelete(m_GradientShader);
		DestroyAndDelete(m_FlatBloom);
		DestroyAndDelete(m_FlatPlain);
		DestroyAndDelete(m_SpotBloom);
		DestroyAndDelete(m_SpotPlain);
		DestroyAndDelete(m_SpotMaterial);
		DestroyAndDelete(m_SpotShader);
		m_FlatPlainPixels.clear();
		delete m_LampMaterial;
		m_LampMaterial = nullptr;
		m_SceneOffPixels.clear();
		m_Checks.clear();
	}

	void PostTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
		m_Camera.SetAspectRatio(m_AspectRatio);
	}

	void PostTest::ImGuiRender()
	{
		ImGui::Checkbox("Post chain", &m_Settings.Enabled);

		int op = static_cast<int>(m_Settings.Tone.Operator);
		ImGui::Combo("Operator", &op, k_OperatorNames, k_OperatorCount);
		m_Settings.Tone.Operator = static_cast<ToneMapOperator>(op);
		ImGui::SliderFloat("Exposure (EV)", &m_Settings.Tone.Exposure, -4.0f, 4.0f);
		ImGui::SliderFloat("Knee", &m_Settings.Tone.Knee, 0.0f, 0.99f);
		ImGui::SliderFloat("White point", &m_Settings.Tone.WhitePoint, 1.0f, 16.0f);
		ImGui::Checkbox("Bloom", &m_Settings.Bloom.Enabled);
		ImGui::SliderFloat("Bloom intensity", &m_Settings.Bloom.Intensity, 0.0f, 2.0f);
		ImGui::SliderFloat("Bloom threshold", &m_Settings.Bloom.Threshold, 0.0f, 4.0f);
		ImGui::SliderFloat("Bloom knee", &m_Settings.Bloom.Knee, 0.0f, 1.0f);
		ImGui::SliderFloat("Bloom radius", &m_Settings.Bloom.Radius, 0.0f, 4.0f);
		ImGui::Checkbox("Room (for ambient occlusion)", &m_ShowRoom);
		ImGui::Checkbox("Ambient occlusion", &m_Settings.AmbientOcclusion.Enabled);
		ImGui::SliderFloat("AO radius", &m_Settings.AmbientOcclusion.Radius, 0.05f, 2.0f);
		ImGui::SliderFloat("AO intensity", &m_Settings.AmbientOcclusion.Intensity, 0.0f, 4.0f);
		ImGui::SliderFloat("AO power", &m_Settings.AmbientOcclusion.Power, 0.5f, 4.0f);
		ImGui::SliderFloat("AO bias", &m_Settings.AmbientOcclusion.Bias, 0.0f, 0.2f);
		ImGui::Checkbox("AO at half resolution", &m_Settings.AmbientOcclusion.HalfResolution);
		ImGui::TextWrapped("Strips, top to bottom: None, Soft, ACES and Neutral on a white 0..8 gradient, then Soft on (1, 0.5, 0.25).");

		const PostProcessStack::Statistics& stats = Renderer::GetPostProcessStack().GetStatistics();
		ImGui::Text("Chain: %u scenes last frame, %u targets, %.1f MB", stats.Scenes, stats.SceneTargets, static_cast<double>(stats.TargetBytes) / (1024.0 * 1024.0));

		GraphicsTest::ImGuiRender();

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

}
