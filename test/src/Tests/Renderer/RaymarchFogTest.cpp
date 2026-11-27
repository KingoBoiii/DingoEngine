#include "RaymarchFogTest.h"

#include "DingoEngine/Graphics/Renderer3DAccess.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Dingo
{

	namespace
	{
		using Access = Internal::Renderer3DAccess;
		using Frame = RaymarchFogTest::Frame;
		using Solid = RaymarchFogTest::Solid;
		using SolidKind = RaymarchFogTest::SolidKind;
		using Placed = RaymarchFogTest::Placed;
		using Sun = RaymarchFogTest::Sun;

		constexpr uint32_t k_FramesBeforeChecks = 10;
		constexpr uint32_t k_Width = 64;
		constexpr uint32_t k_Height = 36;
		constexpr uint32_t k_LargeWidth = 96;
		constexpr uint32_t k_LargeHeight = 54;
		constexpr float k_Fov = 60.0f;
		constexpr float k_Infinity = std::numeric_limits<float>::infinity();
		constexpr float k_WallFront = -11.75f;
		constexpr float k_Tolerance = 0.03f;
		constexpr float k_RelativeFloor = 0.004f;
		constexpr float k_Relative = 0.2f;
		constexpr int k_ModelSteps = 256;
		const glm::vec3 k_Eye(0.0f, 1.0f, 0.0f);

		constexpr const char* k_CustomShader = R"(
#type vertex
#version 450
#include <DingoEngine/Renderer3DCamera.glsl>

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in vec2 a_TexCoord;

void main()
{
	gl_Position = ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 450

layout(location = 0) out vec4 o_Color;

void main()
{
	o_Color = vec4(0.7, 0.3, 0.2, 1.0);
}
)";

		struct Image
		{
			uint32_t Width = 0;
			uint32_t Height = 0;
			std::vector<glm::vec3> Pixels;
			std::vector<uint8_t> Raw;

			bool Valid() const { return Width > 0 && Pixels.size() == static_cast<size_t>(Width) * Height; }
			const glm::vec3& At(uint32_t x, uint32_t y) const { return Pixels[static_cast<size_t>(y) * Width + x]; }
		};

		Image Decode(const TexturePixels& pixels)
		{
			Image image;
			if (pixels.Data.empty())
				return image;
			image.Width = pixels.Width;
			image.Height = pixels.Height;
			image.Pixels.reserve(static_cast<size_t>(pixels.Width) * pixels.Height);
			for (uint32_t y = 0; y < pixels.Height; ++y)
			{
				for (uint32_t x = 0; x < pixels.Width; ++x)
					image.Pixels.push_back(glm::vec3(pixels.GetPixel(x, y)));
			}
			image.Raw = pixels.Data;
			return image;
		}

		struct ReadSet
		{
			std::vector<Image> Images;
			size_t Remaining = 0;
			std::function<void(const std::vector<Image>&)> Done;
		};

		std::shared_ptr<ReadSet> MakeSet(size_t count, std::function<void(const std::vector<Image>&)> done)
		{
			auto set = std::make_shared<ReadSet>();
			set->Images.resize(count);
			set->Remaining = count;
			set->Done = std::move(done);
			return set;
		}

		void Snap(const std::shared_ptr<ReadSet>& set, size_t index, Framebuffer* target, const std::weak_ptr<int>& alive)
		{
			target->GetAttachment(0)->ReadPixels([alive, set, index](const TexturePixels& pixels)
			{
				if (alive.expired())
					return;
				set->Images[index] = Decode(pixels);
				if (--set->Remaining == 0)
					set->Done(set->Images);
			});
		}

		glm::vec3 PixelRay(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
		{
			const float tanHalf = std::tan(glm::radians(k_Fov * 0.5f));
			const float aspect = static_cast<float>(width) / static_cast<float>(height);
			const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
			const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
			return glm::normalize(glm::vec3((2.0f * u - 1.0f) * tanHalf * aspect, (1.0f - 2.0f * v) * tanHalf, -1.0f));
		}

		bool RayBox(const glm::vec3& origin, const glm::vec3& direction, const glm::vec3& low, const glm::vec3& high, float& enter, float& exit)
		{
			enter = -k_Infinity;
			exit = k_Infinity;
			for (int axis = 0; axis < 3; ++axis)
			{
				if (std::abs(direction[axis]) < 1e-9f)
				{
					if (origin[axis] < low[axis] || origin[axis] > high[axis])
						return false;
					continue;
				}
				const float a = (low[axis] - origin[axis]) / direction[axis];
				const float b = (high[axis] - origin[axis]) / direction[axis];
				enter = (std::max)(enter, (std::min)(a, b));
				exit = (std::min)(exit, (std::max)(a, b));
			}
			return exit >= enter;
		}

		float HenyeyGreenstein(float cosTheta, float g)
		{
			const float g2 = g * g;
			const float denominator = 1.0f + g2 - 2.0f * g * cosTheta;
			return (1.0f - g2) / (4.0f * glm::pi<float>() * denominator * std::sqrt(denominator));
		}

		float DistanceFade(float t, float start, float end, float keepBefore, float keepAfter)
		{
			if (end <= start)
				return 1.0f;
			const float s = glm::clamp((t - start) / (end - start), 0.0f, 1.0f);
			return keepBefore + (keepAfter - keepBefore) * s;
		}

		Solid Wall(float front = k_WallFront)
		{
			Solid wall;
			wall.Center = glm::vec3(0.0f, 1.0f, front - 0.25f);
			wall.Half = glm::vec3(30.0f, 30.0f, 0.25f);
			wall.Color = glm::vec4(0.3f, 0.28f, 0.26f, 1.0f);
			return wall;
		}

		Placed Haze(float density = 0.1f)
		{
			Placed placed;
			placed.Settings.Mode = FogVolumeMode::Raymarch;
			placed.Settings.Shape = FogVolumeShape::Box;
			placed.Settings.HalfExtents = glm::vec3(30.0f, 30.0f, 4.0f);
			placed.Settings.Density = density;
			placed.Settings.EdgeFade = 0.0f;
			placed.Settings.NoiseMin = 1.0f;
			placed.Settings.NoiseMax = 1.0f;
			placed.Center = glm::vec3(0.0f, 1.0f, -6.0f);
			return placed;
		}

		Frame Basic(float density = 0.1f)
		{
			Frame frame;
			frame.Solids.push_back(Wall());
			frame.Volumes.push_back(Haze(density));
			return frame;
		}

		Frame Clean(Frame frame)
		{
			frame.Volumes.clear();
			frame.Global = false;
			frame.Extra = nullptr;
			return frame;
		}

		struct Modeled
		{
			Image Color;
			Image Haze;
			std::vector<int> Surface;
			std::vector<float> Distance;
			std::vector<float> Entry;
		};

		using SunScale = std::function<float(uint32_t, uint32_t)>;

		Modeled Predict(const Image& ref, const Frame& frame, const SunScale& sunScale = nullptr)
		{
			Modeled out;
			const size_t count = ref.Pixels.size();
			out.Color = ref;
			out.Haze = ref;
			out.Haze.Raw.clear();
			out.Color.Raw.clear();
			out.Surface.assign(count, -1);
			out.Distance.assign(count, k_Infinity);
			out.Entry.assign(count, k_Infinity);

			struct Segment
			{
				size_t Volume;
				float Enter;
				float Exit;
			};

			for (uint32_t y = 0; y < ref.Height; ++y)
			{
				for (uint32_t x = 0; x < ref.Width; ++x)
				{
					const size_t index = static_cast<size_t>(y) * ref.Width + x;
					const glm::vec3 direction = PixelRay(x, y, ref.Width, ref.Height);

					float surface = k_Infinity;
					int surfaceId = -1;
					for (size_t i = 0; i < frame.Solids.size(); ++i)
					{
						const Solid& solid = frame.Solids[i];
						float enter, exit;
						if (solid.Kind != SolidKind::Glass && RayBox(k_Eye, direction, solid.Center - solid.Half, solid.Center + solid.Half, enter, exit) && enter > 0.0f && enter < surface)
						{
							surface = enter;
							surfaceId = static_cast<int>(i);
						}
					}
					out.Surface[index] = surfaceId;
					out.Distance[index] = surface;

					std::vector<Segment> segments;
					for (size_t i = 0; i < frame.Volumes.size(); ++i)
					{
						const Placed& placed = frame.Volumes[i];
						float enter, exit;
						if (!RayBox(k_Eye, direction, placed.Center - placed.Settings.HalfExtents, placed.Center + placed.Settings.HalfExtents, enter, exit))
							continue;
						enter = (std::max)(enter, 0.0f);
						exit = (std::min)(exit, surface);
						if (exit > enter)
							segments.push_back({ i, enter, exit });
					}
					std::sort(segments.begin(), segments.end(), [](const Segment& a, const Segment& b) { return a.Enter < b.Enter; });

					float transmittance = 1.0f;
					glm::vec3 haze(0.0f);
					for (const Segment& segment : segments)
					{
						const FogVolume& volume = frame.Volumes[segment.Volume].Settings;
						glm::vec3 light(frame.Ambient * volume.AmbientIntensity);
						const float scale = sunScale ? sunScale(x, y) : 1.0f;
						for (const Sun& sun : frame.Suns)
						{
							const float phase = glm::pi<float>() * HenyeyGreenstein(glm::dot(glm::normalize(sun.Direction), -direction), volume.Anisotropy);
							light += phase * sun.Color * volume.LightIntensity * scale;
						}

						const glm::vec3 coefficient = volume.Albedo * volume.ScatterTint;
						const float step = (segment.Exit - segment.Enter) / static_cast<float>(k_ModelSteps);
						const float stepTransmittance = std::exp(-volume.Density * step);
						float inside = 1.0f;
						glm::vec3 scattered(0.0f);
						for (int k = 0; k < k_ModelSteps; ++k)
						{
							const float t = segment.Enter + (static_cast<float>(k) + 0.5f) * step;
							const float weight = DistanceFade(t, volume.NearFadeStart, volume.NearFadeEnd, volume.NearFadeKeep, 1.0f)
								* DistanceFade(t, volume.FarFadeStart, volume.FarFadeEnd, 1.0f, volume.FarFadeKeep);
							scattered += inside * light * coefficient * weight * (1.0f - stepTransmittance);
							inside *= stepTransmittance;
						}
						if (volume.MaxInScatter > 0.0f)
						{
							const float peak = (std::max)({ scattered.x, scattered.y, scattered.z });
							if (peak > volume.MaxInScatter)
								scattered *= volume.MaxInScatter / peak;
						}

						if (out.Entry[index] == k_Infinity)
							out.Entry[index] = segment.Enter;
						haze += transmittance * scattered;
						transmittance *= inside;
					}

					out.Haze.Pixels[index] = haze;
					out.Color.Pixels[index] = ref.Pixels[index] * transmittance + haze;
				}
			}
			return out;
		}

		struct Options
		{
			float Tolerance = k_Tolerance;
			float Visible = 0.05f;
			float Relative = 0.0f;
			uint32_t EdgeMargin = 0;
			float EdgeTolerance = 0.0f;
			uint32_t SkipFrom = 1;
			uint32_t SkipTo = 0;
		};

		Options Proportional()
		{
			Options options;
			options.Relative = k_Relative;
			return options;
		}

		bool NearEdge(const std::vector<int>& surface, uint32_t width, uint32_t height, uint32_t x, uint32_t y, uint32_t margin)
		{
			const int own = surface[static_cast<size_t>(y) * width + x];
			const int low = -static_cast<int>(margin);
			const int high = static_cast<int>(margin);
			for (int dy = low; dy <= high; ++dy)
			{
				for (int dx = low; dx <= high; ++dx)
				{
					const int nx = static_cast<int>(x) + dx;
					const int ny = static_cast<int>(y) + dy;
					if (nx < 0 || ny < 0 || nx >= static_cast<int>(width) || ny >= static_cast<int>(height))
						continue;
					if (surface[static_cast<size_t>(ny) * width + nx] != own)
						return true;
				}
			}
			return false;
		}

		float Worst(const glm::vec3& a, const glm::vec3& b)
		{
			const glm::vec3 difference = glm::abs(a - b);
			const float worst = (std::max)({ difference.x, difference.y, difference.z });
			return std::isfinite(worst) ? worst : 1e9f;
		}

		void Expect(TestChecks& checks, const std::string& name, const Image& got, const Modeled& want, const Image& ref, const Options& options)
		{
			if (!got.Valid() || got.Width != want.Color.Width || got.Height != want.Color.Height)
			{
				checks.Check(false, name + " (no pixels to compare)");
				return;
			}

			float interior = 0.0f;
			float edge = 0.0f;
			float visible = 0.0f;
			float excess = 0.0f;
			uint32_t worstX = 0;
			uint32_t worstY = 0;
			for (uint32_t y = 0; y < got.Height; ++y)
			{
				for (uint32_t x = 0; x < got.Width; ++x)
				{
					if (x >= options.SkipFrom && x <= options.SkipTo)
						continue;
					const float error = Worst(got.At(x, y), want.Color.At(x, y));
					const float change = Worst(want.Color.At(x, y), ref.At(x, y));
					visible = (std::max)(visible, change);
					if (options.EdgeMargin > 0 && NearEdge(want.Surface, got.Width, got.Height, x, y, options.EdgeMargin))
						edge = (std::max)(edge, error);
					else
					{
						excess = (std::max)(excess, error - options.Relative * change);
						if (error > interior)
						{
							interior = error;
							worstX = x;
							worstY = y;
						}
					}
				}
			}

			const bool ok = interior <= options.Tolerance && (options.EdgeMargin == 0 || edge <= options.EdgeTolerance) && visible >= options.Visible && (options.Relative <= 0.0f || excess <= k_RelativeFloor);
			checks.Check(ok, std::format("{} (worst {:.4f} of {:.3f} at {},{}; silhouette {:.4f} of {:.3f}; haze shows {:.3f}; over {:.0f}% of the haze {:.4f} of {:.3f})",
				name, interior, options.Tolerance, worstX, worstY, edge, options.EdgeTolerance, visible, options.Relative * 100.0f, excess, k_RelativeFloor));
		}

		size_t DifferingPixels(const Image& a, const Image& b)
		{
			if (!a.Valid() || !b.Valid() || a.Pixels.size() != b.Pixels.size())
				return (std::numeric_limits<size_t>::max)();
			size_t differing = 0;
			for (size_t i = 0; i < a.Pixels.size(); ++i)
				differing += a.Pixels[i] != b.Pixels[i] ? 1 : 0;
			return differing;
		}

		float WorstDifference(const Image& a, const Image& b)
		{
			if (!a.Valid() || !b.Valid() || a.Pixels.size() != b.Pixels.size())
				return 1e9f;
			float worst = 0.0f;
			for (size_t i = 0; i < a.Pixels.size(); ++i)
				worst = (std::max)(worst, Worst(a.Pixels[i], b.Pixels[i]));
			return worst;
		}

		struct Ids
		{
			uint64_t Scatter = 0;
			uint64_t Depth = 0;
			uint32_t Width = 0;
			uint32_t Height = 0;

			bool SameTextures(const Ids& other) const { return Scatter != 0 && Scatter == other.Scatter && Depth != 0 && Depth == other.Depth; }
		};

		Ids Current(const Renderer3D& renderer)
		{
			Ids ids;
			Texture* scatter = Access::RaymarchScatter(renderer);
			Texture* depth = Access::RaymarchDepth(renderer);
			if (scatter && depth)
			{
				ids.Scatter = scatter->GetId();
				ids.Depth = depth->GetId();
				ids.Width = scatter->GetParams().Width;
				ids.Height = scatter->GetParams().Height;
			}
			return ids;
		}

		Framebuffer* MakeTarget(const char* name, uint32_t width, uint32_t height)
		{
			return Framebuffer::Create(FramebufferParams()
				.SetDebugName(name)
				.SetWidth(static_cast<int32_t>(width))
				.SetHeight(static_cast<int32_t>(height))
				.SetEnableDepth(true)
				.AddAttachment({ TextureFormat::RGBA16F }));
		}
	}

	void RaymarchFogTest::Initialize()
	{
		m_Checks.clear();
		m_Started = false;
		m_ChecksDone = false;
		m_FramesBeforeChecks = 0;
		m_Steps.clear();
		m_Step = 0;
		m_Time = 0.0f;
		m_Alive = std::make_shared<int>(0);

		Renderer3DParams on;
		on.Capabilities.Volumetrics = true;
		on.Capabilities.VolumetricGrid = glm::uvec3(32, 18, 16);
		on.Capabilities.VolumeRaymarchResolution = RaymarchResolution::Half;
		m_Renderer = Renderer3D::Create(on);

		Renderer3DParams off;
		off.Capabilities.Volumetrics = false;
		m_Off = Renderer3D::Create(off);

		Renderer3DParams view;
		view.Capabilities.Volumetrics = true;
		m_View = Renderer3D::Create(view);

		m_Box = Mesh::CreateBox(1.0f, 1.0f, 1.0f);
		m_Quad = Mesh::Create(
			{
				{ glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f) },
				{ glm::vec3(0.5f, -0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f) },
				{ glm::vec3(0.5f, 0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f) },
				{ glm::vec3(-0.5f, 0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f) },
			},
			{ 0, 1, 2, 0, 2, 3 });

		if (m_Renderer)
		{
			m_Alpha = m_Renderer->CreateLitMaterial(MaterialParams().SetDebugName("RaymarchFogTest alpha").SetTranslucent(true).SetBlendMode(BlendMode::Alpha));
		}
		m_CustomShader = Shader::CreateFromSource("RaymarchFogCustom", k_CustomShader);
		if (m_CustomShader && m_CustomShader->IsValid())
			m_Custom = Material::Create(MaterialParams().SetDebugName("RaymarchFogTest custom").SetShader(m_CustomShader).SetCullMode(CullMode::None));

		m_Target = MakeTarget("RaymarchFogTest target", k_Width, k_Height);
		m_Large = MakeTarget("RaymarchFogTest large target", k_LargeWidth, k_LargeHeight);
	}

	void RaymarchFogTest::Update(float deltaTime)
	{
		m_Time += deltaTime;
		if (!Renderer::IsFrameSkipped())
		{
			if (!m_Started)
			{
				if (++m_FramesBeforeChecks > k_FramesBeforeChecks)
				{
					m_Started = true;
					BuildSteps();
				}
			}
			else if (m_Step < m_Steps.size())
			{
				const std::function<void()> step = m_Steps[m_Step++];
				step();
				m_ChecksDone = m_Step == m_Steps.size();
			}
		}
		DrawView();
	}

	void RaymarchFogTest::BuildSteps()
	{
		if (!m_Renderer || !m_Off || !m_View || !m_Box || !m_Quad || !m_Alpha || !m_Custom || !m_Target || !m_Large)
		{
			Check(false, "Raymarch fog: the renderers, meshes, materials and targets are created");
			m_Steps.clear();
			return;
		}

		m_Steps = {
			[this] { CheckModes(); },
			[this] { CheckUniform(); },
			[this] { CheckInsideAndSky(); },
			[this] { CheckDepthEdge(); },
			[this] { CheckShaft(); },
			[this] { CheckCapAndFades(); },
			[this] { CheckSteps(); },
			[this] { CheckFroxelMix(); },
			[this] { CheckTranslucent(); },
			[this] { CheckCustom(); },
			[this] { CheckStillAndScene(); },
			[this] { CheckResize(); },
			[this] { CheckResolution(); },
			[this] { CheckTwoScenes(); },
			[this] { CheckTwoBegins(); },
			[this] { CheckDebugView(); },
		};
	}

	void RaymarchFogTest::Submit(Renderer3D& renderer, const Frame& frame)
	{
		renderer.SetAmbientLight(glm::vec3(1.0f), frame.Ambient);
		for (const Sun& sun : frame.Suns)
		{
			DirectionalLight light;
			light.Direction = glm::normalize(sun.Direction);
			light.Color = sun.Color;
			light.Intensity = 1.0f;
			light.CastShadows = sun.Casts;
			renderer.SubmitLight(light);
		}

		for (const Solid& solid : frame.Solids)
		{
			if (solid.Kind == SolidKind::Glass)
			{
				const glm::mat4 world = glm::translate(glm::mat4(1.0f), solid.Center) * glm::scale(glm::mat4(1.0f), glm::vec3(solid.Half.x * 2.0f, solid.Half.y * 2.0f, 1.0f));
				renderer.SubmitMesh(m_Quad, world, solid.Color, m_Alpha);
				continue;
			}
			const glm::mat4 world = glm::translate(glm::mat4(1.0f), solid.Center) * glm::scale(glm::mat4(1.0f), solid.Half * 2.0f);
			renderer.SubmitMesh(m_Box, world, solid.Color, solid.Kind == SolidKind::Custom ? m_Custom : nullptr);
		}

		if (frame.Global)
			renderer.SetVolumetricFog(frame.Fog);
		renderer.SetFogVolumeTime(0.0);
		for (const Placed& placed : frame.Volumes)
			renderer.SubmitFogVolume(placed.Settings, glm::translate(glm::mat4(1.0f), placed.Center));
		if (frame.Extra)
			frame.Extra(renderer);
	}

	Renderer3D::Statistics RaymarchFogTest::Draw(Renderer3D& renderer, const Frame& frame, Framebuffer* target, bool post)
	{
		Framebuffer* previous = Renderer::GetRenderTarget();
		Renderer::SetRenderTarget(target);

		PostProcessSettings settings;
		settings.Enabled = true;
		settings.Tone.Operator = ToneMapOperator::None;
		PostProcessStack& stack = Renderer::GetPostProcessStack();
		if (post)
			stack.Begin(settings);

		PerspectiveCamera camera(k_Fov, static_cast<float>(target->GetWidth()) / static_cast<float>(target->GetHeight()), 0.1f, 100.0f);
		camera.SetPosition(k_Eye);
		camera.SetUp(glm::vec3(0.0f, 1.0f, 0.0f));
		camera.SetTarget(k_Eye + glm::vec3(0.0f, 0.0f, -1.0f));
		renderer.BeginScene(camera);
		renderer.Clear(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
		Submit(renderer, frame);
		renderer.EndScene();

		if (post)
			stack.End();
		Renderer::SetRenderTarget(previous);
		return renderer.GetStatistics();
	}

	void RaymarchFogTest::CheckModes()
	{
		const Frame frame = Basic();

		const Renderer3D::Statistics direct = Draw(*m_Renderer, frame, m_Target, false);
		Check(direct.RaymarchFallbacks == 1 && direct.RaymarchVolumes == 0 && direct.RaymarchDispatches == 0 && direct.FogVolumes == 1 && direct.VolumetricDispatches == 2,
			std::format("Raymarch fog 1a: without the post chain the volume is drawn as a froxel one (fallbacks {}, raymarch volumes {}, raymarch dispatches {}, kept {}, froxel dispatches {})",
				direct.RaymarchFallbacks, direct.RaymarchVolumes, direct.RaymarchDispatches, direct.FogVolumes, direct.VolumetricDispatches));

		const Renderer3D::Statistics off = Draw(*m_Off, frame, m_Target);
		Check(off.FogVolumes == 0 && off.RaymarchVolumes == 0 && off.RaymarchDispatches == 0 && off.VolumetricDispatches == 0 && off.RaymarchFallbacks == 0,
			std::format("Raymarch fog 1b: with Capabilities.Volumetrics off nothing is kept or marched (kept {}, raymarch dispatches {}, froxel dispatches {})",
				off.FogVolumes, off.RaymarchDispatches, off.VolumetricDispatches));

		const Renderer3D::Statistics on = Draw(*m_Renderer, frame, m_Target);
		Check(on.FogVolumes == 1 && on.RaymarchVolumes == 1 && on.RaymarchDispatches == 1 && on.VolumetricDispatches == 0 && on.RaymarchFallbacks == 0,
			std::format("Raymarch fog 1c: with the post chain one dispatch marches the volume and the froxel passes stay off (kept {}, raymarch volumes {}, raymarch dispatches {}, froxel dispatches {}, fallbacks {})",
				on.FogVolumes, on.RaymarchVolumes, on.RaymarchDispatches, on.VolumetricDispatches, on.RaymarchFallbacks));
	}

	void RaymarchFogTest::CheckUniform()
	{
		const Frame hazy = Basic();
		const Frame clean = Clean(hazy);

		m_Renderer->SetVolumeRaymarchResolution(RaymarchResolution::Half);
		const std::shared_ptr<ReadSet> set = MakeSet(3, [this, hazy](const std::vector<Image>& images)
		{
			const Modeled model = Predict(images[0], hazy);
			Expect(m_Checks, "Raymarch fog 2a: a uniform ambient box over the wall at half resolution is c x T + ambient x (1 - T)", images[1], model, images[0], Proportional());
			Expect(m_Checks, "Raymarch fog 2b: the same at quarter resolution", images[2], model, images[0], Proportional());
		});
		Draw(*m_Renderer, clean, m_Target);
		Snap(set, 0, m_Target, m_Alive);
		Draw(*m_Renderer, hazy, m_Target);
		Snap(set, 1, m_Target, m_Alive);
		m_Renderer->SetVolumeRaymarchResolution(RaymarchResolution::Quarter);
		Draw(*m_Renderer, hazy, m_Target);
		Snap(set, 2, m_Target, m_Alive);
		m_Renderer->SetVolumeRaymarchResolution(RaymarchResolution::Half);
	}

	void RaymarchFogTest::CheckInsideAndSky()
	{
		Frame inside = Basic();
		inside.Solids[0] = Wall(-6.0f);
		const Frame insideClean = Clean(inside);

		Frame sky = Basic();
		sky.Solids.clear();
		const Frame skyClean = Clean(sky);

		const std::shared_ptr<ReadSet> set = MakeSet(4, [this, inside, sky](const std::vector<Image>& images)
		{
			Expect(m_Checks, "Raymarch fog 3: a wall inside the box ends the march at its surface", images[1], Predict(images[0], inside), images[0], Proportional());
			Expect(m_Checks, "Raymarch fog 4: the sky behind the box is black with only the in-scatter", images[3], Predict(images[2], sky), images[2], Options{});
		});
		Draw(*m_Renderer, insideClean, m_Target);
		Snap(set, 0, m_Target, m_Alive);
		Draw(*m_Renderer, inside, m_Target);
		Snap(set, 1, m_Target, m_Alive);
		Draw(*m_Renderer, skyClean, m_Target);
		Snap(set, 2, m_Target, m_Alive);
		Draw(*m_Renderer, sky, m_Target);
		Snap(set, 3, m_Target, m_Alive);
	}

	void RaymarchFogTest::CheckDepthEdge()
	{
		Frame hazy = Basic();
		Solid pillar;
		pillar.Center = glm::vec3(0.3f, 1.1f, -5.0f);
		pillar.Half = glm::vec3(1.2f, 1.0f, 0.5f);
		pillar.Color = glm::vec4(0.05f, 0.05f, 0.05f, 1.0f);
		hazy.Solids.push_back(pillar);
		const Frame clean = Clean(hazy);

		const std::shared_ptr<ReadSet> set = MakeSet(3, [this, hazy](const std::vector<Image>& images)
		{
			const Modeled model = Predict(images[0], hazy);
			Options half;
			half.EdgeMargin = 2;
			half.EdgeTolerance = 0.08f;
			Options quarter;
			quarter.EdgeMargin = 4;
			quarter.EdgeTolerance = 0.08f;
			Expect(m_Checks, "Raymarch fog 5a: a pillar in the haze leaves no halo at its silhouette at half resolution", images[1], model, images[0], half);
			Expect(m_Checks, "Raymarch fog 5b: the same at quarter resolution", images[2], model, images[0], quarter);
		});
		m_Renderer->SetVolumeRaymarchResolution(RaymarchResolution::Half);
		Draw(*m_Renderer, clean, m_Target);
		Snap(set, 0, m_Target, m_Alive);
		Draw(*m_Renderer, hazy, m_Target);
		Snap(set, 1, m_Target, m_Alive);
		m_Renderer->SetVolumeRaymarchResolution(RaymarchResolution::Quarter);
		Draw(*m_Renderer, hazy, m_Target);
		Snap(set, 2, m_Target, m_Alive);
		m_Renderer->SetVolumeRaymarchResolution(RaymarchResolution::Half);
	}

	void RaymarchFogTest::CheckShaft()
	{
		Frame hazy = Basic();
		Sun sun;
		sun.Direction = glm::vec3(0.0f, -1.0f, 0.0f);
		sun.Color = glm::vec3(3.0f);
		sun.Casts = true;
		hazy.Suns.push_back(sun);
		Solid roof;
		roof.Center = glm::vec3(-15.0f, 12.0f, -6.0f);
		roof.Half = glm::vec3(15.0f, 0.25f, 10.0f);
		hazy.Solids.push_back(roof);
		const Frame clean = Clean(hazy);

		const std::shared_ptr<ReadSet> set = MakeSet(2, [this, hazy](const std::vector<Image>& images)
		{
			const SunScale scale = [](uint32_t x, uint32_t) { return x < k_Width / 2 ? 0.0f : 1.0f; };
			Options options;
			options.SkipFrom = k_Width / 2 - 6;
			options.SkipTo = k_Width / 2 + 5;
			Expect(m_Checks, "Raymarch fog 6a: sunlit rays carry the sun's in-scatter, rays under the roof's shadow only the ambient", images[1], Predict(images[0], hazy, scale), images[0], options);

			double lit = 0.0;
			double shaded = 0.0;
			uint32_t samples = 0;
			for (uint32_t y = 0; y < k_Height; ++y)
			{
				for (uint32_t x = 0; x < k_Width / 2 - 6; ++x)
				{
					shaded += images[1].At(x, y).r - images[0].At(x, y).r;
					lit += images[1].At(k_Width - 1 - x, y).r - images[0].At(k_Width - 1 - x, y).r;
					++samples;
				}
			}
			const double contrast = samples ? (lit - shaded) / samples : 0.0;
			Check(contrast > 0.05, std::format("Raymarch fog 6b: the lit half gains more in-scatter than the shadowed half (mean red gain difference {:.3f})", contrast));
		});
		Draw(*m_Renderer, clean, m_Target);
		Snap(set, 0, m_Target, m_Alive);
		Draw(*m_Renderer, hazy, m_Target);
		Snap(set, 1, m_Target, m_Alive);
	}

	void RaymarchFogTest::CheckDebugView()
	{
		Frame shaft = Basic();
		shaft.Solids.clear();
		shaft.Ambient = 0.0f;
		Sun sun;
		sun.Direction = glm::vec3(0.0f, -1.0f, 0.0f);
		sun.Color = glm::vec3(3.0f);
		sun.Casts = true;
		shaft.Suns.push_back(sun);
		Solid roof;
		roof.Center = glm::vec3(-15.0f, 12.0f, -6.0f);
		roof.Half = glm::vec3(15.0f, 0.25f, 10.0f);
		shaft.Solids.push_back(roof);

		Frame open = shaft;
		open.Solids.clear();
		open.Suns[0].Casts = false;

		Frame slice = Clean(shaft);
		slice.Global = true;
		slice.Fog.Density = 0.02f;
		slice.Fog.Temporal = false;
		slice.Extra = [](Renderer3D& renderer) { Internal::Renderer3DAccess::RequestVolumetricSlice(renderer, true, false, 8, 1.0f); };

		const std::shared_ptr<ReadSet> set = MakeSet(2, [this](const std::vector<Image>& images)
		{
			auto mean = [](const Image& image, uint32_t from, uint32_t to)
			{
				glm::dvec3 sum(0.0);
				uint32_t samples = 0;
				for (uint32_t y = 0; y < image.Height; ++y)
				{
					for (uint32_t x = from; x < to; ++x)
					{
						sum += glm::dvec3(image.At(x, y));
						++samples;
					}
				}
				return samples ? sum / static_cast<double>(samples) : glm::dvec3(0.0);
			};
			const glm::dvec3 shadowed = mean(images[0], 0, k_Width / 2 - 6);
			const glm::dvec3 lit = mean(images[0], k_Width / 2 + 6, k_Width);
			const glm::dvec3 unshadowed = mean(images[1], 0, k_Width);
			Check(shadowed.r > 0.005 && shadowed.r > 3.0 * (shadowed.g + shadowed.b),
				std::format("Raymarch fog 18a: the debug view paints rays through the roof's shadow red (mean {:.4f} {:.4f} {:.4f})", shadowed.r, shadowed.g, shadowed.b));
			Check(lit.g > 0.005 && lit.g > 3.0 * (lit.r + lit.b),
				std::format("Raymarch fog 18b: the debug view paints sunlit rays green (mean {:.4f} {:.4f} {:.4f})", lit.r, lit.g, lit.b));
			Check(unshadowed.r > 0.005 && unshadowed.g > 0.005 && unshadowed.b < 0.1 * unshadowed.r && std::abs(unshadowed.r - unshadowed.g) < 0.1 * unshadowed.r,
				std::format("Raymarch fog 18c: a sun that casts no shadows is painted yellow (mean {:.4f} {:.4f} {:.4f})", unshadowed.r, unshadowed.g, unshadowed.b));
		});

		m_Renderer->SetVolumetricDebugView(VolumetricDebugView::ShadowSamples);
		Draw(*m_Renderer, shaft, m_Target);
		Snap(set, 0, m_Target, m_Alive);
		Draw(*m_Renderer, open, m_Target);
		Snap(set, 1, m_Target, m_Alive);
		m_Renderer->SetVolumetricDebugView(VolumetricDebugView::None);

		Draw(*m_Renderer, slice, m_Target);
		const Texture* picture = Internal::Renderer3DAccess::VolumetricSlice(*m_Renderer);
		const glm::uvec3 grid = m_Renderer->GetVolumetricGrid();
		Check(picture && picture->GetParams().Width == grid.x && picture->GetParams().Height == grid.y,
			std::format("Raymarch fog 18d: a requested froxel slice is a texture of the grid's x and y size (got {} x {})", picture ? picture->GetParams().Width : 0u, picture ? picture->GetParams().Height : 0u));
	}

	void RaymarchFogTest::CheckCapAndFades()
	{
		Frame capped = Basic();
		capped.Ambient = 1.5f;
		capped.Volumes[0].Settings.MaxInScatter = 0.2f;
		const Frame cappedClean = Clean(capped);

		Frame faded = Basic();
		FogVolume& settings = faded.Volumes[0].Settings;
		settings.RaymarchSteps = 64;
		settings.NearFadeStart = 2.0f;
		settings.NearFadeEnd = 6.0f;
		settings.NearFadeKeep = 0.0f;
		settings.FarFadeStart = 6.0f;
		settings.FarFadeEnd = 11.0f;
		settings.FarFadeKeep = 0.25f;
		const Frame fadedClean = Clean(faded);

		const std::shared_ptr<ReadSet> set = MakeSet(4, [this, capped, faded](const std::vector<Image>& images)
		{
			Expect(m_Checks, "Raymarch fog 7: MaxInScatter scales the strongest channel down to it and leaves the transmittance alone", images[1], Predict(images[0], capped), images[0], Options{});
			Options fades;
			fades.Tolerance = 0.02f;
			fades.Relative = k_Relative;
			Expect(m_Checks, "Raymarch fog 8: near and far fades thin the in-scatter and leave the transmittance alone", images[3], Predict(images[2], faded), images[2], fades);
		});
		Draw(*m_Renderer, cappedClean, m_Target);
		Snap(set, 0, m_Target, m_Alive);
		Draw(*m_Renderer, capped, m_Target);
		Snap(set, 1, m_Target, m_Alive);
		Draw(*m_Renderer, fadedClean, m_Target);
		Snap(set, 2, m_Target, m_Alive);
		Draw(*m_Renderer, faded, m_Target);
		Snap(set, 3, m_Target, m_Alive);
	}

	void RaymarchFogTest::CheckSteps()
	{
		Frame frame = Basic(0.15f);
		frame.Volumes[0].Settings.HalfExtents = glm::vec3(6.0f, 6.0f, 4.0f);
		frame.Volumes[0].Settings.EdgeFade = 2.0f;
		Frame coarse = frame;
		coarse.Volumes[0].Settings.RaymarchSteps = 8;
		Frame fine = frame;
		fine.Volumes[0].Settings.RaymarchSteps = 64;

		const std::shared_ptr<ReadSet> set = MakeSet(2, [this](const std::vector<Image>& images)
		{
			const float worst = WorstDifference(images[0], images[1]);
			Check(worst <= 1.0f / 255.0f + 1e-4f, std::format("Raymarch fog 9: 8 and 64 steps through a soft-edged box agree within 1/255 (worst {:.5f})", worst));
		});
		Draw(*m_Renderer, coarse, m_Target);
		Snap(set, 0, m_Target, m_Alive);
		Draw(*m_Renderer, fine, m_Target);
		Snap(set, 1, m_Target, m_Alive);
	}

	void RaymarchFogTest::CheckFroxelMix()
	{
		Frame mixed = Basic();
		mixed.Global = true;
		mixed.Fog.Density = 0.02f;
		mixed.Fog.Temporal = false;
		Frame froxelOnly = mixed;
		froxelOnly.Volumes.clear();
		const Frame clean = Clean(mixed);

		const std::shared_ptr<ReadSet> set = MakeSet(3, [this, mixed, clean](const std::vector<Image>& images)
		{
			const Image& ref = images[0];
			const Modeled geometry = Predict(ref, clean);
			const VolumetricFog& fog = mixed.Fog;
			const float ambient = mixed.Ambient * fog.AmbientIntensity;
			auto transmittanceTo = [&](float distance, float forward)
			{
				return std::exp(-fog.Density * (std::max)(0.0f, distance - fog.NearDistance / forward));
			};

			Image froxel = ref;
			froxel.Raw.clear();
			Modeled froxelOnly = geometry;
			for (uint32_t y = 0; y < ref.Height; ++y)
			{
				for (uint32_t x = 0; x < ref.Width; ++x)
				{
					const float forward = -PixelRay(x, y, ref.Width, ref.Height).z;
					const float t = transmittanceTo(geometry.Distance[static_cast<size_t>(y) * ref.Width + x], forward);
					froxel.Pixels[static_cast<size_t>(y) * ref.Width + x] = ref.At(x, y) * t + glm::vec3(ambient * fog.Albedo) * (1.0f - t);
				}
			}
			froxelOnly.Color = froxel;

			Modeled both = Predict(froxel, mixed);
			for (uint32_t y = 0; y < ref.Height; ++y)
			{
				for (uint32_t x = 0; x < ref.Width; ++x)
				{
					const size_t index = static_cast<size_t>(y) * ref.Width + x;
					if (both.Entry[index] == k_Infinity)
						continue;
					const float forward = -PixelRay(x, y, ref.Width, ref.Height).z;
					both.Color.Pixels[index] += (transmittanceTo(both.Entry[index], forward) - 1.0f) * both.Haze.Pixels[index];
				}
			}

			Options options;
			options.Tolerance = 0.05f;
			Expect(m_Checks, "Raymarch fog 10a: the global froxel fog alone is c x Tf + Lf on the wall", images[1], froxelOnly, ref, options);
			Expect(m_Checks, "Raymarch fog 10b: a global froxel fog and a raymarch box are (c x Tf + Lf) x Tr + Tf(entry) x Lr", images[2], both, froxel, options);
		});
		Draw(*m_Renderer, clean, m_Target);
		Snap(set, 0, m_Target, m_Alive);
		Draw(*m_Renderer, froxelOnly, m_Target);
		Snap(set, 1, m_Target, m_Alive);
		Draw(*m_Renderer, mixed, m_Target);
		Snap(set, 2, m_Target, m_Alive);
	}

	void RaymarchFogTest::CheckTranslucent()
	{
		constexpr float k_Alpha = 0.5f;
		Frame wall = Basic();
		Frame glass = wall;
		Solid pane;
		pane.Kind = SolidKind::Glass;
		pane.Center = glm::vec3(0.0f, 1.0f, -5.0f);
		pane.Half = glm::vec3(2.0f, 1.0f, 0.5f);
		pane.Color = glm::vec4(0.9f, 0.3f, 0.2f, k_Alpha);
		glass.Solids.push_back(pane);

		const std::shared_ptr<ReadSet> set = MakeSet(3, [this, wall](const std::vector<Image>& images)
		{
			const Image& plain = images[0];
			const Image& pane = images[1];
			Modeled model = Predict(plain, wall);
			uint32_t covered = 0;
			for (uint32_t y = 0; y < plain.Height; ++y)
			{
				for (uint32_t x = 0; x < plain.Width; ++x)
				{
					if (Worst(plain.At(x, y), pane.At(x, y)) <= 1e-4f)
						continue;
					++covered;
					const size_t index = static_cast<size_t>(y) * plain.Width + x;
					model.Color.Pixels[index] = (pane.At(x, y) - (1.0f - k_Alpha) * plain.At(x, y)) + (1.0f - k_Alpha) * model.Color.Pixels[index];
				}
			}
			Check(covered > 100, std::format("Raymarch fog 11a: the translucent quad covers part of the view ({} px)", covered));
			Expect(m_Checks, "Raymarch fog 11b: a translucent quad takes the haze behind it but is not hazed itself", images[2], model, plain, Options{});
		});
		Draw(*m_Renderer, Clean(wall), m_Target);
		Snap(set, 0, m_Target, m_Alive);
		Draw(*m_Renderer, Clean(glass), m_Target);
		Snap(set, 1, m_Target, m_Alive);
		Draw(*m_Renderer, glass, m_Target);
		Snap(set, 2, m_Target, m_Alive);
	}

	void RaymarchFogTest::CheckCustom()
	{
		Frame hazy = Basic();
		hazy.Solids[0].Kind = SolidKind::Custom;
		const Frame clean = Clean(hazy);

		const std::shared_ptr<ReadSet> set = MakeSet(2, [this, hazy](const std::vector<Image>& images)
		{
			Expect(m_Checks, "Raymarch fog 12: an opaque custom shader is hazed as a lit surface is", images[1], Predict(images[0], hazy), images[0], Options{});
		});
		Draw(*m_Renderer, clean, m_Target);
		Snap(set, 0, m_Target, m_Alive);
		Draw(*m_Renderer, hazy, m_Target);
		Snap(set, 1, m_Target, m_Alive);
	}

	void RaymarchFogTest::CheckStillAndScene()
	{
		const Frame hazy = Basic();
		Frame viaScene = Clean(hazy);
		const Placed placed = hazy.Volumes[0];
		viaScene.Extra = [placed, ambient = hazy.Ambient](Renderer3D& renderer)
		{
			Dingo::Scene scene("Raymarch fog scene");
			Entity entity = scene.CreateEntity("Haze");
			entity.AddComponent<Transform3DComponent>(Transform3DComponent(placed.Center));
			entity.AddComponent(FogVolumeComponent(placed.Settings));
			scene.CreateEntity("Ambient").AddComponent(AmbientLightComponent(glm::vec3(1.0f), ambient));
			scene.SubmitLights(renderer);
		};

		Renderer3D::Statistics sceneStats;
		const std::shared_ptr<ReadSet> set = MakeSet(3, [this](const std::vector<Image>& images)
		{
			const size_t still = DifferingPixels(images[0], images[1]);
			Check(images[0].Valid() && still == 0, std::format("Raymarch fog 13: two still frames of one scene are identical ({} px differ)", still));
			const size_t scene = DifferingPixels(images[0], images[2]);
			Check(images[0].Valid() && scene == 0, std::format("Raymarch fog 15b: FogVolumeComponents through Scene::SubmitLights draw what SubmitFogVolume draws ({} px differ)", scene));
		});
		Draw(*m_Renderer, hazy, m_Target);
		Snap(set, 0, m_Target, m_Alive);
		Draw(*m_Renderer, hazy, m_Target);
		Snap(set, 1, m_Target, m_Alive);
		sceneStats = Draw(*m_Renderer, viaScene, m_Target);
		Snap(set, 2, m_Target, m_Alive);
		Check(sceneStats.RaymarchVolumes == 1 && sceneStats.RaymarchDispatches == 1,
			std::format("Raymarch fog 15a: a FogVolumeComponent in Raymarch mode is marched (raymarch volumes {}, dispatches {})", sceneStats.RaymarchVolumes, sceneStats.RaymarchDispatches));
	}

	void RaymarchFogTest::CheckResize()
	{
		const Frame hazy = Basic();
		const Frame clean = Clean(hazy);

		m_Renderer->SetVolumeRaymarchResolution(RaymarchResolution::Half);
		Draw(*m_Renderer, hazy, m_Target);
		const Ids small = Current(*m_Renderer);

		const std::shared_ptr<ReadSet> set = MakeSet(2, [this, hazy](const std::vector<Image>& images)
		{
			Expect(m_Checks, "Raymarch fog 14c: a larger render target still matches the model", images[1], Predict(images[0], hazy), images[0], Options{});
		});
		Draw(*m_Renderer, clean, m_Large);
		Snap(set, 0, m_Large, m_Alive);
		Draw(*m_Renderer, hazy, m_Large);
		Snap(set, 1, m_Large, m_Alive);
		const Ids large = Current(*m_Renderer);

		m_Steps.insert(m_Steps.begin() + static_cast<std::ptrdiff_t>(m_Step), [this, hazy, small, large]
		{
			Draw(*m_Renderer, hazy, m_Target);
			const Ids back = Current(*m_Renderer);
			Check(small.SameTextures(back) && !small.SameTextures(large),
				std::format("Raymarch fog 14a: each render target keeps its own scatter and depth textures from frame to frame (scatter {} {} {}, depth {} {} {})", small.Scatter, large.Scatter, back.Scatter, small.Depth, large.Depth, back.Depth));
			Check(small.Width == k_Width / 2 && small.Height == k_Height / 2 && large.Width == k_LargeWidth / 2 && large.Height == k_LargeHeight / 2 && back.Width == small.Width && back.Height == small.Height,
				std::format("Raymarch fog 14b: the textures are half the target's size each time ({}x{}, {}x{}, {}x{})", small.Width, small.Height, large.Width, large.Height, back.Width, back.Height));
		});
	}

	void RaymarchFogTest::CheckResolution()
	{
		const Frame hazy = Basic();
		const Frame clean = Clean(hazy);

		m_Renderer->SetVolumeRaymarchResolution(RaymarchResolution::Half);
		const std::shared_ptr<ReadSet> set = MakeSet(3, [this, hazy](const std::vector<Image>& images)
		{
			const Modeled model = Predict(images[0], hazy);
			Expect(m_Checks, "Raymarch fog 17b: switched to quarter at run time it still matches the model", images[1], model, images[0], Options{});
			Expect(m_Checks, "Raymarch fog 17c: switched back to half it still matches the model", images[2], model, images[0], Options{});
		});
		Draw(*m_Renderer, clean, m_Target);
		Snap(set, 0, m_Target, m_Alive);

		const auto half = std::make_shared<Ids>();
		const auto quarter = std::make_shared<Ids>();
		const std::ptrdiff_t at = static_cast<std::ptrdiff_t>(m_Step);
		m_Steps.insert(m_Steps.begin() + at, [this, hazy, half]
		{
			m_Renderer->SetVolumeRaymarchResolution(RaymarchResolution::Half);
			Draw(*m_Renderer, hazy, m_Target);
			*half = Current(*m_Renderer);
		});
		m_Steps.insert(m_Steps.begin() + at + 1, [this, hazy, quarter, set]
		{
			m_Renderer->SetVolumeRaymarchResolution(RaymarchResolution::Quarter);
			Draw(*m_Renderer, hazy, m_Target);
			*quarter = Current(*m_Renderer);
			Snap(set, 1, m_Target, m_Alive);
		});
		m_Steps.insert(m_Steps.begin() + at + 2, [this, hazy, half, quarter, set]
		{
			m_Renderer->SetVolumeRaymarchResolution(RaymarchResolution::Half);
			Draw(*m_Renderer, hazy, m_Target);
			const Ids again = Current(*m_Renderer);
			Snap(set, 2, m_Target, m_Alive);
			Check(half->SameTextures(*quarter) && half->SameTextures(again) && half->Width == k_Width / 2 && quarter->Width == k_Width / 4 && quarter->Height == k_Height / 4 && again.Width == half->Width && again.Height == half->Height,
				std::format("Raymarch fog 17a: half <-> quarter keeps the textures' ids and resizes them in place ({}x{} id {}, {}x{} id {}, {}x{} id {})",
					half->Width, half->Height, half->Scatter, quarter->Width, quarter->Height, quarter->Scatter, again.Width, again.Height, again.Scatter));
		});
	}

	void RaymarchFogTest::CheckTwoBegins()
	{
		const Frame hazy = Basic();
		const Frame clean = Clean(hazy);

		struct Round
		{
			Ids Small;
			Ids Large;
			uint32_t Dispatches = 0;
		};
		const auto rounds = std::make_shared<std::vector<Round>>(2);

		m_Renderer->SetVolumeRaymarchResolution(RaymarchResolution::Half);
		const std::shared_ptr<ReadSet> set = MakeSet(4, [this, hazy, rounds](const std::vector<Image>& images)
		{
			Expect(m_Checks, "Raymarch fog 16c: two Begins of different sizes in a frame: the small target matches the model", images[2], Predict(images[0], hazy), images[0], Options{});
			Expect(m_Checks, "Raymarch fog 16d: two Begins of different sizes in a frame: the large target matches the model", images[3], Predict(images[1], hazy), images[1], Options{});
			const Round& a = (*rounds)[0];
			const Round& b = (*rounds)[1];
			Check(a.Small.SameTextures(b.Small) && a.Large.SameTextures(b.Large) && !b.Small.SameTextures(b.Large),
				std::format("Raymarch fog 16e: each Begin of a frame keeps its own scatter and depth textures from frame to frame (small {} {}, large {} {})", a.Small.Scatter, b.Small.Scatter, a.Large.Scatter, b.Large.Scatter));
			Check(b.Small.Width == k_Width / 2 && b.Small.Height == k_Height / 2 && b.Large.Width == k_LargeWidth / 2 && b.Large.Height == k_LargeHeight / 2 && a.Dispatches == 1 && b.Dispatches == 1,
				std::format("Raymarch fog 16f: both marches are half their target's size and dispatch once ({}x{}, {}x{}, dispatches {} {})", b.Small.Width, b.Small.Height, b.Large.Width, b.Large.Height, a.Dispatches, b.Dispatches));
		});
		Draw(*m_Renderer, clean, m_Target);
		Snap(set, 0, m_Target, m_Alive);
		Draw(*m_Renderer, clean, m_Large);
		Snap(set, 1, m_Large, m_Alive);

		const std::ptrdiff_t at = static_cast<std::ptrdiff_t>(m_Step);
		for (size_t round = 0; round < 2; ++round)
		{
			m_Steps.insert(m_Steps.begin() + at + static_cast<std::ptrdiff_t>(round), [this, hazy, rounds, set, round]
			{
				Round& current = (*rounds)[round];
				const Renderer3D::Statistics small = Draw(*m_Renderer, hazy, m_Target);
				current.Small = Current(*m_Renderer);
				if (round == 1)
					Snap(set, 2, m_Target, m_Alive);
				const Renderer3D::Statistics large = Draw(*m_Renderer, hazy, m_Large);
				current.Large = Current(*m_Renderer);
				if (round == 1)
					Snap(set, 3, m_Large, m_Alive);
				current.Dispatches = small.RaymarchDispatches == 1 && large.RaymarchDispatches == 1 ? 1u : 0u;
			});
		}
	}

	void RaymarchFogTest::CheckTwoScenes()
	{
		Frame first;
		first.Solids.push_back(Wall());
		Placed behind = Haze();
		behind.Center = glm::vec3(0.0f, 1.0f, -14.0f);
		behind.Settings.HalfExtents = glm::vec3(30.0f, 30.0f, 1.0f);
		first.Volumes.push_back(behind);

		Frame second;
		Solid pillar;
		pillar.Center = glm::vec3(0.3f, 1.1f, -5.0f);
		pillar.Half = glm::vec3(1.2f, 1.0f, 0.5f);
		pillar.Color = glm::vec4(0.05f, 0.05f, 0.05f, 1.0f);
		second.Solids.push_back(pillar);
		second.Volumes.push_back(Haze());

		Frame model;
		model.Solids = { Wall(), pillar };
		model.Volumes.push_back(Haze());

		auto drawBoth = [this](const Frame& a, const Frame& b)
		{
			Framebuffer* previous = Renderer::GetRenderTarget();
			Renderer::SetRenderTarget(m_Target);
			PostProcessSettings settings;
			settings.Enabled = true;
			settings.Tone.Operator = ToneMapOperator::None;
			PostProcessStack& stack = Renderer::GetPostProcessStack();
			stack.Begin(settings);

			PerspectiveCamera camera(k_Fov, static_cast<float>(k_Width) / static_cast<float>(k_Height), 0.1f, 100.0f);
			camera.SetPosition(k_Eye);
			camera.SetUp(glm::vec3(0.0f, 1.0f, 0.0f));
			camera.SetTarget(k_Eye + glm::vec3(0.0f, 0.0f, -1.0f));
			m_Renderer->BeginScene(camera);
			m_Renderer->Clear(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
			Submit(*m_Renderer, a);
			m_Renderer->EndScene();
			m_Renderer->BeginScene(camera);
			Submit(*m_Renderer, b);
			m_Renderer->EndScene();

			stack.End();
			Renderer::SetRenderTarget(previous);
			return m_Renderer->GetStatistics();
		};

		m_Renderer->SetVolumeRaymarchResolution(RaymarchResolution::Half);
		const std::shared_ptr<ReadSet> set = MakeSet(2, [this, model](const std::vector<Image>& images)
		{
			Options options;
			options.EdgeMargin = 2;
			options.EdgeTolerance = 0.08f;
			Expect(m_Checks, "Raymarch fog 16a: the second scene's march sees the depth the second scene drew, not only the first's", images[1], Predict(images[0], model), images[0], options);
		});
		drawBoth(Clean(first), Clean(second));
		Snap(set, 0, m_Target, m_Alive);
		const Renderer3D::Statistics stats = drawBoth(first, second);
		Snap(set, 1, m_Target, m_Alive);
		Check(stats.RaymarchVolumes == 1 && stats.RaymarchDispatches == 1,
			std::format("Raymarch fog 16b: the second scene marches its own volume once (raymarch volumes {}, dispatches {})", stats.RaymarchVolumes, stats.RaymarchDispatches));
	}

	void RaymarchFogTest::DrawView()
	{
		if (!m_View || !m_Box)
			return;

		PostProcessSettings settings;
		settings.Enabled = true;
		Renderer::GetPostProcessStack().Begin(settings);

		PerspectiveCamera camera(k_Fov, m_AspectRatio, 0.1f, 100.0f);
		camera.SetPosition(glm::vec3(0.0f, 2.0f, 3.0f));
		camera.SetUp(glm::vec3(0.0f, 1.0f, 0.0f));
		camera.SetTarget(glm::vec3(0.0f, 1.2f, -8.0f));
		m_View->BeginScene(camera);
		m_View->Clear(glm::vec4(0.04f, 0.06f, 0.1f, 1.0f));
		m_View->SetAmbientLight(glm::vec3(0.6f, 0.7f, 0.9f), 0.35f);

		DirectionalLight sun;
		sun.Direction = glm::normalize(glm::vec3(0.35f + 0.15f * std::sin(m_Time * 0.4f), -1.0f, -0.4f));
		sun.Color = glm::vec3(1.0f, 0.9f, 0.7f);
		sun.Intensity = 2.0f;
		sun.CastShadows = true;
		m_View->SubmitLight(sun);

		auto block = [this](const glm::vec3& center, const glm::vec3& half, const glm::vec4& color)
		{
			m_View->SubmitMesh(m_Box, glm::translate(glm::mat4(1.0f), center) * glm::scale(glm::mat4(1.0f), half * 2.0f), color);
		};
		block(glm::vec3(0.0f, -0.25f, -6.0f), glm::vec3(14.0f, 0.25f, 14.0f), glm::vec4(0.55f, 0.52f, 0.48f, 1.0f));
		block(glm::vec3(0.0f, 3.0f, -12.25f), glm::vec3(14.0f, 6.0f, 0.25f), glm::vec4(0.8f, 0.75f, 0.7f, 1.0f));
		block(glm::vec3(-2.5f, 1.5f, -5.0f), glm::vec3(0.4f, 1.5f, 0.4f), glm::vec4(0.1f, 0.1f, 0.12f, 1.0f));
		block(glm::vec3(2.5f, 1.5f, -8.0f), glm::vec3(0.4f, 1.5f, 0.4f), glm::vec4(0.1f, 0.1f, 0.12f, 1.0f));
		for (int slat = -6; slat <= 6; ++slat)
			block(glm::vec3(static_cast<float>(slat) * 1.4f, 6.5f, -5.0f), glm::vec3(0.4f, 0.1f, 9.0f), glm::vec4(0.3f, 0.25f, 0.2f, 1.0f));

		FogVolume haze;
		haze.Mode = FogVolumeMode::Raymarch;
		haze.HalfExtents = glm::vec3(12.0f, 4.0f, 6.0f);
		haze.Density = 0.07f;
		haze.EdgeFade = 1.5f;
		haze.Albedo = glm::vec3(1.0f, 0.95f, 0.85f);
		haze.Anisotropy = 0.5f;
		haze.LightIntensity = 2.5f;
		haze.RaymarchSteps = 24;
		haze.NoiseScale = 4.0f;
		haze.NoiseVelocity = glm::vec3(0.3f, 0.0f, 0.1f);
		haze.NoiseMin = 0.4f;
		haze.NoiseMax = 1.0f;
		m_View->SetFogVolumeTime(m_Time);
		m_View->SubmitFogVolume(haze, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 3.0f, -6.0f)));

		m_View->EndScene();
		Renderer::GetPostProcessStack().End();
	}

	void RaymarchFogTest::Cleanup()
	{
		m_Alive.reset();
		m_Steps.clear();
		m_Step = 0;
		DestroyAndDelete(m_Alpha);
		DestroyAndDelete(m_Custom);
		DestroyAndDelete(m_CustomShader);
		DestroyAndDelete(m_Box);
		DestroyAndDelete(m_Quad);
		DestroyAndDelete(m_Target);
		DestroyAndDelete(m_Large);
		for (Renderer3D** renderer : { &m_Renderer, &m_Off, &m_View })
		{
			if (*renderer)
			{
				(*renderer)->Shutdown();
				delete *renderer;
				*renderer = nullptr;
			}
		}
		m_Checks.clear();
	}

	void RaymarchFogTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
	}

	void RaymarchFogTest::ImGuiRender()
	{
		ImGui::TextWrapped("Raymarch fog volumes: the checks run on 64 x 36 off-screen targets; the window shows a haze box with sun shafts through a slatted roof.");
		GraphicsTest::ImGuiRender();

		ImGui::Separator();
		for (const CheckResult& result : m_Checks)
			ImGui::TextColored(result.Passed ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s %s", result.Passed ? "PASS" : "FAIL", result.Name.c_str());
	}

}
