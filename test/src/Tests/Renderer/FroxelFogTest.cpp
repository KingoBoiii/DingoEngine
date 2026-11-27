#include "FroxelFogTest.h"

#include "DingoEngine/Graphics/Renderer3DAccess.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <limits>
#include <string>
#include <string_view>

namespace Dingo
{

	namespace
	{
		constexpr uint32_t k_FramesBeforeChecks = 10;
		constexpr glm::uvec3 k_Grid{ 32, 18, 16 };
		constexpr float k_Fov = 60.0f;
		constexpr float k_Aspect = 32.0f / 18.0f;
		constexpr float k_Byte = 1.0f / 255.0f;
		constexpr uint32_t k_CentreX = k_Grid.x / 2;
		constexpr uint32_t k_CentreY = k_Grid.y / 2;
		const glm::vec3 k_Forward(0.0f, 0.0f, -1.0f);

		using Scene = FroxelFogTest::Scene;
		using Sun = FroxelFogTest::Sun;

		float SliceDepth(const VolumetricFog& fog, float boundary)
		{
			return fog.NearDistance * std::pow(fog.MaxDistance / fog.NearDistance, boundary / static_cast<float>(k_Grid.z));
		}

		float SliceDepth(float boundary)
		{
			return SliceDepth(VolumetricFog{}, boundary);
		}

		glm::vec3 FroxelRay(uint32_t x, uint32_t y)
		{
			const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(k_Grid.x);
			const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(k_Grid.y);
			const float tanHalf = std::tan(glm::radians(k_Fov * 0.5f));
			return glm::vec3((2.0f * u - 1.0f) * tanHalf * k_Aspect, (1.0f - 2.0f * v) * tanHalf, -1.0f);
		}

		float HenyeyGreenstein(float cosTheta, float g)
		{
			const float g2 = g * g;
			const float denominator = 1.0f + g2 - 2.0f * g * cosTheta;
			return (1.0f - g2) / (4.0f * glm::pi<float>() * denominator * std::sqrt(denominator));
		}

		float MediumDensity(const VolumetricFog& fog, float worldY)
		{
			return fog.Density * std::exp(-fog.HeightFalloff * (std::max)(worldY - fog.BaseHeight, 0.0f));
		}

		// What the two kernels compute for one column, step for step, in float.
		std::vector<glm::vec4> EmulateColumn(const Scene& scene, uint32_t x, uint32_t y)
		{
			const VolumetricFog& fog = scene.Fog;
			const glm::vec3 ray = FroxelRay(x, y);
			const float rayLength = glm::length(ray);
			const glm::vec3 direction = ray / rayLength;

			std::vector<glm::vec4> column;
			glm::vec3 scattered(0.0f);
			float transmittance = 1.0f;
			float previous = 0.0f;
			for (uint32_t k = 0; k < k_Grid.z; ++k)
			{
				const glm::vec3 world = scene.Eye + ray * SliceDepth(fog, static_cast<float>(k) + 0.5f);
				const float sigmaT = MediumDensity(fog, world.y);
				glm::vec3 light = scene.AmbientColor * scene.AmbientIntensity * fog.AmbientIntensity;
				for (const Sun& sun : scene.Suns)
				{
					const float phase = glm::pi<float>() * HenyeyGreenstein(glm::dot(glm::normalize(sun.Direction), -direction), fog.Anisotropy);
					light += phase * sun.Color * sun.Intensity * fog.LightIntensity;
				}
				const glm::vec3 scatter = fog.Albedo * sigmaT * light;

				const float end = SliceDepth(fog, static_cast<float>(k + 1));
				const float length = (end - previous) * rayLength;
				previous = end;
				const float step = std::exp(-sigmaT * length);
				const float factor = sigmaT > 1e-6f ? (1.0f - step) / sigmaT : length;
				scattered += transmittance * scatter * factor;
				transmittance *= step;
				column.push_back(glm::vec4(scattered, transmittance));
			}
			return column;
		}

		float TrueTransmittance(const VolumetricFog& fog, const glm::vec3& eye, uint32_t x, uint32_t y, float endDepth)
		{
			const glm::vec3 ray = FroxelRay(x, y);
			const float rayLength = glm::length(ray);
			constexpr int k_Steps = 40000;
			const float dt = endDepth / static_cast<float>(k_Steps);
			double opticalDepth = 0.0;
			for (int i = 0; i < k_Steps; ++i)
				opticalDepth += static_cast<double>(MediumDensity(fog, eye.y + ray.y * (static_cast<float>(i) + 0.5f) * dt) * rayLength * dt);
			return static_cast<float>(std::exp(-opticalDepth));
		}

		Scene UniformScene(float density, bool temporal)
		{
			Scene scene;
			scene.AmbientIntensity = 0.5f;
			scene.Fog.Density = density;
			scene.Fog.Anisotropy = 0.0f;
			scene.Fog.Temporal = temporal;
			return scene;
		}

		glm::vec4 UniformExpected(float density, float ambient, uint32_t slice, uint32_t x, uint32_t y)
		{
			const float alpha = std::exp(-density * SliceDepth(static_cast<float>(slice + 1)) * glm::length(FroxelRay(x, y)));
			return glm::vec4(glm::vec3(ambient * (1.0f - alpha)), alpha);
		}

		float Finite(float value)
		{
			return std::isfinite(value) ? value : 1e9f;
		}

		float MaxChannel(const glm::vec4& value)
		{
			const float worst = (std::max)({ std::abs(value.r), std::abs(value.g), std::abs(value.b), std::abs(value.a) });
			return std::isfinite(value.r + value.g + value.b + value.a) ? worst : 1e9f;
		}

		bool IsSlice(const TexturePixels& image)
		{
			return !image.Data.empty() && image.Width == k_Grid.x && image.Height == k_Grid.y;
		}

		float WorstTexel(const TexturePixels& image, const std::function<glm::vec4(uint32_t, uint32_t)>& expected)
		{
			if (!IsSlice(image))
				return 1e9f;
			float worst = 0.0f;
			for (uint32_t y = 0; y < k_Grid.y; ++y)
			{
				for (uint32_t x = 0; x < k_Grid.x; ++x)
					worst = (std::max)(worst, MaxChannel(image.GetPixel(x, y) - expected(x, y)));
			}
			return worst;
		}

		float WorstDifference(const TexturePixels& a, const TexturePixels& b)
		{
			if (!IsSlice(a) || !IsSlice(b))
				return 1e9f;
			return WorstTexel(a, [&b](uint32_t x, uint32_t y) { return b.GetPixel(x, y); });
		}

		float WorstDifference(const std::vector<TexturePixels>& a, const std::vector<TexturePixels>& b)
		{
			if (a.empty() || a.size() != b.size())
				return 1e9f;
			float worst = 0.0f;
			for (size_t i = 0; i < a.size(); ++i)
				worst = (std::max)(worst, WorstDifference(a[i], b[i]));
			return worst;
		}

		float Relative(const glm::vec3& got, const glm::vec3& want)
		{
			float worst = 0.0f;
			for (int i = 0; i < 3; ++i)
			{
				const float value = std::abs(got[i] - want[i]) / (std::max)(std::abs(want[i]), 1e-5f);
				worst = (std::max)(worst, std::isfinite(value) ? value : 1e9f);
			}
			return worst;
		}

		std::string Rgb(const glm::vec3& color)
		{
			return std::format("{:.5f},{:.5f},{:.5f}", color.r, color.g, color.b);
		}

		struct ReadSet
		{
			std::vector<TexturePixels> Images;
			size_t Remaining = 0;
			std::function<void(const std::vector<TexturePixels>&)> Done;
		};

		std::shared_ptr<ReadSet> MakeReadSet(size_t count, std::function<void(const std::vector<TexturePixels>&)> done)
		{
			auto set = std::make_shared<ReadSet>();
			set->Images.resize(count);
			set->Remaining = count;
			set->Done = std::move(done);
			return set;
		}

		void ReadInto(const std::shared_ptr<ReadSet>& set, size_t index, Texture* texture, uint32_t slice, const std::weak_ptr<int>& alive)
		{
			if (!texture)
			{
				if (--set->Remaining == 0)
					set->Done(set->Images);
				return;
			}
			texture->ReadSubresource(0, slice, [alive, set, index](const TexturePixels& pixels)
			{
				if (alive.expired())
					return;
				set->Images[index] = pixels;
				if (--set->Remaining == 0)
					set->Done(set->Images);
			});
		}

		void ReadSlices(Texture* texture, const std::vector<uint32_t>& slices, const std::weak_ptr<int>& alive, std::function<void(const std::vector<TexturePixels>&)> done)
		{
			const std::shared_ptr<ReadSet> set = MakeReadSet(slices.size(), std::move(done));
			for (size_t i = 0; i < slices.size(); ++i)
				ReadInto(set, i, texture, slices[i], alive);
		}

		glm::vec4 TexelAt(const TexturePixels& image, uint32_t x, uint32_t y)
		{
			return IsSlice(image) ? image.GetPixel(x, y) : glm::vec4(-10.0f);
		}

		const glm::vec3 k_Eye(0.0f, 1.0f, 0.0f);
		constexpr float k_Boundary = 8.0f;
		constexpr float k_AppliedDensity = 0.05f;
		constexpr float k_AppliedAmbient = 0.5f;
		constexpr float k_AppliedTolerance = 4e-3f;

		struct Worst
		{
			float Value = 1e9f;
			uint32_t X = 0;
			uint32_t Y = 0;
			glm::vec3 Got{ 0.0f };
			glm::vec3 Want{ 0.0f };
		};

		Worst WorstColor(const TexturePixels& image, const std::function<glm::vec3(uint32_t, uint32_t)>& expected)
		{
			Worst worst;
			if (!IsSlice(image))
				return worst;
			worst.Value = 0.0f;
			for (uint32_t y = 0; y < k_Grid.y; ++y)
			{
				for (uint32_t x = 0; x < k_Grid.x; ++x)
				{
					const glm::vec3 got = glm::vec3(image.GetPixel(x, y));
					const glm::vec3 want = expected(x, y);
					const glm::vec3 difference = glm::abs(got - want);
					const float value = (std::max)({ difference.x, difference.y, difference.z, std::isfinite(got.x + got.y + got.z) ? 0.0f : 1e9f });
					if (value > worst.Value)
						worst = { value, x, y, got, want };
				}
			}
			return worst;
		}

		std::string Describe(const Worst& worst)
		{
			return std::format("worst {:.5f} at ({}, {}): got {}, want {}", worst.Value, worst.X, worst.Y, Rgb(worst.Got), Rgb(worst.Want));
		}

		glm::vec3 PixelRgb(const TexturePixels& image, uint32_t x, uint32_t y)
		{
			return glm::vec3(TexelAt(image, x, y));
		}

		// What the volume holds for the surface at slice boundary `boundary` of the uniform medium of the
		// applied checks: rgb = L, a = T.
		glm::vec4 FogAt(float boundary, uint32_t x, uint32_t y)
		{
			return UniformExpected(k_AppliedDensity, k_AppliedAmbient, static_cast<uint32_t>(boundary) - 1, x, y);
		}

		glm::vec3 Fogged(const glm::vec3& surface, float boundary, uint32_t x, uint32_t y)
		{
			const glm::vec4 fog = FogAt(boundary, x, y);
			return surface * fog.a + glm::vec3(fog);
		}

		VolumetricFog AppliedFog()
		{
			return UniformScene(k_AppliedDensity, false).Fog;
		}

		void Capture(Framebuffer* target, const std::shared_ptr<ReadSet>& set, size_t index, const std::weak_ptr<int>& alive)
		{
			ReadInto(set, index, target ? target->GetAttachment(0) : nullptr, 0, alive);
		}

		float QuadHeight(float depth)
		{
			return std::tan(glm::radians(k_Fov * 0.5f)) * depth * 2.0f * 1.25f;
		}

		void AppliedLight(Renderer3D& renderer)
		{
			renderer.SetAmbientLight(glm::vec3(1.0f), k_AppliedAmbient);
		}

		void ApplyFog(Renderer3D& renderer)
		{
			renderer.SetVolumetricFog(AppliedFog());
		}

		Worst WorstEqual(const TexturePixels& a, const TexturePixels& b)
		{
			return WorstColor(a, [&b](uint32_t x, uint32_t y) { return PixelRgb(b, x, y); });
		}

		bool IsUnused(const Texture* texture)
		{
			return texture && (texture->GetParams().Width == 1 && texture->GetParams().Height == 1 && texture->GetParams().Depth == 1);
		}

		void WriteFlatPanorama(const std::filesystem::path& file, const glm::vec3& color)
		{
			constexpr uint32_t k_Width = 16;
			constexpr uint32_t k_Height = 8;
			const float brightest = (std::max)({ color.r, color.g, color.b });
			int exponent = 0;
			const float scale = std::frexp(brightest, &exponent) * 256.0f / brightest;
			const std::array<uint8_t, 4> rgbe = {
				static_cast<uint8_t>(color.r * scale), static_cast<uint8_t>(color.g * scale), static_cast<uint8_t>(color.b * scale), static_cast<uint8_t>(exponent + 128)
			};
			std::ofstream out(file, std::ios::binary);
			const std::string header = std::format("#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y {} +X {}\n", k_Height, k_Width);
			out.write(header.data(), static_cast<std::streamsize>(header.size()));
			for (uint32_t i = 0; i < k_Width * k_Height; ++i)
				out.write(reinterpret_cast<const char*>(rgbe.data()), 4);
		}

		constexpr const char* k_CustomShader = R"(
#type vertex
#version 450
#include <DingoEngine/Renderer3DCamera.glsl>

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in vec2 a_TexCoord;

layout(location = 0) out vec3 v_WorldPosition;

void main()
{
	gl_Position = ViewProjection * vec4(a_Position, 1.0);
	v_WorldPosition = a_Position;
}

#type fragment
#version 450
#include <DingoEngine/Renderer3DCamera.glsl>
#include <DingoEngine/Volumetrics.glsl>

layout(location = 0) in vec3 v_WorldPosition;
layout(location = 0) out vec4 o_Color;

void main()
{
	vec3 color = vec3(0.7, 0.3, 0.2);
	if (VolumeParams.x > 0.5)
		color = DE_ApplyVolumetricFog(color, DE_SampleVolumetricFog(ViewProjection, VolumeParams, VolumeGrid, v_WorldPosition), 0);
	o_Color = vec4(color, 1.0);
}
)";
	}

	void FroxelFogTest::Initialize()
	{
		m_Checks.clear();
		m_ChecksDone = false;
		m_FramesBeforeChecks = 0;
		m_Stage = Stage::Immediate;
		m_StageFrame = 0;
		m_Reference.clear();
		m_Still.clear();
		m_CostFrames = 0;
		m_CostLogged = false;
		m_Alive = std::make_shared<int>(0);

		Renderer3DParams probe;
		probe.Capabilities.Volumetrics = true;
		probe.Capabilities.VolumetricGrid = k_Grid;
		m_Probe = Renderer3D::Create(probe);

		Renderer3DParams view;
		view.Capabilities.Volumetrics = true;
		view.Capabilities.VolumetricGrid = glm::uvec3(64, 36, 32);
		m_Renderer = Renderer3D::Create(view);

		m_Target = Framebuffer::Create(FramebufferParams()
			.SetDebugName("FroxelFogTest target")
			.SetWidth(64)
			.SetHeight(36)
			.SetEnableDepth(true)
			.AddAttachment({ TextureFormat::RGBA16F }));
		m_Box = Mesh::CreateBox(1.0f, 1.0f, 1.0f);
		m_Sphere = Mesh::CreateSphere(0.5f, 24, 32);

		m_ViewSkyReady = false;
		if (m_Renderer)
		{
			m_ViewPbr = m_Renderer->CreatePbrMaterial(PbrMaterialParams()
				.SetMaterial(MaterialParams().SetDebugName("FroxelFogTest view pbr"))
				.SetBaseColor(glm::vec4(0.3f, 0.6f, 0.9f, 1.0f))
				.SetRoughness(0.4f));
			m_ViewGlass = m_Renderer->CreateLitMaterial(MaterialParams().SetDebugName("FroxelFogTest view glass").SetTranslucent(true).SetBlendMode(BlendMode::Alpha));

			auto glow = [](const char* name, ParticleBlend blend, const glm::vec4& color)
			{
				return ParticleEffect::Create(ParticleEffectParams()
					.SetDebugName(name)
					.SetRate(1.0f)
					.SetLifetime(4.0f, 4.0f)
					.SetSpeed(0.0f, 0.0f)
					.SetCapacity(8)
					.SetStartSize(2.0f, 2.0f)
					.SetColors({ { 0.0f, color }, { 1.0f, color } })
					.SetBlend(blend)
					.SetSoftDistance(0.0f));
			};
			m_ViewAlphaEffect = glow("FroxelFogTest view alpha glow", ParticleBlend::Alpha, glm::vec4(1.0f, 0.6f, 0.2f, 0.7f));
			m_ViewAdditiveEffect = glow("FroxelFogTest view additive glow", ParticleBlend::Additive, glm::vec4(0.3f, 0.6f, 1.0f, 0.7f));
			if (m_ViewAlphaEffect)
				m_ViewAlphaEmitter = m_Renderer->CreateParticleEmitter(m_ViewAlphaEffect);
			if (m_ViewAdditiveEffect)
				m_ViewAdditiveEmitter = m_Renderer->CreateParticleEmitter(m_ViewAdditiveEffect);
		}

		m_ApplySteps.clear();
		m_ApplyStep = 0;
		m_GridBefore.clear();
		m_IntegratedBefore = nullptr;

		Renderer3DParams off;
		off.Capabilities.Volumetrics = false;
		m_Off = Renderer3D::Create(off);
		Renderer3DParams idle;
		idle.Capabilities.Volumetrics = true;
		idle.Capabilities.VolumetricGrid = k_Grid;
		m_Idle = Renderer3D::Create(idle);

		m_ApplyTarget = Framebuffer::Create(FramebufferParams()
			.SetDebugName("FroxelFogTest apply target")
			.SetWidth(static_cast<int32_t>(k_Grid.x))
			.SetHeight(static_cast<int32_t>(k_Grid.y))
			.SetEnableDepth(true)
			.AddAttachment({ TextureFormat::RGBA16F }));

		m_Quad = Mesh::Create(
			{
				{ glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f) },
				{ glm::vec3(0.5f, -0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f) },
				{ glm::vec3(0.5f, 0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f) },
				{ glm::vec3(-0.5f, 0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f) },
			},
			{ 0, 1, 2, 0, 2, 3 });

		if (m_Probe)
		{
			auto lit = [this](const char* name, BlendMode blend)
			{
				return m_Probe->CreateLitMaterial(MaterialParams().SetDebugName(name).SetTranslucent(true).SetBlendMode(blend));
			};
			m_LitAdditive = lit("FroxelFogTest additive", BlendMode::Additive);
			m_LitMultiply = lit("FroxelFogTest multiply", BlendMode::Multiply);
			m_LitAlpha = lit("FroxelFogTest alpha", BlendMode::Alpha);
			m_Pbr = m_Probe->CreatePbrMaterial(PbrMaterialParams()
				.SetMaterial(MaterialParams().SetDebugName("FroxelFogTest pbr").SetCullMode(CullMode::None))
				.SetBaseColor(glm::vec4(0.8f, 0.6f, 0.4f, 1.0f))
				.SetRoughness(0.5f));
			m_PbrAlpha = m_Probe->CreatePbrMaterial(PbrMaterialParams()
				.SetMaterial(MaterialParams().SetDebugName("FroxelFogTest pbr alpha").SetCullMode(CullMode::None).SetTranslucent(true).SetBlendMode(BlendMode::Alpha))
				.SetBaseColor(glm::vec4(0.9f, 0.3f, 0.2f, 0.5f))
				.SetRoughness(0.5f));

			m_CustomShader = Shader::CreateFromSource("FroxelFogCustom", k_CustomShader);
			if (m_CustomShader && m_CustomShader->IsValid())
				m_Custom = Material::Create(MaterialParams().SetDebugName("FroxelFogTest custom").SetShader(m_CustomShader).SetCullMode(CullMode::None));

			const glm::vec4 dot(1.0f, 0.6f, 0.3f, 1.0f);
			auto effect = [dot](const char* name, ParticleBlend blend)
			{
				return ParticleEffect::Create(ParticleEffectParams()
					.SetDebugName(name)
					.SetRate(0.0f)
					.SetLifetime(10.0f, 10.0f)
					.SetSpeed(0.0f, 0.0f)
					.SetCapacity(4)
					.SetStartSize(14.0f, 14.0f)
					.SetColors({ { 0.0f, dot }, { 1.0f, dot } })
					.SetBlend(blend)
					.SetSoftDistance(0.0f));
			};
			m_AlphaEffect = effect("FroxelFogTest alpha dot", ParticleBlend::Alpha);
			m_AdditiveEffect = effect("FroxelFogTest additive dot", ParticleBlend::Additive);
			if (m_AlphaEffect)
				m_AlphaEmitter = m_Probe->CreateParticleEmitter(m_AlphaEffect);
			if (m_AdditiveEffect)
				m_AdditiveEmitter = m_Probe->CreateParticleEmitter(m_AdditiveEffect);
		}

		m_Folder = TestTempDirectory("DingoFroxelFogTest");
		std::error_code error;
		std::filesystem::create_directories(m_Folder, error);
		const std::filesystem::path panorama = m_Folder / "sky.hdr";
		WriteFlatPanorama(panorama, glm::vec3(0.4f, 0.5f, 0.6f));
		m_Environment = Environment::Create(EnvironmentParams().SetPath(panorama).SetDebugName("FroxelFogTest sky"));
	}

	Texture* FroxelFogTest::Integrated() const
	{
		return Internal::Renderer3DAccess::VolumetricIntegrated(*m_Probe);
	}

	Texture* FroxelFogTest::Scatter() const
	{
		return Internal::Renderer3DAccess::VolumetricScatter(*m_Probe);
	}

	Renderer3D::Statistics FroxelFogTest::RenderScene(const Scene& scene, Renderer3D* renderer)
	{
		Renderer3D& target = renderer ? *renderer : *m_Probe;
		Framebuffer* previous = Renderer::GetRenderTarget();
		Renderer::SetRenderTarget(m_Target);

		PerspectiveCamera camera(k_Fov, k_Aspect, 0.1f, 100.0f);
		camera.SetPosition(scene.Eye);
		camera.SetUp(glm::vec3(0.0f, 1.0f, 0.0f));
		camera.SetTarget(scene.Eye + k_Forward);

		target.BeginScene(camera.GetViewProjectionMatrix());
		target.Clear(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
		target.SetAmbientLight(scene.AmbientColor, scene.AmbientIntensity);
		for (const Sun& sun : scene.Suns)
		{
			DirectionalLight light;
			light.Direction = glm::normalize(sun.Direction);
			light.Color = sun.Color;
			light.Intensity = sun.Intensity;
			light.CastShadows = sun.Casts;
			light.ShadowStrength = sun.Strength;
			target.SubmitLight(light);
		}
		target.SetVolumetricFog(scene.Fog);
		target.SubmitMesh(m_Box, glm::translate(glm::mat4(1.0f), scene.Eye + glm::vec3(0.0f, 0.0f, -10.0f)), glm::vec4(1.0f));
		if (scene.Slab)
		{
			const glm::vec3 centre = scene.WideSlab ? glm::vec3(0.0f, 5.0f, -60.0f) : glm::vec3(0.0f, 5.0f, -20.0f);
			const glm::vec3 size = scene.WideSlab ? glm::vec3(600.0f, 0.5f, 600.0f) : glm::vec3(24.0f, 0.5f, 30.0f);
			target.SubmitMesh(m_Box, glm::scale(glm::translate(glm::mat4(1.0f), centre), size), glm::vec4(1.0f));
		}
		target.EndScene();

		Renderer::SetRenderTarget(previous);
		return target.GetStatistics();
	}

	void FroxelFogTest::Update(float deltaTime)
	{
		m_ViewDelta = deltaTime;
		if (!m_ChecksDone && !Renderer::IsFrameSkipped() && ++m_FramesBeforeChecks > k_FramesBeforeChecks)
		{
			m_ChecksDone = true;
			RunChecks();
		}
		else if (m_Stage == Stage::Temporal && !Renderer::IsFrameSkipped())
			StepTemporal();
		else if (m_Stage == Stage::Apply && !Renderer::IsFrameSkipped())
			StepApply();

		Renderer2D& renderer2D = Application::Get().GetRenderer2D();
		renderer2D.BeginScene(glm::ortho(-m_AspectRatio, m_AspectRatio, -1.0f, 1.0f, -1.0f, 1.0f));
		renderer2D.Clear(m_ClearColor);
		renderer2D.EndScene();
		DrawView();
		LogCost();
	}

	void FroxelFogTest::LogCost()
	{
		if (m_Stage != Stage::Done || m_CostLogged || Renderer::IsFrameSkipped() || ++m_CostFrames < 150)
			return;
		m_CostLogged = true;
		for (const GpuTimerStats& timer : Renderer::GetGpuTimers())
		{
			if (timer.Name && std::string_view(timer.Name) == "Volumetrics")
			{
				const glm::uvec3 grid = m_Renderer ? m_Renderer->GetVolumetricGrid() : glm::uvec3(0);
				DE_INFO("Froxel cost: Volumetrics {:.3f} ms mean, {:.3f} ms last, {:.3f} ms max per frame over {} frames, {} scenes a frame, grid {}x{}x{}", timer.MeanMs, timer.LastMs, timer.MaxMs, timer.Samples, 4, grid.x, grid.y, grid.z);
				return;
			}
		}
		DE_INFO("Froxel cost: the Volumetrics GPU timer has no samples on this backend.");
	}

	void FroxelFogTest::RunChecks()
	{
		if (!m_Probe || !m_Renderer || !m_Target || !m_Box || !m_Sphere)
		{
			Check(false, "Froxel: the renderers, the target and the meshes are created");
			return;
		}

		CheckUniform();
		CheckHeightFalloff();
		CheckPhase();
		CheckShafts();
		CheckTint();
		CheckOutsideShadow();
		RenderReference();
		m_Stage = Stage::Temporal;
	}

	void FroxelFogTest::CheckUniform()
	{
		constexpr float k_Density = 0.05f;
		constexpr float k_Ambient = 0.5f;
		const Renderer3D::Statistics stats = RenderScene(UniformScene(k_Density, false));
		const glm::uvec3 gridSize = Integrated() ? glm::uvec3(Integrated()->GetParams().Width, Integrated()->GetParams().Height, Integrated()->GetParams().Depth) : glm::uvec3(0);
		const bool ready = stats.Volumetrics && stats.VolumetricDispatches == 2 && !stats.VolumetricTemporal && gridSize == k_Grid;

		const std::vector<uint32_t> slices = { k_Grid.z / 2, k_Grid.z - 1 };
		ReadSlices(Integrated(), slices, m_Alive, [this, ready, slices](const std::vector<TexturePixels>& images)
		{
			float worst = 0.0f;
			for (size_t i = 0; i < slices.size(); ++i)
				worst = (std::max)(worst, WorstTexel(images[i], [&](uint32_t x, uint32_t y) { return UniformExpected(k_Density, k_Ambient, slices[i], x, y); }));
			Check(ready && worst < 2e-3f, std::format("Froxel 3: slices D/2 and D-1 of a uniform ambient-only volume are alpha = e^(-sigma z |ray|) and rgb = A (1 - alpha) at every texel (worst {:.5f})", worst));
		});
	}

	void FroxelFogTest::CheckHeightFalloff()
	{
		Scene scene = UniformScene(0.05f, false);
		scene.Fog.HeightFalloff = 0.15f;
		RenderScene(scene);

		const std::vector<uint32_t> slices = { k_Grid.z / 2, k_Grid.z - 1 };
		ReadSlices(Integrated(), slices, m_Alive, [this, scene, slices](const std::vector<TexturePixels>& images)
		{
			float worstWalk = 0.0f;
			float worstTrue = 0.0f;
			std::string detail;
			for (const uint32_t row : { 0u, k_Grid.y - 1 })
			{
				const std::vector<glm::vec4> column = EmulateColumn(scene, k_CentreX, row);
				for (size_t i = 0; i < slices.size(); ++i)
				{
					const glm::vec4 got = TexelAt(images[i], k_CentreX, row);
					worstWalk = (std::max)(worstWalk, MaxChannel(got - column[slices[i]]));
					const float truth = TrueTransmittance(scene.Fog, scene.Eye, k_CentreX, row, SliceDepth(static_cast<float>(slices[i] + 1)));
					worstTrue = (std::max)(worstTrue, Finite(std::abs(got.a - truth)));
					detail += std::format(" [row {} slice {} alpha {:.4f} walk {:.4f} true {:.4f}]", row, slices[i], got.a, column[slices[i]].a, truth);
				}
			}
			Check(worstWalk < 3e-3f, std::format("Froxel 4a: the centre column's top and bottom rows equal the CPU walk of the same froxels under a height falloff (worst {:.5f};{})", worstWalk, detail));
			Check(worstTrue < 0.03f, std::format("Froxel 4b: their alpha is the true transmittance of the ray within 0.03 (worst {:.5f})", worstTrue));
		});
	}

	void FroxelFogTest::CheckPhase()
	{
		constexpr float k_Density = 0.03f;
		const glm::vec3 color(1.0f, 0.8f, 0.6f);
		auto sunScene = [&](float travelZ, float anisotropy)
		{
			Scene scene = UniformScene(k_Density, false);
			scene.AmbientIntensity = 0.0f;
			scene.Fog.Anisotropy = anisotropy;
			Sun sun;
			sun.Direction = glm::vec3(0.0f, 0.0f, travelZ);
			sun.Color = color;
			sun.Intensity = 2.0f;
			scene.Suns.push_back(sun);
			return scene;
		};
		const Scene toward = sunScene(1.0f, 0.7f);
		const Scene away = sunScene(-1.0f, 0.7f);
		const Scene isotropic = sunScene(-1.0f, 0.0f);

		const std::shared_ptr<ReadSet> set = MakeReadSet(4, [this, toward, away, isotropic, color](const std::vector<TexturePixels>& images)
		{
			constexpr uint32_t k_Slice = 3;
			const glm::vec3 gotToward = glm::vec3(TexelAt(images[0], k_CentreX, k_CentreY));
			const glm::vec3 gotAway = glm::vec3(TexelAt(images[1], k_CentreX, k_CentreY));
			const glm::vec3 wantToward = glm::vec3(EmulateColumn(toward, k_CentreX, k_CentreY)[k_Slice]);
			const glm::vec3 wantAway = glm::vec3(EmulateColumn(away, k_CentreX, k_CentreY)[k_Slice]);
			const float worstWalk = (std::max)(Relative(gotToward, wantToward), Relative(gotAway, wantAway));
			Check(worstWalk < 0.01f, std::format("Froxel 5a: a sun the camera looks toward and one it looks away from scatter as the CPU walk (worst {:.4f}; toward {} want {}, away {} want {})", worstWalk, Rgb(gotToward), Rgb(wantToward), Rgb(gotAway), Rgb(wantAway)));

			const glm::vec3 direction = glm::normalize(FroxelRay(k_CentreX, k_CentreY));
			const float wantRatio = HenyeyGreenstein(glm::dot(glm::vec3(0.0f, 0.0f, 1.0f), -direction), 0.7f) / HenyeyGreenstein(glm::dot(glm::vec3(0.0f, 0.0f, -1.0f), -direction), 0.7f);
			const float gotRatio = gotToward.g / gotAway.g;
			Check(std::abs(gotRatio / wantRatio - 1.0f) < 0.01f, std::format("Froxel 5b: toward-sun over away-from-sun is the Henyey-Greenstein ratio (got {:.3f}, want {:.3f})", gotRatio, wantRatio));

			const glm::vec3 gotInject = glm::vec3(TexelAt(images[2], k_CentreX, k_CentreY));
			const glm::vec3 wantInject = k_Density * color * 2.0f * 0.25f;
			Check(Relative(gotInject, wantInject) < 0.01f, std::format("Froxel 5c: with g = 0 the injected scatter is sigma C / 4 (got {}, want {})", Rgb(gotInject), Rgb(wantInject)));

			const glm::vec3 gotIntegrated = glm::vec3(TexelAt(images[3], k_CentreX, k_CentreY));
			const float alpha = std::exp(-k_Density * SliceDepth(static_cast<float>(k_Grid.z)) * glm::length(FroxelRay(k_CentreX, k_CentreY)));
			const glm::vec3 wantIntegrated = color * 2.0f * (1.0f - alpha) * 0.25f;
			Check(Relative(gotIntegrated, wantIntegrated) < 0.01f, std::format("Froxel 5d: with g = 0 the last slice's in-scatter is C (1 - alpha) / 4 (got {}, want {})", Rgb(gotIntegrated), Rgb(wantIntegrated)));
		});

		RenderScene(toward);
		ReadInto(set, 0, Integrated(), 3, m_Alive);
		RenderScene(away);
		ReadInto(set, 1, Integrated(), 3, m_Alive);
		RenderScene(isotropic);
		ReadInto(set, 2, Scatter(), 5, m_Alive);
		ReadInto(set, 3, Integrated(), k_Grid.z - 1, m_Alive);
	}

	void FroxelFogTest::CheckShafts()
	{
		constexpr float k_Density = 0.03f;
		auto shaftScene = [](bool slab, float strength)
		{
			Scene scene = UniformScene(k_Density, false);
			scene.AmbientIntensity = 0.0f;
			Sun sun;
			sun.Direction = glm::vec3(0.1f, -1.0f, 0.05f);
			sun.Casts = true;
			sun.Strength = strength;
			scene.Suns.push_back(sun);
			scene.Slab = slab;
			return scene;
		};

		const std::shared_ptr<ReadSet> set = MakeReadSet(6, [this](const std::vector<TexturePixels>& images)
		{
			const glm::vec3 lit = glm::vec3(TexelAt(images[0], k_CentreX, k_CentreY));
			const glm::vec3 shadowed = glm::vec3(TexelAt(images[2], k_CentreX, k_CentreY));
			const glm::vec3 half = glm::vec3(TexelAt(images[4], k_CentreX, k_CentreY));
			const glm::vec3 open = glm::vec3(TexelAt(images[3], 0, k_CentreY));
			const glm::vec3 openReference = glm::vec3(TexelAt(images[1], 0, k_CentreY));
			const glm::vec3 want(k_Density * 0.25f);

			Check(Relative(lit, want) < 0.02f, std::format("Froxel 6a: with no caster the sun's in-scatter is sigma / 4 (got {}, want {})", Rgb(lit), Rgb(want)));
			Check(shadowed.g < 0.02f * lit.g, std::format("Froxel 6b: a froxel under the slab has no sun in-scatter (got {}, lit {})", Rgb(shadowed), Rgb(lit)));
			Check(Relative(half, lit * 0.5f) < 0.03f, std::format("Froxel 6c: ShadowStrength 0.5 halves the in-scatter there (got {}, half of lit {})", Rgb(half), Rgb(lit * 0.5f)));
			Check(Relative(open, openReference) < 0.03f && open.g > 0.5f * want.g, std::format("Froxel 6d: a froxel outside the slab's shadow is unchanged (got {}, reference {})", Rgb(open), Rgb(openReference)));
		});

		RenderScene(shaftScene(false, 1.0f));
		ReadInto(set, 0, Scatter(), 9, m_Alive);
		ReadInto(set, 1, Scatter(), 11, m_Alive);
		RenderScene(shaftScene(true, 1.0f));
		ReadInto(set, 2, Scatter(), 9, m_Alive);
		ReadInto(set, 3, Scatter(), 11, m_Alive);
		RenderScene(shaftScene(true, 0.5f));
		ReadInto(set, 4, Scatter(), 9, m_Alive);
		ReadInto(set, 5, Scatter(), 11, m_Alive);
	}

	void FroxelFogTest::CheckTint()
	{
		const glm::vec3 tint(0.5f, 2.0f, 0.0f);
		auto tinted = [&](const glm::vec3& value)
		{
			Scene scene = UniformScene(0.04f, false);
			scene.AmbientIntensity = 0.5f;
			Sun sun;
			sun.Direction = glm::vec3(0.0f, 0.0f, 1.0f);
			sun.Color = glm::vec3(1.0f, 0.8f, 0.6f);
			scene.Suns.push_back(sun);
			scene.Fog.ScatterTint = value;
			return scene;
		};

		const std::shared_ptr<ReadSet> set = MakeReadSet(4, [this, tint](const std::vector<TexturePixels>& images)
		{
			float worstScale = 0.0f;
			float worstAlpha = 0.0f;
			std::string detail;
			for (size_t i = 0; i < 2; ++i)
			{
				const glm::vec4 plain = TexelAt(images[i], k_CentreX, k_CentreY);
				const glm::vec4 got = TexelAt(images[i + 2], k_CentreX, k_CentreY);
				const glm::vec3 want = glm::vec3(plain) * tint;
				worstScale = (std::max)(worstScale, (std::max)(Relative(glm::vec3(got.r, got.g, 0.0f), glm::vec3(want.r, want.g, 0.0f)), Finite(std::abs(got.b) / (std::max)(plain.b, 1e-5f))));
				worstAlpha = (std::max)(worstAlpha, Finite(std::abs(got.a - plain.a)));
				detail += std::format(" [{} plain {} tinted {}]", i == 0 ? "inject" : "integrated", Rgb(glm::vec3(plain)), Rgb(glm::vec3(got)));
			}
			Check(worstScale < 0.01f, std::format("Froxel 17a: ScatterTint (0.5, 2, 0) scales the inject and integrated in-scatter per channel (worst {:.4f};{})", worstScale, detail));
			Check(worstAlpha < 1e-5f, std::format("Froxel 17b: ScatterTint leaves the transmittance alone (worst difference {:.6f})", worstAlpha));
		});

		RenderScene(tinted(glm::vec3(1.0f)));
		ReadInto(set, 0, Scatter(), 5, m_Alive);
		ReadInto(set, 1, Integrated(), 8, m_Alive);
		RenderScene(tinted(tint));
		ReadInto(set, 2, Scatter(), 5, m_Alive);
		ReadInto(set, 3, Integrated(), 8, m_Alive);
	}

	void FroxelFogTest::CheckOutsideShadow()
	{
		constexpr float k_Density = 0.03f;
		constexpr float k_Near = 30.0f;
		constexpr float k_Far = 90.0f;
		constexpr uint32_t k_Depth = 32;
		auto sunScene = [&](bool wide, bool casts, float outside)
		{
			Scene scene = UniformScene(k_Density, false);
			scene.AmbientIntensity = 0.0f;
			Sun sun;
			sun.Direction = glm::vec3(0.1f, -1.0f, 0.05f);
			sun.Casts = casts;
			scene.Suns.push_back(sun);
			scene.Slab = wide;
			scene.WideSlab = wide;
			scene.Fog.NearDistance = k_Near;
			scene.Fog.MaxDistance = k_Far;
			scene.Fog.OutsideShadowLight = outside;
			return scene;
		};
		auto depthAt = [&](uint32_t slice) { return k_Near * std::pow(k_Far / k_Near, (static_cast<float>(slice) + 0.5f) / static_cast<float>(k_Depth)); };

		const Renderer3D::Statistics stats = RenderScene(sunScene(true, true, 1.0f), m_Renderer);
		Texture* firstScatter = Internal::Renderer3DAccess::VolumetricScatter(*m_Renderer);
		if (stats.ShadowCascades == 0)
		{
			Check(false, "Froxel 18: the sun draws shadow cascades");
			return;
		}
		const float end = stats.ShadowCascadeEnds[stats.ShadowCascades - 1];
		const float start = stats.ShadowCascades > 1 ? stats.ShadowCascadeEnds[stats.ShadowCascades - 2] : 0.0f;
		const float band = (end - start) * 0.1f;

		std::vector<uint32_t> slices;
		std::vector<float> fade;
		for (uint32_t k = 0; k < k_Depth; ++k)
		{
			const float depth = depthAt(k);
			if (depth >= end - band - 4.0f && depth <= end + 8.0f)
			{
				slices.push_back(k);
				fade.push_back(glm::clamp((depth - (end - band)) / band, 0.0f, 1.0f));
			}
		}
		size_t inTail = 0;
		for (const float f : fade)
			inTail += f > 0.15f && f < 0.85f ? 1 : 0;
		Check(slices.size() >= 4 && inTail >= 1, std::format("Froxel 18a: the sampled slices cover the cascade tail (end {:.1f}, tail {:.2f}, {} slices, {} inside the tail)", end, band, slices.size(), inTail));
		if (slices.size() < 4)
			return;

		struct Series
		{
			const char* Name;
			std::vector<float> Want;
			float Tolerance;
			bool Monotone;
		};
		const size_t count = slices.size();
		const std::vector<float> lit(count, 1.0f);
		std::vector<float> dark(count);
		for (size_t i = 0; i < count; ++i)
			dark[i] = 1.0f - fade[i];
		const std::vector<Series> series = {
			{ "Froxel 18b: a shadow over the whole view fades from dark to lit over the last cascade's tail, with no step at its end", fade, 0.08f, true },
			{ "Froxel 19a: OutsideShadowLight 0 fades the sun from lit to dark over the same tail and stays dark past it", dark, 0.08f, false },
			{ "Froxel 19b: OutsideShadowLight 1 keeps the sun lit across the end of the last cascade", lit, 0.03f, false },
			{ "Froxel 19c: a sun that casts no shadow stays lit whatever OutsideShadowLight is", lit, 0.03f, false },
		};

		const std::shared_ptr<ReadSet> set = MakeReadSet(count * series.size(), [this, count, series](const std::vector<TexturePixels>& images)
		{
			constexpr float k_Lit = 0.03f * 0.25f;
			auto Centre = [](const TexturePixels& image) { return image.Data.empty() ? glm::vec4(-10.0f) : image.GetPixel(image.Width / 2, image.Height / 2); };
			for (size_t s = 0; s < series.size(); ++s)
			{
				float worst = 0.0f;
				float drop = 0.0f;
				std::string values;
				for (size_t i = 0; i < count; ++i)
				{
					const float got = Centre(images[s * count + i]).g / k_Lit;
					worst = (std::max)(worst, Finite(std::abs(got - series[s].Want[i])));
					if (i > 0)
						drop = (std::max)(drop, Finite(Centre(images[s * count + i - 1]).g / k_Lit - got));
					values += std::format(" {:.2f}", got);
				}
				const bool monotone = !series[s].Monotone || drop < 0.03f;
				Check(worst < series[s].Tolerance && monotone, std::format("{} (worst {:.3f}, largest drop {:.3f}; fractions{})", series[s].Name, worst, drop, values));
			}
		});

		for (size_t i = 0; i < count; ++i)
			ReadInto(set, i, firstScatter, slices[i], m_Alive);
		const std::array<Scene, 3> others = { sunScene(false, true, 0.0f), sunScene(false, true, 1.0f), sunScene(false, false, 0.0f) };
		for (size_t s = 0; s < others.size(); ++s)
		{
			RenderScene(others[s], m_Renderer);
			Texture* texture = Internal::Renderer3DAccess::VolumetricScatter(*m_Renderer);
			for (size_t i = 0; i < count; ++i)
				ReadInto(set, (s + 1) * count + i, texture, slices[i], m_Alive);
		}
	}

	void FroxelFogTest::RenderReference()
	{
		RenderScene(UniformScene(0.05f, false));
		ReadSlices(Integrated(), { 4, 8, 12, 15 }, m_Alive, [this](const std::vector<TexturePixels>& images)
		{
			m_Reference = images;
		});
	}

	void FroxelFogTest::StepTemporal()
	{
		const uint32_t t = m_StageFrame++;
		const std::vector<uint32_t> slices = { 4, 8, 12, 15 };
		auto still = [](float density) { return UniformScene(density, true); };
		auto readAlpha = [this](const char* name, float want, float tolerance)
		{
			ReadSlices(Scatter(), { 8 }, m_Alive, [this, name, want, tolerance](const std::vector<TexturePixels>& images)
			{
				const float got = TexelAt(images[0], k_CentreX, k_CentreY).a;
				Check(std::abs(got - want) < tolerance, std::format("{} (got {:.5f}, want {:.5f})", name, got, want));
			});
		};

		if (t < 29)
		{
			RenderScene(still(0.05f));
		}
		else if (t == 29)
		{
			const Renderer3D::Statistics stats = RenderScene(still(0.05f));
			Check(stats.Volumetrics && stats.VolumetricTemporal, "Froxel 7a: a scene after 29 still Temporal frames blends with the previous frame");
			ReadSlices(Integrated(), slices, m_Alive, [this](const std::vector<TexturePixels>& images) { m_Still = images; });
		}
		else if (t == 30)
		{
			RenderScene(still(0.05f));
			ReadSlices(Integrated(), slices, m_Alive, [this](const std::vector<TexturePixels>& images)
			{
				const float againstReference = WorstDifference(m_Still, m_Reference);
				const float consecutive = WorstDifference(images, m_Still);
				Check(againstReference <= k_Byte, std::format("Froxel 7b: after 30 still frames the volume is within 1/255 of Temporal off (worst {:.5f})", againstReference));
				Check(consecutive <= k_Byte, std::format("Froxel 7c: consecutive still frames differ by no more than 1/255 (worst {:.5f})", consecutive));
			});
		}
		else if (t == 31)
		{
			const Renderer3D::Statistics stats = RenderScene(still(0.02f));
			Check(stats.VolumetricTemporal, "Froxel 7d: the frame after a still one blends with it");
			readAlpha("Froxel 7e: a new density blends 0.1 current over 0.9 history", 0.9f * 0.05f + 0.1f * 0.02f, 1e-4f);
		}
		else if (t == 33)
		{
			const Renderer3D::Statistics stats = RenderScene(still(0.02f));
			Check(!stats.VolumetricTemporal, "Froxel 7f: a frame after one with no temporal scene has no history");
			readAlpha("Froxel 7g: without history the new density is taken whole", 0.02f, 1e-4f);
		}
		else if (t == 34)
		{
			RenderScene(still(0.02f));
		}
		else if (t == 35)
		{
			const Renderer3D::Statistics first = RenderScene(still(0.02f));
			ReadSlices(Integrated(), { 8 }, m_Alive, [this](const std::vector<TexturePixels>& images)
			{
				const float worst = WorstTexel(images[0], [](uint32_t x, uint32_t y) { return UniformExpected(0.02f, 0.5f, 8, x, y); });
				Check(worst < 2e-3f, std::format("Froxel 8b: the first scene of a frame is the volume of its own density (worst {:.5f})", worst));
			});
			const Renderer3D::Statistics second = RenderScene(UniformScene(0.05f, true));
			ReadSlices(Integrated(), { 8 }, m_Alive, [this](const std::vector<TexturePixels>& images)
			{
				const float worst = WorstTexel(images[0], [](uint32_t x, uint32_t y) { return UniformExpected(0.05f, 0.5f, 8, x, y); });
				Check(worst < 2e-3f, std::format("Froxel 8c: the second scene equals the Temporal off volume of its density (worst {:.5f})", worst));
			});
			Check(first.VolumetricTemporal && !second.VolumetricTemporal, "Froxel 8a: of two Temporal scenes in one frame only the first blends with the history");
		}
		else if (t == 36)
		{
			const Renderer3D::Statistics stats = RenderScene(still(0.02f));
			Check(stats.VolumetricTemporal, "Froxel 8d: the frame after a pair still has the first scene's history");
			readAlpha("Froxel 8e: the second scene of the pair left the history alone", 0.02f, 1e-4f);
		}
		else if (t == 37)
		{
			const Renderer3D::Statistics stats = RenderScene(still(0.02f));
			Check(stats.VolumetricTemporal, "Froxel 20a: the frame before a reset blends with the history");
			m_Probe->ResetVolumetricHistory();
			Check(Internal::Renderer3DAccess::VolumetricHistoryFrame(*m_Probe) == ~0ull, "Froxel 20b: ResetVolumetricHistory drops the history");
		}
		else if (t == 38)
		{
			const Renderer3D::Statistics stats = RenderScene(still(0.02f));
			Check(stats.Volumetrics && !stats.VolumetricTemporal, "Froxel 20c: the frame after ResetVolumetricHistory has no history");
		}
		else if (t == 39 || t == 40)
		{
			Dingo::Scene scene("Froxel Camera Scene");
			Entity camera = scene.CreateEntity("Camera");
			camera.AddComponent(Transform3DComponent(k_Eye));
			CameraComponent& component = camera.AddComponent<CameraComponent>();
			component.Type = CameraComponent::ProjectionType::Perspective;
			component.FOV = k_Fov;
			component.ResetVolumetricHistory = t == 39;
			FogComponent fog(Fog{}, false);
			fog.Volumetric = still(0.02f).Fog;
			scene.CreateEntity("Fog").AddComponent<FogComponent>(fog);

			SceneRenderer renderer(Application::Get().GetRenderer2D(), *m_Probe);
			renderer.Render(scene, m_ApplyTarget, false);
			const Renderer3D::Statistics stats = m_Probe->GetStatistics();
			if (t == 39)
			{
				Check(stats.Volumetrics && !stats.VolumetricTemporal, "Froxel 20d: a CameraComponent with ResetVolumetricHistory drops the history for its frame");
				Check(!camera.GetComponent<CameraComponent>().ResetVolumetricHistory, "Froxel 20e: the SceneRenderer clears the flag");
			}
			else
			{
				Check(stats.VolumetricTemporal, "Froxel 20f: the frame after a camera reset blends again");
				m_Stage = Stage::Apply;
				BuildApplySteps();
			}
		}
	}

	Renderer3D::Statistics FroxelFogTest::RenderApply(Renderer3D& renderer, const std::function<void(Renderer3D&)>& body, bool orthographic)
	{
		Framebuffer* previous = Renderer::GetRenderTarget();
		Renderer::SetRenderTarget(m_ApplyTarget);

		glm::mat4 viewProjection(1.0f);
		if (orthographic)
		{
			const glm::mat4 view = glm::lookAt(k_Eye, k_Eye + k_Forward, glm::vec3(0.0f, 1.0f, 0.0f));
			viewProjection = glm::ortho(-12.0f, 12.0f, -6.75f, 6.75f, 0.1f, 100.0f) * view;
		}
		else
		{
			PerspectiveCamera camera(k_Fov, k_Aspect, 0.1f, 100.0f);
			camera.SetPosition(k_Eye);
			camera.SetUp(glm::vec3(0.0f, 1.0f, 0.0f));
			camera.SetTarget(k_Eye + k_Forward);
			viewProjection = camera.GetViewProjectionMatrix();
		}

		renderer.BeginScene(viewProjection);
		renderer.Clear(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
		body(renderer);
		renderer.EndScene();

		Renderer::SetRenderTarget(previous);
		return renderer.GetStatistics();
	}

	void FroxelFogTest::SubmitQuad(Renderer3D& renderer, float depth, const glm::vec4& color, Material* material) const
	{
		const float height = QuadHeight(depth);
		const glm::mat4 transform = glm::scale(glm::translate(glm::mat4(1.0f), k_Eye + glm::vec3(0.0f, 0.0f, -depth)), glm::vec3(height * k_Aspect, height, 1.0f));
		renderer.SubmitMesh(m_Quad, transform, color, material);
	}

	void FroxelFogTest::RenderGridQuads(const std::function<void(size_t)>& afterScene)
	{
		const std::array<Material*, 3> materials = { nullptr, m_Pbr, m_Custom };
		for (size_t i = 0; i < materials.size(); ++i)
		{
			RenderApply(*m_Probe, [&](Renderer3D& renderer)
			{
				AppliedLight(renderer);
				ApplyFog(renderer);
				SubmitQuad(renderer, SliceDepth(k_Boundary), i == 0 ? glm::vec4(0.8f, 0.6f, 0.4f, 1.0f) : glm::vec4(1.0f), materials[i]);
			});
			afterScene(i);
		}
	}

	void FroxelFogTest::BuildApplySteps()
	{
		m_ApplyStep = 0;
		if (!m_Probe || !m_Off || !m_Idle || !m_ApplyTarget || !m_Quad || !m_LitAdditive || !m_LitMultiply || !m_LitAlpha || !m_Pbr || !m_PbrAlpha || !m_Custom || !m_Environment || !m_AlphaEmitter || !m_AdditiveEmitter)
		{
			m_ApplySteps = { [this]() { Check(false, "Froxel 1, 2, 9-16: the applied-fog renderers, target, quad, materials, custom shader, sky and emitters are created"); } };
			return;
		}

		m_ApplySteps = {
			[this]() { CheckOff(); },
			[this]() { CheckApplyUniform(); },
			[this]() { CheckTranslucent(); },
			[this]() { CheckParticles(); },
			[this]() { CheckSky(); },
			[this]() { CheckCustomShader(); },
			[this]() { CheckScenePath(); },
			[this]() { CheckOrthographic(); },
			[this]() { CheckInvalid(); },
			[this]() { CheckGridBefore(); },
			[this]() { CheckGridAfter(); },
		};
	}

	void FroxelFogTest::StepApply()
	{
		if (m_ApplyStep < m_ApplySteps.size())
		{
			m_ApplySteps[m_ApplyStep++]();
			return;
		}

		m_Stage = Stage::Done;
		m_ApplySteps.clear();
		if (m_Renderer)
			m_Renderer->SetVolumetricGrid(glm::uvec3(160, 90, 64));
	}

	void FroxelFogTest::CheckOff()
	{
		Fog distance;
		distance.Mode = FogMode::Exponential;
		distance.Density = 0.1f;
		distance.Color = glm::vec3(0.5f, 0.6f, 0.7f);

		auto render = [&](Renderer3D& renderer, bool withFog, bool volumetric, bool cleared)
		{
			return RenderApply(renderer, [&](Renderer3D& r)
			{
				AppliedLight(r);
				if (withFog)
					r.SetFog(distance);
				if (volumetric)
					r.SetVolumetricFog(AppliedFog());
				if (cleared)
					r.ClearVolumetricFog();
				SubmitQuad(r, SliceDepth(k_Boundary), glm::vec4(0.8f, 0.6f, 0.4f, 1.0f));
			});
		};

		const std::shared_ptr<ReadSet> set = MakeReadSet(5, [this](const std::vector<TexturePixels>& images)
		{
			const Worst off = WorstEqual(images[0], images[1]);
			const Worst cleared = WorstEqual(images[0], images[2]);
			const Worst none = WorstEqual(images[0], images[3]);
			Check(off.Value < 1e-4f && cleared.Value < 1e-4f, std::format("Froxel 1b: with Volumetrics off, or on and cleared, a scene with distance fog is the one of a renderer that never set the volume (off {}; cleared {})", Describe(off), Describe(cleared)));
			const Worst disabled = WorstEqual(images[0], images[4]);
			Check(disabled.Value < 1e-4f, std::format("Froxel 1d: a renderer that used the volume and then has Volumetrics switched off draws the same as one that never did ({})", Describe(disabled)));
			Check(none.Value > 0.01f, std::format("Froxel 1c: the distance fog shows in that picture (differs from the unfogged one by {:.4f})", none.Value));
		});

		const Renderer3D::Statistics idle = render(*m_Idle, true, false, false);
		Capture(m_ApplyTarget, set, 0, m_Alive);
		const Renderer3D::Statistics off = render(*m_Off, true, true, false);
		Capture(m_ApplyTarget, set, 1, m_Alive);
		const Renderer3D::Statistics cleared = render(*m_Probe, true, true, true);
		Capture(m_ApplyTarget, set, 2, m_Alive);
		render(*m_Idle, false, false, false);
		Capture(m_ApplyTarget, set, 3, m_Alive);
		const Renderer3D::Statistics used = render(*m_Probe, true, true, false);
		m_Probe->SetVolumetricsEnabled(false);
		const Renderer3D::Statistics disabled = render(*m_Probe, true, true, false);
		Capture(m_ApplyTarget, set, 4, m_Alive);
		m_Probe->SetVolumetricsEnabled(true);
		Check(used.Volumetrics && used.VolumetricDispatches == 2 && !disabled.Volumetrics && disabled.VolumetricDispatches == 0 && disabled.Fogged,
			std::format("Froxel 1e: a used volume dispatches until Volumetrics is switched off and then nothing, keeping the distance fog (used {} {}, off {} {} fogged {})", used.Volumetrics, used.VolumetricDispatches, disabled.Volumetrics, disabled.VolumetricDispatches, disabled.Fogged));

		const bool unused = IsUnused(Internal::Renderer3DAccess::VolumetricIntegrated(*m_Idle)) && IsUnused(Internal::Renderer3DAccess::VolumetricIntegrated(*m_Off));
		Check(idle.Fogged && off.Fogged && cleared.Fogged && !idle.Volumetrics && !off.Volumetrics && !cleared.Volumetrics
			&& idle.VolumetricDispatches == 0 && off.VolumetricDispatches == 0 && cleared.VolumetricDispatches == 0 && unused,
			std::format("Froxel 1a: those scenes keep the distance fog and dispatch nothing, and the renderers that never used the volume hold 1 x 1 x 1 (fogged {} {} {}, volumetric {} {} {}, dispatches {} {} {}, unused {})",
				idle.Fogged, off.Fogged, cleared.Fogged, idle.Volumetrics, off.Volumetrics, cleared.Volumetrics, idle.VolumetricDispatches, off.VolumetricDispatches, cleared.VolumetricDispatches, unused));
	}

	void FroxelFogTest::CheckApplyUniform()
	{
		const glm::vec4 color(0.8f, 0.6f, 0.4f, 1.0f);
		auto render = [&](Material* material, bool volumetric, bool distance)
		{
			return RenderApply(*m_Probe, [&](Renderer3D& r)
			{
				AppliedLight(r);
				if (volumetric)
					ApplyFog(r);
				if (distance)
				{
					Fog fog;
					fog.Mode = FogMode::Exponential;
					fog.Density = k_AppliedDensity;
					fog.Color = glm::vec3(k_AppliedAmbient);
					r.SetFog(fog);
				}
				SubmitQuad(r, SliceDepth(k_Boundary), material ? glm::vec4(1.0f) : color, material);
			});
		};

		const std::shared_ptr<ReadSet> set = MakeReadSet(5, [this](const std::vector<TexturePixels>& images)
		{
			auto fogged = [&](const TexturePixels& got, const TexturePixels& surface)
			{
				return WorstColor(got, [&surface](uint32_t x, uint32_t y) { return Fogged(PixelRgb(surface, x, y), k_Boundary, x, y); });
			};
			const Worst lit = fogged(images[1], images[0]);
			const Worst pbr = fogged(images[4], images[3]);
			const Worst exponential = WorstEqual(images[1], images[2]);
			Check(lit.Value < k_AppliedTolerance, std::format("Froxel 2a: a lit quad in a uniform medium is S T + L ({})", Describe(lit)));
			Check(pbr.Value < k_AppliedTolerance, std::format("Froxel 2b: a PBR quad is S T + L ({})", Describe(pbr)));
			Check(exponential.Value < k_AppliedTolerance, std::format("Froxel 2c: the lit quad equals FogMode::Exponential of the same density and colour ({})", Describe(exponential)));
		});

		render(nullptr, false, false);
		Capture(m_ApplyTarget, set, 0, m_Alive);
		const Renderer3D::Statistics lit = render(nullptr, true, false);
		Capture(m_ApplyTarget, set, 1, m_Alive);
		render(nullptr, false, true);
		Capture(m_ApplyTarget, set, 2, m_Alive);
		render(m_Pbr, false, false);
		Capture(m_ApplyTarget, set, 3, m_Alive);
		const Renderer3D::Statistics pbr = render(m_Pbr, true, false);
		Capture(m_ApplyTarget, set, 4, m_Alive);
		Check(lit.Volumetrics && lit.VolumetricDispatches == 2 && pbr.Volumetrics && pbr.PbrDraws == 1, std::format("Froxel 2d: the fogged scenes ran the volume (lit {} with {} dispatches, PBR {} with {} PBR draws)", lit.Volumetrics, lit.VolumetricDispatches, pbr.Volumetrics, pbr.PbrDraws));
	}

	void FroxelFogTest::CheckTranslucent()
	{
		struct Case
		{
			const char* Name;
			Material* Surface;
			glm::vec4 Color;
			int Blend;
		};
		constexpr int k_Additive = 0;
		constexpr int k_Multiply = 1;
		constexpr float k_Alpha = 0.5f;
		constexpr float k_BehindBoundary = 12.0f;
		const std::array<Case, 4> cases = { {
			{ "an additive lit quad keeps its colour times T over an opaque quad that is fogged itself", m_LitAdditive, glm::vec4(0.6f, 0.4f, 0.2f, 1.0f), k_Additive },
			{ "a multiply lit quad goes toward white by 1 - T", m_LitMultiply, glm::vec4(0.7f, 0.8f, 0.9f, 1.0f), k_Multiply },
			{ "an alpha lit quad takes both the transmittance and the in-scatter", m_LitAlpha, glm::vec4(0.9f, 0.3f, 0.2f, k_Alpha), 2 },
			{ "an alpha PBR quad takes both too", m_PbrAlpha, glm::vec4(1.0f), 2 },
		} };
		const glm::vec4 opaque(0.5f, 0.6f, 0.7f, 1.0f);

		auto render = [&](bool fog, const Case* surface)
		{
			return RenderApply(*m_Probe, [&](Renderer3D& r)
			{
				AppliedLight(r);
				if (fog)
					ApplyFog(r);
				SubmitQuad(r, SliceDepth(k_BehindBoundary), opaque);
				if (surface)
					SubmitQuad(r, SliceDepth(k_Boundary), surface->Color, surface->Surface);
			});
		};

		const std::shared_ptr<ReadSet> set = MakeReadSet(1 + 2 * cases.size(), [this, cases](const std::vector<TexturePixels>& images)
		{
			const TexturePixels& behind = images[0];
			for (size_t i = 0; i < cases.size(); ++i)
			{
				const TexturePixels& baseline = images[1 + 2 * i];
				const TexturePixels& got = images[2 + 2 * i];
				const int blend = cases[i].Blend;
				const Worst worst = WorstColor(got, [&](uint32_t x, uint32_t y)
				{
					const glm::vec4 near = FogAt(k_Boundary, x, y);
					const glm::vec3 opaqueBefore = PixelRgb(behind, x, y);
					const glm::vec3 opaqueFogged = Fogged(opaqueBefore, k_BehindBoundary, x, y);
					const glm::vec3 both = PixelRgb(baseline, x, y);
					if (blend == k_Additive)
						return opaqueFogged + (both - opaqueBefore) * near.a;
					if (blend == k_Multiply)
					{
						const glm::vec3 factor = both / glm::max(opaqueBefore, glm::vec3(1e-3f));
						return opaqueFogged * (factor * near.a + glm::vec3(1.0f - near.a));
					}
					return (both - opaqueBefore * (1.0f - k_Alpha)) * near.a + glm::vec3(near) * k_Alpha + opaqueFogged * (1.0f - k_Alpha);
				});
				Check(worst.Value < 2.0f * k_AppliedTolerance, std::format("Froxel 9{}: {} ({})", static_cast<char>('a' + i), cases[i].Name, Describe(worst)));
			}
		});

		render(false, nullptr);
		Capture(m_ApplyTarget, set, 0, m_Alive);
		for (size_t i = 0; i < cases.size(); ++i)
		{
			render(false, &cases[i]);
			Capture(m_ApplyTarget, set, 1 + 2 * i, m_Alive);
			const Renderer3D::Statistics stats = render(true, &cases[i]);
			Capture(m_ApplyTarget, set, 2 + 2 * i, m_Alive);
			Check(stats.Volumetrics && stats.TranslucentDraws >= 1, std::format("Froxel 9{}-draw: the fogged scene drew its translucent quad with the volume (volumetric {}, translucent draws {})", static_cast<char>('a' + i), stats.Volumetrics, stats.TranslucentDraws));
		}
	}

	void FroxelFogTest::CheckParticles()
	{
		const glm::vec3 dot(1.0f, 0.6f, 0.3f);
		const glm::mat4 transform = glm::translate(glm::mat4(1.0f), k_Eye + glm::vec3(0.0f, 0.0f, -SliceDepth(k_Boundary)));
		auto render = [&](const std::shared_ptr<ParticleEmitter>& emitter, bool fog)
		{
			return RenderApply(*m_Probe, [&](Renderer3D& r)
			{
				AppliedLight(r);
				if (fog)
					ApplyFog(r);
				if (!fog)
					emitter->Emit(1);
				r.SubmitParticles(*emitter, transform, fog ? 0.0f : 0.016f);
			});
		};

		const std::shared_ptr<ReadSet> set = MakeReadSet(4, [this, dot](const std::vector<TexturePixels>& images)
		{
			const glm::vec3 centre = PixelRgb(images[0], k_CentreX, k_CentreY);
			const glm::vec3 additiveCentre = PixelRgb(images[2], k_CentreX, k_CentreY);
			Check(centre.r > 0.05f && additiveCentre.r > 0.05f, std::format("Froxel 10a: the baseline dots are drawn (alpha centre {}, additive centre {})", Rgb(centre), Rgb(additiveCentre)));

			const Worst alpha = WorstColor(images[1], [&](uint32_t x, uint32_t y)
			{
				const glm::vec4 fog = FogAt(k_Boundary, x, y);
				return (dot * fog.a + glm::vec3(fog)) * PixelRgb(images[0], x, y).r;
			});
			const Worst additive = WorstColor(images[3], [&](uint32_t x, uint32_t y)
			{
				return PixelRgb(images[2], x, y) * FogAt(k_Boundary, x, y).a;
			});
			Check(alpha.Value < 2.0f * k_AppliedTolerance, std::format("Froxel 10b: an alpha particle is its fogged colour at its own coverage ({})", Describe(alpha)));
			Check(additive.Value < 2.0f * k_AppliedTolerance, std::format("Froxel 10c: an additive particle keeps its colour times T ({})", Describe(additive)));
		});

		render(m_AlphaEmitter, false);
		Capture(m_ApplyTarget, set, 0, m_Alive);
		const Renderer3D::Statistics alpha = render(m_AlphaEmitter, true);
		Capture(m_ApplyTarget, set, 1, m_Alive);
		render(m_AdditiveEmitter, false);
		Capture(m_ApplyTarget, set, 2, m_Alive);
		const Renderer3D::Statistics additive = render(m_AdditiveEmitter, true);
		Capture(m_ApplyTarget, set, 3, m_Alive);
		Check(alpha.Volumetrics && additive.Volumetrics && alpha.ParticleEmitters == 1 && additive.ParticleEmitters == 1, std::format("Froxel 10d: the fogged scenes drew one emitter each with the volume (alpha {} with {}, additive {} with {})", alpha.Volumetrics, alpha.ParticleEmitters, additive.Volumetrics, additive.ParticleEmitters));
	}

	void FroxelFogTest::CheckSky()
	{
		const bool ready = m_Environment->Prepare();
		SceneEnvironment sky;
		sky.Environment = m_Environment;
		sky.Lighting = false;
		auto render = [&](bool fog)
		{
			return RenderApply(*m_Probe, [&](Renderer3D& r)
			{
				AppliedLight(r);
				if (fog)
					ApplyFog(r);
				r.SetEnvironment(sky);
			});
		};

		const std::shared_ptr<ReadSet> set = MakeReadSet(2, [this](const std::vector<TexturePixels>& images)
		{
			const Worst worst = WorstColor(images[1], [&](uint32_t x, uint32_t y) { return Fogged(PixelRgb(images[0], x, y), static_cast<float>(k_Grid.z), x, y); });
			Check(worst.Value < k_AppliedTolerance, std::format("Froxel 11a: the sky is S T + L of the last slice ({})", Describe(worst)));
		});

		const Renderer3D::Statistics baseline = render(false);
		Capture(m_ApplyTarget, set, 0, m_Alive);
		const Renderer3D::Statistics fogged = render(true);
		Capture(m_ApplyTarget, set, 1, m_Alive);
		Check(ready && baseline.Skybox && fogged.Skybox && fogged.Volumetrics, std::format("Froxel 11b: both scenes drew the sky and the fogged one the volume (prepared {}, sky {} {}, volumetric {})", ready, baseline.Skybox, fogged.Skybox, fogged.Volumetrics));
	}

	void FroxelFogTest::CheckCustomShader()
	{
		const glm::vec3 color(0.7f, 0.3f, 0.2f);
		auto render = [&](bool fog)
		{
			return RenderApply(*m_Probe, [&](Renderer3D& r)
			{
				AppliedLight(r);
				if (fog)
					ApplyFog(r);
				SubmitQuad(r, SliceDepth(k_Boundary), glm::vec4(1.0f), m_Custom);
			});
		};

		const std::shared_ptr<ReadSet> set = MakeReadSet(2, [this, color](const std::vector<TexturePixels>& images)
		{
			const Worst plain = WorstColor(images[0], [color](uint32_t, uint32_t) { return color; });
			const Worst fogged = WorstColor(images[1], [color](uint32_t x, uint32_t y) { return Fogged(color, k_Boundary, x, y); });
			Check(plain.Value < k_AppliedTolerance, std::format("Froxel 12a: a custom shader that includes Volumetrics.glsl draws its own colour with no volume ({})", Describe(plain)));
			Check(fogged.Value < k_AppliedTolerance, std::format("Froxel 12b: with the volume it draws c T + L ({})", Describe(fogged)));
		});

		render(false);
		Capture(m_ApplyTarget, set, 0, m_Alive);
		const Renderer3D::Statistics stats = render(true);
		Capture(m_ApplyTarget, set, 1, m_Alive);
		Check(stats.Volumetrics, "Froxel 12c: the scene with the custom material ran the volume");
	}

	void FroxelFogTest::CheckScenePath()
	{
		Fog distance;
		distance.Mode = FogMode::Exponential;
		distance.Density = 0.1f;
		distance.Color = glm::vec3(0.5f, 0.6f, 0.7f);
		const glm::vec4 color(0.8f, 0.6f, 0.4f, 1.0f);
		const float depth = SliceDepth(k_Boundary);
		Mesh* quad = m_Quad;

		auto render = [&](bool fogComponent, bool volumetric)
		{
			return RenderApply(*m_Probe, [&](Renderer3D& r)
			{
				Dingo::Scene scene("Froxel Scene");
				const float height = QuadHeight(depth);
				Entity entity = scene.CreateEntity("Quad");
				entity.AddComponent(Transform3DComponent(k_Eye + glm::vec3(0.0f, 0.0f, -depth), glm::vec3(height * k_Aspect, height, 1.0f)));
				entity.AddComponent<MeshRendererComponent>(quad).Color = color;
				scene.CreateEntity("Ambient").AddComponent(AmbientLightComponent(glm::vec3(1.0f), k_AppliedAmbient));
				if (fogComponent)
				{
					FogComponent fog(distance, false);
					if (volumetric)
						fog.Volumetric = AppliedFog();
					scene.CreateEntity("Fog").AddComponent<FogComponent>(fog);
				}
				scene.SubmitLights(r);
				if (!fogComponent)
					ApplyFog(r);
				scene.RenderEntities3D(r);
			});
		};

		const std::shared_ptr<ReadSet> set = MakeReadSet(3, [this](const std::vector<TexturePixels>& images)
		{
			const Worst same = WorstEqual(images[0], images[1]);
			const Worst differs = WorstEqual(images[0], images[2]);
			Check(same.Value < 1e-4f, std::format("Froxel 13a: a FogComponent with Volumetric through SubmitLights equals SetVolumetricFog ({})", Describe(same)));
			Check(differs.Value > 0.01f, std::format("Froxel 13b: it replaces the component's distance fog, which the same component without Volumetric keeps (differs by {:.4f})", differs.Value));
		});

		const Renderer3D::Statistics component = render(true, true);
		Capture(m_ApplyTarget, set, 0, m_Alive);
		render(false, false);
		Capture(m_ApplyTarget, set, 1, m_Alive);
		const Renderer3D::Statistics distanceOnly = render(true, false);
		Capture(m_ApplyTarget, set, 2, m_Alive);
		Check(component.Volumetrics && component.VolumetricDispatches == 2 && distanceOnly.Fogged && !distanceOnly.Volumetrics && distanceOnly.VolumetricDispatches == 0,
			std::format("Froxel 13c: the component with Volumetric runs the volume and the one without keeps the distance fog (volumetric {} with {} dispatches; distance {} volumetric {})", component.Volumetrics, component.VolumetricDispatches, distanceOnly.Fogged, distanceOnly.Volumetrics));
	}

	void FroxelFogTest::CheckOrthographic()
	{
		auto render = [&](bool fog)
		{
			return RenderApply(*m_Probe, [&](Renderer3D& r)
			{
				AppliedLight(r);
				if (fog)
					ApplyFog(r);
				SubmitQuad(r, SliceDepth(k_Boundary), glm::vec4(0.8f, 0.6f, 0.4f, 1.0f));
			}, true);
		};

		const std::shared_ptr<ReadSet> set = MakeReadSet(2, [this](const std::vector<TexturePixels>& images)
		{
			const Worst same = WorstEqual(images[0], images[1]);
			Check(same.Value < 1e-4f, std::format("Froxel 15b: an orthographic scene with a volumetric fog set is the picture without it ({})", Describe(same)));
		});

		render(false);
		Capture(m_ApplyTarget, set, 0, m_Alive);
		const Renderer3D::Statistics stats = render(true);
		Capture(m_ApplyTarget, set, 1, m_Alive);
		Check(!stats.Volumetrics && stats.VolumetricDispatches == 0, std::format("Froxel 15a: an orthographic camera has no volume (volumetric {}, dispatches {})", stats.Volumetrics, stats.VolumetricDispatches));
	}

	void FroxelFogTest::CheckInvalid()
	{
		std::vector<std::pair<std::string, bool>> results;
		RenderApply(*m_Probe, [&](Renderer3D& r)
		{
			auto expect = [&](const char* name, bool accepted, const std::function<void(VolumetricFog&)>& change)
			{
				VolumetricFog fog = AppliedFog();
				change(fog);
				results.emplace_back(name, r.SetVolumetricFog(fog) == accepted);
			};
			constexpr float k_NaN = std::numeric_limits<float>::quiet_NaN();
			constexpr float k_Infinity = std::numeric_limits<float>::infinity();
			expect("the default settings are accepted", true, [](VolumetricFog& fog) { fog = VolumetricFog{}; });
			expect("a negative Density is rejected", false, [](VolumetricFog& fog) { fog.Density = -1.0f; });
			expect("a negative HeightFalloff is rejected", false, [](VolumetricFog& fog) { fog.HeightFalloff = -1.0f; });
			expect("a negative ScatterTint is rejected", false, [](VolumetricFog& fog) { fog.ScatterTint = glm::vec3(1.0f, -0.1f, 1.0f); });
			expect("a NaN ScatterTint is rejected", false, [](VolumetricFog& fog) { fog.ScatterTint = glm::vec3(1.0f, k_NaN, 1.0f); });
			expect("a ScatterTint above 1 is accepted", true, [](VolumetricFog& fog) { fog.ScatterTint = glm::vec3(0.6f, 0.74f, 3.0f); });
			expect("a NaN OutsideShadowLight is rejected", false, [](VolumetricFog& fog) { fog.OutsideShadowLight = k_NaN; });
			expect("an OutsideShadowLight past 1 is clamped, not rejected", true, [](VolumetricFog& fog) { fog.OutsideShadowLight = 2.0f; });
			expect("a NaN Density is rejected", false, [](VolumetricFog& fog) { fog.Density = k_NaN; });
			expect("an infinite Density is rejected", false, [](VolumetricFog& fog) { fog.Density = k_Infinity; });
			expect("a negative LightIntensity is rejected", false, [](VolumetricFog& fog) { fog.LightIntensity = -1.0f; });
			expect("a negative AmbientIntensity is rejected", false, [](VolumetricFog& fog) { fog.AmbientIntensity = -1.0f; });
			expect("a NearDistance of 0 is rejected", false, [](VolumetricFog& fog) { fog.NearDistance = 0.0f; });
			expect("a MaxDistance that is not past NearDistance is rejected", false, [](VolumetricFog& fog) { fog.MaxDistance = fog.NearDistance; });
			expect("an Anisotropy past the limit is clamped, not rejected", true, [](VolumetricFog& fog) { fog.Anisotropy = 2.0f; });
			expect("an Albedo past 1 is clamped, not rejected", true, [](VolumetricFog& fog) { fog.Albedo = glm::vec3(2.0f); });
			expect("a Density of 0 is accepted", true, [](VolumetricFog& fog) { fog.Density = 0.0f; });
			r.ClearVolumetricFog();
		});
		for (const auto& [name, passed] : results)
			Check(passed, "Froxel 16: SetVolumetricFog " + name);
	}

	void FroxelFogTest::CheckGridBefore()
	{
		const std::shared_ptr<ReadSet> set = MakeReadSet(3, [this](const std::vector<TexturePixels>& images)
		{
			m_GridBefore = images;
		});
		RenderGridQuads([&](size_t index) { Capture(m_ApplyTarget, set, index, m_Alive); });

		Texture* integrated = Integrated();
		m_IntegratedBefore = integrated;
		m_IntegratedIdBefore = integrated ? integrated->GetId() : 0;
		m_IntegratedGenerationBefore = integrated ? integrated->GetGeneration() : 0;
		m_Probe->SetVolumetricGrid(glm::uvec3(k_Grid.x, k_Grid.y, k_Grid.z * 2));
	}

	void FroxelFogTest::CheckGridAfter()
	{
		const std::shared_ptr<ReadSet> set = MakeReadSet(3, [this](const std::vector<TexturePixels>& images)
		{
			const std::array<const char*, 3> names = { "lit", "PBR", "custom" };
			for (size_t i = 0; i < names.size(); ++i)
			{
				const bool have = i < m_GridBefore.size();
				const Worst worst = have ? WorstEqual(images[i], m_GridBefore[i]) : Worst{};
				const glm::vec3 centre = have ? PixelRgb(m_GridBefore[i], k_CentreX, k_CentreY) : glm::vec3(0.0f);
				Check(have && centre.r > 0.05f && worst.Value < 1.5f * k_AppliedTolerance, std::format("Froxel 14b: the {} quad draws the same against the new grid ({}; centre before {})", names[i], Describe(worst), Rgb(centre)));
			}
		});
		RenderGridQuads([&](size_t index) { Capture(m_ApplyTarget, set, index, m_Alive); });

		Texture* integrated = Integrated();
		const bool kept = integrated && integrated == m_IntegratedBefore && integrated->GetId() == m_IntegratedIdBefore;
		const bool changed = integrated && integrated->GetGeneration() != m_IntegratedGenerationBefore && integrated->GetParams().Depth == k_Grid.z * 2;
		Check(kept && changed, std::format("Froxel 14a: a new grid keeps the integrated texture's pointer and id and reinitialises it in place (kept {}, new generation and depth {})", kept, changed));
	}

	void FroxelFogTest::DrawView()
	{
		if (!m_Renderer || !m_Box || !m_Sphere)
			return;

		Framebuffer* output = Renderer::GetRenderTarget();
		const float halfWidth = static_cast<float>(output ? output->GetWidth() : m_Width) * 0.5f;
		const float halfHeight = static_cast<float>(output ? output->GetHeight() : m_Height) * 0.5f;

		struct Panel
		{
			Viewport Area;
			glm::vec3 Eye;
			glm::vec3 Target;
			VolumetricFog Fog;
			glm::vec3 AmbientColor;
			float AmbientIntensity;
			std::vector<Sun> Suns;
		};

		auto draw = [&](const Panel& panel, const std::function<void(Renderer3D&)>& submit)
		{
			Renderer::SetViewport(panel.Area);
			PerspectiveCamera camera(k_Fov, panel.Area.Width / panel.Area.Height, 0.1f, 100.0f);
			camera.SetPosition(panel.Eye);
			camera.SetUp(glm::vec3(0.0f, 1.0f, 0.0f));
			camera.SetTarget(panel.Target);
			m_Renderer->BeginScene(camera.GetViewProjectionMatrix());
			m_Renderer->SetAmbientLight(panel.AmbientColor, panel.AmbientIntensity);
			for (const Sun& sun : panel.Suns)
			{
				DirectionalLight light;
				light.Direction = glm::normalize(sun.Direction);
				light.Color = sun.Color;
				light.Intensity = sun.Intensity;
				light.CastShadows = sun.Casts;
				light.ShadowStrength = sun.Strength;
				m_Renderer->SubmitLight(light);
			}
			m_Renderer->SetVolumetricFog(panel.Fog);
			submit(*m_Renderer);
			m_Renderer->EndScene();
		};

		auto box = [this](Renderer3D& renderer, const glm::vec3& centre, const glm::vec3& size, const glm::vec4& color)
		{
			renderer.SubmitMesh(m_Box, glm::scale(glm::translate(glm::mat4(1.0f), centre), size), color);
		};
		auto sphere = [this](Renderer3D& renderer, const glm::vec3& centre, float diameter, const glm::vec4& color)
		{
			renderer.SubmitMesh(m_Sphere, glm::scale(glm::translate(glm::mat4(1.0f), centre), glm::vec3(diameter)), color);
		};

		Panel shafts{ { 0.0f, 0.0f, halfWidth, halfHeight }, { 0.0f, 1.5f, 3.0f }, { 0.0f, 1.5f, -6.0f }, {}, glm::vec3(1.0f), 0.01f, {} };
		shafts.Fog.Density = 0.12f;
		shafts.Fog.Anisotropy = 0.7f;
		Sun shaftSun;
		shaftSun.Direction = glm::vec3(0.25f, -0.35f, 0.9f);
		shaftSun.Casts = true;
		shaftSun.Intensity = 3.0f;
		shafts.Suns.push_back(shaftSun);
		draw(shafts, [&](Renderer3D& renderer)
		{
			box(renderer, glm::vec3(0.0f, -0.1f, -12.0f), glm::vec3(30.0f, 0.2f, 40.0f), glm::vec4(0.6f, 0.6f, 0.55f, 1.0f));
			for (float x = -6.0f; x <= 6.0f; x += 0.55f)
				box(renderer, glm::vec3(x, 4.0f, -4.0f), glm::vec3(0.3f, 8.0f, 0.2f), glm::vec4(0.5f, 0.35f, 0.25f, 1.0f));
			box(renderer, glm::vec3(0.0f, 8.1f, 2.0f), glm::vec3(14.0f, 0.2f, 12.0f), glm::vec4(0.5f, 0.35f, 0.25f, 1.0f));
		});

		Panel height{ { halfWidth, 0.0f, halfWidth, halfHeight }, { 0.0f, 1.2f, 4.0f }, { 0.0f, 0.8f, -6.0f }, {}, glm::vec3(1.0f, 1.0f, 1.0f), 0.6f, {} };
		height.Fog.Density = 0.35f;
		height.Fog.HeightFalloff = 0.6f;
		height.Fog.Albedo = glm::vec3(0.8f, 0.85f, 1.0f);
		height.Fog.Temporal = false;
		draw(height, [&](Renderer3D& renderer)
		{
			box(renderer, glm::vec3(0.0f, -0.1f, -8.0f), glm::vec3(30.0f, 0.2f, 30.0f), glm::vec4(0.45f, 0.5f, 0.45f, 1.0f));
			for (int i = -3; i <= 3; ++i)
				box(renderer, glm::vec3(static_cast<float>(i) * 1.5f, 0.5f + static_cast<float>((i + 3) % 3), -6.0f - static_cast<float>(i % 2) * 2.0f), glm::vec3(0.8f, 1.0f + static_cast<float>((i + 3) % 3) * 2.0f, 0.8f), glm::vec4(0.7f, 0.5f, 0.4f, 1.0f));
			sphere(renderer, glm::vec3(-1.0f, 0.5f, -3.0f), 1.0f, glm::vec4(0.9f, 0.9f, 0.9f, 1.0f));
			renderer.SubmitMesh(m_Sphere, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.5f, -3.0f)), glm::vec3(1.0f)), glm::vec4(1.0f), m_ViewPbr);
		});

		for (int i = 0; i < 3; ++i)
		{
			constexpr std::array<float, 3> k_Anisotropy = { -0.5f, 0.0f, 0.8f };
			const float width = halfWidth / 3.0f;
			Panel phase{ { static_cast<float>(i) * width, halfHeight, width, halfHeight }, { 0.0f, 0.0f, 3.0f }, { 0.0f, 0.0f, -6.0f }, {}, glm::vec3(1.0f), 0.0f, {} };
			phase.Fog.Density = 0.15f;
			phase.Fog.Anisotropy = k_Anisotropy[static_cast<size_t>(i)];
			phase.Fog.Temporal = false;
			Sun sun;
			sun.Direction = glm::vec3(0.0f, 0.15f, 1.0f);
			sun.Intensity = 2.0f;
			phase.Suns.push_back(sun);
			draw(phase, [&](Renderer3D& renderer) { box(renderer, glm::vec3(0.0f, 0.0f, -6.0f), glm::vec3(1.5f), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)); });
		}

		Panel dense{ { halfWidth, halfHeight, halfWidth, halfHeight }, { 0.0f, 1.0f, 4.0f }, { 0.0f, 0.8f, -12.0f }, {}, glm::vec3(1.0f), 0.7f, {} };
		dense.Fog.Density = 0.12f;
		dense.Fog.Temporal = false;
		if (!m_ViewSkyReady && m_Environment)
			m_ViewSkyReady = m_Environment->Prepare();
		draw(dense, [&](Renderer3D& renderer)
		{
			constexpr std::array<float, 4> k_Depths = { -2.0f, -6.0f, -10.0f, -16.0f };
			for (size_t i = 0; i < k_Depths.size(); ++i)
				sphere(renderer, glm::vec3(-3.0f + static_cast<float>(i) * 2.0f, 0.8f, k_Depths[i]), 1.5f, glm::vec4(0.9f, 0.4f, 0.3f, 1.0f));
			renderer.SubmitMesh(m_Sphere, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.2f, -4.0f)), glm::vec3(1.6f)), glm::vec4(0.4f, 0.9f, 0.5f, 0.5f), m_ViewGlass);
			if (m_ViewAlphaEmitter)
				renderer.SubmitParticles(*m_ViewAlphaEmitter, glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 2.2f, -8.0f)), m_ViewDelta);
			if (m_ViewAdditiveEmitter)
				renderer.SubmitParticles(*m_ViewAdditiveEmitter, glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 2.4f, -13.0f)), m_ViewDelta);
			if (m_ViewSkyReady)
			{
				SceneEnvironment sky;
				sky.Environment = m_Environment;
				sky.Lighting = false;
				renderer.SetEnvironment(sky);
			}
		});

		Renderer::ResetViewport();
	}

	void FroxelFogTest::Cleanup()
	{
		m_Alive.reset();
		m_ApplySteps.clear();
		m_GridBefore.clear();
		m_IntegratedBefore = nullptr;
		m_AlphaEmitter.reset();
		m_AdditiveEmitter.reset();
		m_ViewAlphaEmitter.reset();
		m_ViewAdditiveEmitter.reset();
		DestroyAndDelete(m_ViewPbr);
		DestroyAndDelete(m_ViewGlass);
		delete m_ViewAlphaEffect;
		m_ViewAlphaEffect = nullptr;
		delete m_ViewAdditiveEffect;
		m_ViewAdditiveEffect = nullptr;
		DestroyAndDelete(m_Target);
		DestroyAndDelete(m_ApplyTarget);
		for (Material** material : { &m_LitAdditive, &m_LitMultiply, &m_LitAlpha, &m_Pbr, &m_PbrAlpha, &m_Custom })
			DestroyAndDelete(*material);
		DestroyAndDelete(m_CustomShader);
		DestroyAndDelete(m_Quad);
		delete m_AlphaEffect;
		m_AlphaEffect = nullptr;
		delete m_AdditiveEffect;
		m_AdditiveEffect = nullptr;
		delete m_Environment;
		m_Environment = nullptr;
		for (Renderer3D** renderer : { &m_Probe, &m_Renderer, &m_Off, &m_Idle })
		{
			if (*renderer)
			{
				(*renderer)->Shutdown();
				delete *renderer;
				*renderer = nullptr;
			}
		}
		DestroyAndDelete(m_Box);
		DestroyAndDelete(m_Sphere);
		m_Reference.clear();
		m_Still.clear();
		std::error_code error;
		std::filesystem::remove_all(m_Folder, error);
		m_Checks.clear();
	}

	void FroxelFogTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
		m_Width = width;
		m_Height = height;
	}

	void FroxelFogTest::ImGuiRender()
	{
		ImGui::TextWrapped("Top left: sun shafts through a slatted wall. Top right: height fog over boxes and a lit and a PBR sphere. Bottom left: one sun seen with Anisotropy -0.5, 0 and 0.8. Bottom right: spheres at four distances, a translucent sphere, alpha and additive particles and the sky in a dense fog. The lit, PBR, translucent, particle and sky shaders apply the volume.");
		if (m_Renderer)
		{
			const Renderer3D::Statistics& stats = m_Renderer->GetStatistics();
			ImGui::Text("Volumetrics %s, temporal %s, dispatches %u, grid %ux%ux%u", stats.Volumetrics ? "on" : "off", stats.VolumetricTemporal ? "on" : "off", stats.VolumetricDispatches,
				m_Renderer->GetVolumetricGrid().x, m_Renderer->GetVolumetricGrid().y, m_Renderer->GetVolumetricGrid().z);
		}
		GraphicsTest::ImGuiRender();

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

}
