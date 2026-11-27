#include "IblTest.h"

#include "DingoEngine/Graphics/EnvironmentAccess.h"
#include "DingoEngine/Graphics/EnvironmentShaders.h"

#include <glm/glm.hpp>
#include <glm/gtc/bitfield.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <fstream>
#include <functional>

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
namespace GlslIbl
{
	using glm::vec2;
	using glm::vec3;
	using glm::vec4;
	using glm::asin;
	using glm::atan;
	using glm::bitfieldReverse;
	using glm::clamp;
	using glm::cos;
	using glm::dot;
	using glm::exp2;
	using glm::log2;
	using glm::max;
	using glm::min;
	using glm::mix;
	using glm::normalize;
	using glm::pow;
	using glm::sin;
	using glm::sqrt;
	using glm::step;
	using uint = std::uint32_t;

#include "DingoEngine/Graphics/Shaders/CubeMap.glsl"
#include "DingoEngine/Graphics/Shaders/Environment.glsl"
#include "DingoEngine/Graphics/Shaders/Brdf.glsl"
#include "DingoEngine/Graphics/Shaders/Ibl.glsl"
}
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace Dingo
{

	namespace
	{
		constexpr uint32_t k_FramesBeforeChecks = 10;
		constexpr uint32_t k_PanoramaWidth = 512;
		constexpr uint32_t k_PanoramaHeight = 256;
		constexpr uint32_t k_SpecularFace = 128;
		constexpr uint32_t k_SpecularMips = 6;
		constexpr float k_A = 1.0f;
		constexpr float k_B = 0.5f;
		constexpr float k_C = 0.2f;
		const glm::vec3 k_Tint(1.0f, 0.8f, 0.6f);

		float AnalyticScalar(const glm::vec3& dir)
		{
			return k_A + k_B * dir.y + k_C * (3.0f * dir.y * dir.y - 1.0f);
		}

		glm::vec3 AnalyticRadiance(const glm::vec3& dir)
		{
			return AnalyticScalar(dir) * k_Tint;
		}

		glm::vec3 AnalyticIrradianceOverPi(const glm::vec3& n)
		{
			return (k_A + (2.0f / 3.0f) * k_B * n.y + 0.25f * k_C * (3.0f * n.y * n.y - 1.0f)) * k_Tint;
		}

		std::array<uint8_t, 4> EncodeRgbe(const glm::vec3& color)
		{
			const float largest = (std::max)({ color.r, color.g, color.b });
			if (largest < 1e-32f)
				return { 0, 0, 0, 0 };
			int exponent = 0;
			(void)std::frexp(largest, &exponent);
			const float scale = std::ldexp(256.0f, -exponent);
			auto mantissa = [scale](float channel) { return static_cast<uint8_t>((std::min)(std::round(channel * scale), 255.0f)); };
			return { mantissa(color.r), mantissa(color.g), mantissa(color.b), static_cast<uint8_t>(exponent + 128) };
		}

		void WritePanorama(const std::filesystem::path& file, const std::function<glm::vec3(const glm::vec3&)>& radiance)
		{
			std::vector<std::array<uint8_t, 4>> fileRows(static_cast<size_t>(k_PanoramaWidth) * k_PanoramaHeight);
			for (uint32_t row = 0; row < k_PanoramaHeight; ++row)
			{
				for (uint32_t x = 0; x < k_PanoramaWidth; ++x)
				{
					const glm::vec2 uv((static_cast<float>(x) + 0.5f) / static_cast<float>(k_PanoramaWidth), (static_cast<float>(row) + 0.5f) / static_cast<float>(k_PanoramaHeight));
					fileRows[static_cast<size_t>(k_PanoramaHeight - 1 - row) * k_PanoramaWidth + x] = EncodeRgbe(radiance(GlslIbl::DE_EquirectDirection(uv)));
				}
			}
			std::ofstream out(file, std::ios::binary);
			const std::string header = std::format("#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y {} +X {}\n", k_PanoramaHeight, k_PanoramaWidth);
			out.write(header.data(), static_cast<std::streamsize>(header.size()));
			out.write(reinterpret_cast<const char*>(fileRows.data()), static_cast<std::streamsize>(fileRows.size() * 4));
		}

		float RelativeError(const glm::vec3& got, const glm::vec3& expected)
		{
			float worst = 0.0f;
			for (int i = 0; i < 3; ++i)
				worst = (std::max)(worst, std::abs(got[i] - expected[i]) / (std::max)(std::abs(expected[i]), 1e-3f));
			return worst;
		}

		float MonteCarloScalar(const glm::vec3& n, float roughness, float (*scalar)(const glm::vec3&))
		{
			constexpr uint32_t k_Samples = 20000;
			const float a = roughness * roughness;
			const glm::vec3 up = std::abs(n.z) < 0.999f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
			const glm::vec3 tangent = glm::normalize(glm::cross(up, n));
			const glm::vec3 bitangent = glm::cross(n, tangent);
			double sum = 0.0;
			double weight = 0.0;
			for (uint32_t i = 0; i < k_Samples; ++i)
			{
				const glm::vec3 h = GlslIbl::DE_ImportanceSampleGgx(GlslIbl::DE_Hammersley(i, k_Samples), a);
				const glm::vec3 half = tangent * h.x + bitangent * h.y + n * h.z;
				const glm::vec3 l = half * (2.0f * glm::dot(n, half)) - n;
				const float nol = (std::max)(glm::dot(n, l), 0.0f);
				sum += static_cast<double>(scalar(l) * nol);
				weight += static_cast<double>(nol);
			}
			return static_cast<float>(sum / weight);
		}

		struct PrefilterBands
		{
			float Lambda1;
			float Lambda2;
		};

		PrefilterBands MonteCarloBands(float roughness)
		{
			const glm::vec3 up(0.0f, 1.0f, 0.0f);
			return {
				MonteCarloScalar(up, roughness, [](const glm::vec3& d) { return d.y; }),
				MonteCarloScalar(up, roughness, [](const glm::vec3& d) { return 0.5f * (3.0f * d.y * d.y - 1.0f); }) };
		}

		glm::vec3 FunkHeckePrefilter(const glm::vec3& n, const PrefilterBands& bands)
		{
			return (k_A + bands.Lambda1 * k_B * n.y + bands.Lambda2 * k_C * (3.0f * n.y * n.y - 1.0f)) * k_Tint;
		}

		struct SpecularReadback
		{
			uint32_t Remaining = k_SpecularMips * 6;
			uint32_t Empty = 0;
			std::array<std::vector<glm::vec3>, k_SpecularMips * 6> Pixels;
		};

		void ReadSpecular(Environment* environment, const std::weak_ptr<int>& alive, std::function<void(const SpecularReadback&)> done)
		{
			Texture* specular = Internal::EnvironmentAccess::GetSpecular(environment);
			auto readback = std::make_shared<SpecularReadback>();
			for (uint32_t mip = 0; mip < k_SpecularMips; ++mip)
			{
				for (uint32_t face = 0; face < 6; ++face)
				{
					specular->ReadSubresource(mip, face, [alive, readback, mip, face, done](const TexturePixels& pixels)
					{
						if (alive.expired())
							return;
						const uint32_t size = k_SpecularFace >> mip;
						std::vector<glm::vec3>& target = readback->Pixels[mip * 6 + face];
						if (pixels.Data.empty() || pixels.Width != size || pixels.Height != size)
							++readback->Empty;
						else
						{
							target.reserve(static_cast<size_t>(size) * size);
							for (uint32_t y = 0; y < size; ++y)
							{
								for (uint32_t x = 0; x < size; ++x)
									target.push_back(glm::vec3(pixels.GetPixel(x, y)));
							}
						}
						if (--readback->Remaining == 0)
							done(*readback);
					});
				}
			}
		}

		glm::vec3 TexelDirection(uint32_t face, uint32_t x, uint32_t y, uint32_t size)
		{
			const glm::vec2 uv((static_cast<float>(x) + 0.5f) / static_cast<float>(size), (static_cast<float>(y) + 0.5f) / static_cast<float>(size));
			return GlslIbl::DE_CubeFaceDirection(face, uv);
		}

		std::array<glm::vec3, 9> ShCoefficients(const std::vector<uint8_t>& bytes)
		{
			std::array<glm::vec3, 9> coefficients = {};
			if (bytes.size() < 9 * 4 * sizeof(float))
				return coefficients;
			const float* values = reinterpret_cast<const float*>(bytes.data());
			for (size_t i = 0; i < 9; ++i)
				coefficients[i] = glm::vec3(values[i * 4], values[i * 4 + 1], values[i * 4 + 2]);
			return coefficients;
		}

		glm::vec3 ShIrradianceOverPi(const std::array<glm::vec3, 9>& coefficients, const glm::vec3& n)
		{
			glm::vec3 sum(0.0f);
			for (int i = 0; i < 9; ++i)
				sum += coefficients[static_cast<size_t>(i)] * GlslIbl::DE_ShBasis(i, n);
			return sum;
		}

		constexpr uint32_t k_ShotSize = 33;
		constexpr uint32_t k_PathShotSize = 65;
		constexpr uint32_t k_Centre = k_ShotSize / 2;
		constexpr float k_Tolerance = 0.03f;
		const glm::vec4 k_Clear(1.0f, 0.0f, 1.0f, 1.0f);

		glm::vec3 SrgbEncode(const glm::vec3& linear)
		{
			glm::vec3 out;
			for (int i = 0; i < 3; ++i)
			{
				const float c = (std::max)(linear[i], 0.0f);
				out[i] = c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
			}
			return glm::min(out, glm::vec3(1.0f));
		}

		glm::vec3 SrgbDecode(const glm::vec3& encoded)
		{
			glm::vec3 out;
			for (int i = 0; i < 3; ++i)
			{
				const float c = (std::max)(encoded[i], 0.0f);
				out[i] = c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
			}
			return out;
		}

		float MaxDifference(const glm::vec4& pixel, const glm::vec3& expected)
		{
			return (std::max)({ std::abs(pixel.r - expected.r), std::abs(pixel.g - expected.g), std::abs(pixel.b - expected.b) });
		}

		bool IsClear(const glm::vec4& pixel)
		{
			return std::abs(pixel.r - 1.0f) < 0.01f && pixel.g < 0.01f && std::abs(pixel.b - 1.0f) < 0.01f;
		}

		bool SamePixel(const glm::vec4& a, const glm::vec4& b, float tolerance = 0.005f)
		{
			return std::abs(a.r - b.r) < tolerance && std::abs(a.g - b.g) < tolerance && std::abs(a.b - b.b) < tolerance;
		}

		glm::vec4 CentrePixel(const TexturePixels& image)
		{
			return image.Data.empty() ? glm::vec4(-10.0f) : image.GetPixel(k_Centre, k_Centre);
		}

		glm::vec3 AzimuthRadiance(const glm::vec3& dir)
		{
			return (1.0f + 0.6f * dir.x) * k_Tint;
		}

		struct Surface
		{
			glm::vec3 Base;
			float Metallic;
			float Roughness;
		};

		// What Renderer3D_Pbr.glsl's PbrAmbient returns for a sphere centre (the normal faces the camera, so
		// the horizon term is 1) in linear light, with the LUT's value computed directly.
		glm::vec3 SurfaceLighting(const Surface& surface, float occlusion, const glm::vec3& irradianceOverPi, const glm::vec3& prefiltered)
		{
			const glm::vec3 diffuse = surface.Base * (1.0f - surface.Metallic);
			const glm::vec3 f0 = glm::mix(glm::vec3(0.04f), surface.Base, surface.Metallic);
			const glm::vec2 dfg = GlslIbl::DE_IntegrateBrdf(1.0f, surface.Roughness, 256.0f);
			const float specularOcclusion = GlslIbl::DE_SpecularOcclusion(1.0f, occlusion, surface.Roughness * surface.Roughness);
			return diffuse * irradianceOverPi * occlusion + prefiltered * (f0 * dfg.x + glm::vec3(dfg.y)) * specularOcclusion;
		}

		glm::vec3 FurnaceLighting(const Surface& surface, float occlusion = 1.0f)
		{
			return SurfaceLighting(surface, occlusion, glm::vec3(1.0f), glm::vec3(1.0f));
		}

		glm::vec3 FlatLighting(const Surface& surface, float ambient)
		{
			const glm::vec3 diffuse = surface.Base * (1.0f - surface.Metallic);
			const glm::vec3 f0 = glm::mix(glm::vec3(0.04f), surface.Base, surface.Metallic);
			return glm::vec3(ambient) * (diffuse + GlslIbl::EnvBRDFApprox(f0, surface.Roughness, 1.0f));
		}

		uint32_t CountDifferent(const TexturePixels& reference, const TexturePixels& other, float tolerance, uint32_t* covered = nullptr)
		{
			uint32_t differing = 0;
			uint32_t mesh = 0;
			if (reference.Data.empty() || other.Data.empty() || reference.Width != other.Width || reference.Height != other.Height)
				return 0xFFFFFFFFu;
			for (uint32_t y = 0; y < reference.Height; ++y)
			{
				for (uint32_t x = 0; x < reference.Width; ++x)
				{
					const glm::vec4 expected = reference.GetPixel(x, y);
					if (IsClear(expected))
						continue;
					++mesh;
					differing += SamePixel(other.GetPixel(x, y), expected, tolerance) ? 0 : 1;
				}
			}
			if (covered)
				*covered = mesh;
			return differing;
		}

		std::array<uint8_t, 4> BumpTexel(const glm::vec2& tilt)
		{
			const float z = std::sqrt(1.0f - tilt.x * tilt.x - tilt.y * tilt.y);
			auto encode = [](float value) { return static_cast<uint8_t>(std::lround((value * 0.5f + 0.5f) * 255.0f)); };
			return { encode(tilt.x), encode(tilt.y), encode(z), 255 };
		}

		float UniformScalar(const glm::vec3&)
		{
			return 1.0f;
		}

		float AnalyticIrradianceScalar(const glm::vec3& n)
		{
			return k_A + (2.0f / 3.0f) * k_B * n.y + 0.25f * k_C * (3.0f * n.y * n.y - 1.0f);
		}

		float AzimuthScalar(const glm::vec3& dir)
		{
			return 1.0f + 0.6f * dir.x;
		}

		float AzimuthIrradianceScalar(const glm::vec3& n)
		{
			return 1.0f + 0.6f * (2.0f / 3.0f) * n.x;
		}

		struct EnvModel
		{
			float (*Scalar)(const glm::vec3&);
			float (*IrradianceScalar)(const glm::vec3&);
			glm::vec3 Tint;
		};

		const EnvModel k_UniformModel{ UniformScalar, UniformScalar, glm::vec3(1.0f) };
		const EnvModel k_AnalyticModel{ AnalyticScalar, AnalyticIrradianceScalar, k_Tint };
		const EnvModel k_AzimuthModel{ AzimuthScalar, AzimuthIrradianceScalar, k_Tint };

		// The sphere centre's lighting in linear light: the normal and the reflection both point at the
		// camera there, so one lookup direction serves the irradiance and the prefiltered cube.
		glm::vec3 CentreLighting(const EnvModel& model, const Surface& surface, float occlusion, const glm::vec3& normal, float rotationDegrees = 0.0f)
		{
			const float radians = glm::radians(-rotationDegrees);
			const glm::vec3 lookup = GlslIbl::DE_RotateY(normal, std::cos(radians), std::sin(radians));
			const glm::vec3 irradiance = model.IrradianceScalar(lookup) * model.Tint;
			const glm::vec3 prefiltered = MonteCarloScalar(lookup, surface.Roughness, model.Scalar) * model.Tint;
			return SurfaceLighting(surface, occlusion, irradiance, prefiltered);
		}

		std::string Rgb(const glm::vec4& pixel)
		{
			return std::format("{:.3f},{:.3f},{:.3f}", pixel.r, pixel.g, pixel.b);
		}

		std::string Rgb(const glm::vec3& color)
		{
			return std::format("{:.3f},{:.3f},{:.3f}", color.r, color.g, color.b);
		}
	}

	void IblTest::Initialize()
	{
		m_Checks.clear();
		m_ChecksDone = false;
		m_Alive = std::make_shared<int>(0);

		m_Folder = TestTempDirectory("DingoIblTest");
		std::error_code error;
		std::filesystem::create_directories(m_Folder, error);

		m_UniformFile = m_Folder / "uniform.hdr";
		m_AnalyticFile = m_Folder / "analytic.hdr";
		const std::filesystem::path azimuthFile = m_Folder / "azimuth.hdr";
		WritePanorama(m_UniformFile, [](const glm::vec3&) { return glm::vec3(1.0f); });
		WritePanorama(m_AnalyticFile, AnalyticRadiance);
		WritePanorama(azimuthFile, AzimuthRadiance);

		m_Uniform = Environment::Create(EnvironmentParams().SetPath(m_UniformFile).SetDebugName("IblTest uniform"));
		m_Analytic = Environment::Create(EnvironmentParams().SetPath(m_AnalyticFile).SetDebugName("IblTest analytic"));
		m_Azimuth = Environment::Create(EnvironmentParams().SetPath(azimuthFile).SetDebugName("IblTest azimuth"));
		m_Renderer = Renderer3D::Create();
		if (!m_Renderer)
			return;

		m_Sphere = Mesh::CreateSphere(1.0f, 48, 64);
		if (m_Sphere)
		{
			std::vector<SkinnedMeshVertex> skin;
			for (const MeshVertex& vertex : m_Sphere->GetVertices())
				skin.push_back({ vertex.Position, vertex.Normal, vertex.TexCoord, glm::u16vec4(0), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f) });
			m_SkinnedSphere = Mesh::CreateSkinned(m_Sphere->GetVertices(), skin, m_Sphere->GetIndices());
		}
		m_Palette.assign(1, glm::mat4(1.0f));

		constexpr float k_GridRoughness[5] = { 0.05f, 0.25f, 0.5f, 0.75f, 1.0f };
		for (float metallic : { 1.0f, 0.0f })
		{
			for (float roughness : k_GridRoughness)
				m_GridMaterials.push_back(CreatePbr({ 0.9f, 0.6f, 0.3f }, metallic, roughness));
		}
		m_DiffuseMaterial = CreatePbr({ 0.5f, 0.5f, 0.5f }, 0.0f, 1.0f);
		m_ChromeMaterial = CreatePbr({ 1.0f, 1.0f, 1.0f }, 1.0f, 0.2f);

		constexpr uint32_t k_MapSize = 64;
		std::vector<uint8_t> bumps;
		std::vector<uint8_t> blotches;
		for (uint32_t y = 0; y < k_MapSize; ++y)
		{
			for (uint32_t x = 0; x < k_MapSize; ++x)
			{
				const float a = 2.0f * glm::pi<float>() * 8.0f * (static_cast<float>(x) + 0.5f) / k_MapSize;
				const float b = 2.0f * glm::pi<float>() * 8.0f * (static_cast<float>(y) + 0.5f) / k_MapSize;
				const std::array<uint8_t, 4> texel = BumpTexel(0.45f * glm::vec2(-std::cos(a) * std::sin(b), -std::sin(a) * std::cos(b)));
				bumps.insert(bumps.end(), texel.begin(), texel.end());
				const uint8_t occlusion = ((x / 8 + y / 8) % 2 == 0) ? 255 : 70;
				blotches.insert(blotches.end(), { occlusion, occlusion, occlusion, 255 });
			}
		}
		Texture* bumpMap = Texture::CreateFromData(k_MapSize, k_MapSize, bumps.data(), TextureFormat::RGBA8_UNORM, "IblTest_BumpMap");
		Texture* blotchMap = Texture::CreateFromData(k_MapSize, k_MapSize, blotches.data(), TextureFormat::RGBA8_UNORM, "IblTest_OcclusionMap");
		m_Textures.push_back(bumpMap);
		m_Textures.push_back(blotchMap);
		m_BumpMaterial = CreatePbr(PbrMaterialParams()
			.SetMaterial(MaterialParams().SetDebugName("IblTest_Bump"))
			.SetBaseColor({ 0.8f, 0.75f, 0.7f, 1.0f })
			.SetRoughness(0.5f)
			.SetNormalMap(bumpMap)
			.SetOcclusionMap(blotchMap));
	}

	Framebuffer* IblTest::NewTarget(const char* name, uint32_t size)
	{
		Framebuffer* target = Framebuffer::Create(FramebufferParams()
			.SetDebugName(name)
			.SetWidth(static_cast<int32_t>(size))
			.SetHeight(static_cast<int32_t>(size))
			.SetEnableDepth(true)
			.AddAttachment({ TextureFormat::RGBA8_UNORM }));
		m_Targets.push_back(target);
		return target;
	}

	glm::mat4 IblTest::ShotMatrix(const glm::vec3& direction) const
	{
		PerspectiveCamera camera(60.0f, 1.0f, 0.1f, 100.0f);
		camera.SetPosition(glm::vec3(0.0f));
		camera.SetUp(std::abs(direction.y) > 0.9f ? glm::vec3(0.0f, 0.0f, -1.0f) : glm::vec3(0.0f, 1.0f, 0.0f));
		camera.SetTarget(direction);
		return camera.GetViewProjectionMatrix();
	}

	Renderer3D::Statistics IblTest::RenderShot(Framebuffer* target, const glm::mat4& viewProjection, const SceneEnvironment* sky, const std::function<void(Renderer3D&)>& submit)
	{
		Framebuffer* previous = Renderer::GetRenderTarget();
		Renderer::SetRenderTarget(target);
		m_Renderer->BeginScene(viewProjection);
		m_Renderer->Clear(k_Clear);
		if (submit)
			submit(*m_Renderer);
		if (sky)
			m_Renderer->SetEnvironment(*sky);
		m_Renderer->EndScene();
		Renderer::SetRenderTarget(previous);
		return m_Renderer->GetStatistics();
	}

	void IblTest::ReadTargets(std::vector<Framebuffer*> targets, std::function<void(const std::vector<TexturePixels>&)> done)
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

	Material* IblTest::CreatePbr(const PbrMaterialParams& params)
	{
		Material* material = m_Renderer ? m_Renderer->CreatePbrMaterial(params) : nullptr;
		if (material)
			m_Materials.push_back(material);
		return material;
	}

	Material* IblTest::CreatePbr(const glm::vec3& base, float metallic, float roughness)
	{
		return CreatePbr(PbrMaterialParams()
			.SetMaterial(MaterialParams().SetDebugName("IblTest_Pbr"))
			.SetBaseColor(glm::vec4(base, 1.0f))
			.SetMetallic(metallic)
			.SetRoughness(roughness));
	}

	Renderer3D::Statistics IblTest::ShotOfSphere(Framebuffer* target, const glm::vec3& direction, const SceneEnvironment* sky, Material* material, float ambient)
	{
		return RenderShot(target, ShotMatrix(direction), sky, [this, direction, material, ambient](Renderer3D& renderer)
		{
			renderer.SetAmbientLight(glm::vec3(1.0f), ambient);
			renderer.SubmitMesh(m_Sphere, glm::translate(glm::mat4(1.0f), direction * 3.0f), glm::vec4(1.0f), material);
		});
	}

	void IblTest::Update(float deltaTime)
	{
		if (!m_ChecksDone && !Renderer::IsFrameSkipped() && ++m_FramesBeforeChecks > k_FramesBeforeChecks)
		{
			m_ChecksDone = true;
			RunChecks();
		}

		Renderer2D& renderer2D = Application::Get().GetRenderer2D();
		renderer2D.BeginScene(glm::ortho(-m_AspectRatio, m_AspectRatio, -1.0f, 1.0f, -1.0f, 1.0f));
		renderer2D.Clear(m_ClearColor);
		renderer2D.EndScene();
		DrawView();
	}

	void IblTest::RunChecks()
	{
		const bool ready = m_Uniform && m_Analytic && m_Azimuth && m_Uniform->GetFaceSize() == k_SpecularFace && m_Analytic->GetFaceSize() == k_SpecularFace && m_Azimuth->GetFaceSize() == k_SpecularFace
			&& m_Uniform->Prepare() && m_Analytic->Prepare() && m_Azimuth->Prepare() && m_Uniform->IsReady() && m_Analytic->IsReady() && m_Azimuth->IsReady()
			&& Internal::GetBrdfLut() && Internal::EnvironmentAccess::GetSpecular(m_Analytic) && Internal::EnvironmentAccess::GetIrradiance(m_Analytic);
		if (!ready)
		{
			Check(false, "IBL: the three 512 x 256 panoramas load as 128 texel Environments and bake inside a frame");
			return;
		}
		const bool scene = m_Renderer && m_Sphere && m_SkinnedSphere && m_DiffuseMaterial && m_ChromeMaterial && m_BumpMaterial && m_GridMaterials.size() == 10;
		if (!scene)
		{
			Check(false, "IBL: the renderer, the spheres and the PBR materials are created");
			return;
		}

		CheckLut();
		CheckShUniform();
		CheckShAnalytic();
		CheckPrefilterUniform();
		CheckPrefilterAnalytic();
		CheckFurnace();
		CheckAnalyticLighting();
		CheckIntensity();
		CheckRotation();
		CheckLitUnchanged();
		CheckTwoEnvironments();
		CheckHotReload();
		CheckDeleted();
		CheckOcclusion();
		CheckPaths();
		CheckOrthographic();
		CheckSceneDriven();
	}

	void IblTest::CheckLut()
	{
		const std::weak_ptr<int> alive = m_Alive;
		Internal::GetBrdfLut()->ReadPixels([this, alive](const TexturePixels& pixels)
		{
			if (alive.expired())
				return;
			const uint32_t last = GlslIbl::k_BrdfLutSize - 1;
			const uint32_t samples[3] = { 0, GlslIbl::k_BrdfLutSize / 2, last };
			float worst = 0.0f;
			std::string worstAt;
			bool ok = pixels.Width == GlslIbl::k_BrdfLutSize && pixels.Height == GlslIbl::k_BrdfLutSize && !pixels.Data.empty();
			for (uint32_t y : samples)
			{
				for (uint32_t x : samples)
				{
					if (!ok)
						break;
					const glm::vec2 uv((static_cast<float>(x) + 0.5f) / static_cast<float>(GlslIbl::k_BrdfLutSize), (static_cast<float>(y) + 0.5f) / static_cast<float>(GlslIbl::k_BrdfLutSize));
					const glm::vec2 value = GlslIbl::DE_BrdfLutValue(uv);
					const glm::vec2 expected = GlslIbl::DE_IntegrateBrdf(value.x, value.y, 256.0f);
					const glm::vec4 got = pixels.GetPixel(x, y);
					const float error = (std::max)(std::abs(got.x - expected.x), std::abs(got.y - expected.y));
					if (error > worst)
					{
						worst = error;
						worstAt = std::format("texel {},{} got {:.4f},{:.4f} expected {:.4f},{:.4f}", x, y, got.x, got.y, expected.x, expected.y);
					}
				}
			}
			Check(ok && worst <= 4e-3f, std::format("IBL 1: the BRDF LUT at 9 texels matches DE_IntegrateBrdf on the CPU (worst {:.5f}, {})", worst, worstAt));
		});
	}

	void IblTest::CheckShUniform()
	{
		const std::weak_ptr<int> alive = m_Alive;
		Internal::EnvironmentAccess::GetIrradiance(m_Uniform)->ReadBack([this, alive](const std::vector<uint8_t>& bytes)
		{
			if (alive.expired())
				return;
			const std::array<glm::vec3, 9> c = ShCoefficients(bytes);
			const float y00 = GlslIbl::DE_ShBasis(0, glm::vec3(0.0f, 1.0f, 0.0f));
			float dcError = 0.0f;
			float others = 0.0f;
			for (int ch = 0; ch < 3; ++ch)
				dcError = (std::max)(dcError, std::abs(c[0][ch] * y00 - 1.0f));
			for (size_t i = 1; i < 9; ++i)
			{
				for (int ch = 0; ch < 3; ++ch)
					others = (std::max)(others, std::abs(c[i][ch]));
			}
			Check(!bytes.empty() && dcError < 1e-3f && others < 1e-3f, std::format("IBL 2: the uniform environment's SH is c0 Y00 = 1 with every other coefficient 0 (DC error {:.5f}, largest other {:.5f}, {} bytes)", dcError, others, bytes.size()));
		});
	}

	void IblTest::CheckShAnalytic()
	{
		const std::weak_ptr<int> alive = m_Alive;
		Internal::EnvironmentAccess::GetIrradiance(m_Analytic)->ReadBack([this, alive](const std::vector<uint8_t>& bytes)
		{
			if (alive.expired())
				return;
			const std::array<glm::vec3, 9> c = ShCoefficients(bytes);
			const glm::vec3 directions[7] = {
				{ 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 },
				glm::normalize(glm::vec3(1.0f, 1.0f, -1.0f)) };
			float worst = 0.0f;
			for (const glm::vec3& n : directions)
				worst = (std::max)(worst, RelativeError(ShIrradianceOverPi(c, n), AnalyticIrradianceOverPi(n)));
			Check(!bytes.empty() && worst < 0.015f, std::format("IBL 3: the analytic environment's SH matches the closed-form irradiance / pi in 7 directions (worst relative error {:.3f} %)", worst * 100.0f));
		});
	}

	void IblTest::CheckPrefilterUniform()
	{
		ReadSpecular(m_Uniform, m_Alive, [this](const SpecularReadback& readback)
		{
			float worst = 0.0f;
			uint32_t wrongSize = 0;
			for (uint32_t mip = 0; mip < k_SpecularMips; ++mip)
			{
				const uint32_t size = k_SpecularFace >> mip;
				for (uint32_t face = 0; face < 6; ++face)
				{
					const std::vector<glm::vec3>& texels = readback.Pixels[mip * 6 + face];
					if (texels.size() != static_cast<size_t>(size) * size)
						++wrongSize;
					for (const glm::vec3& texel : texels)
						worst = (std::max)(worst, (std::max)({ std::abs(texel.r - 1.0f), std::abs(texel.g - 1.0f), std::abs(texel.b - 1.0f) }));
				}
			}
			Check(readback.Empty == 0 && wrongSize == 0 && worst <= 1e-3f, std::format("IBL 4a: the uniform environment prefilters to 1 on every face of all 6 mips (worst {:.5f}, {} empty readbacks)", worst, readback.Empty));
		});
	}

	void IblTest::CheckPrefilterAnalytic()
	{
		ReadSpecular(m_Analytic, m_Alive, [this](const SpecularReadback& readback)
		{
			float worstMip0 = 0.0f;
			if (readback.Empty == 0)
			{
				for (uint32_t face = 0; face < 6; ++face)
				{
					const std::vector<glm::vec3>& texels = readback.Pixels[face];
					for (uint32_t y = 0; y < k_SpecularFace; ++y)
					{
						for (uint32_t x = 0; x < k_SpecularFace; ++x)
							worstMip0 = (std::max)(worstMip0, RelativeError(texels[static_cast<size_t>(y) * k_SpecularFace + x], AnalyticRadiance(TexelDirection(face, x, y, k_SpecularFace))));
					}
				}
			}
			Check(readback.Empty == 0 && worstMip0 < 0.015f, std::format("IBL 4b: mip 0 of the analytic environment's specular cube is its function on all six faces (worst relative error {:.3f} %)", worstMip0 * 100.0f));

			std::string perMip;
			float worstMip5 = 0.0f;
			for (uint32_t mip = 1; mip < k_SpecularMips && readback.Empty == 0; ++mip)
			{
				const uint32_t size = k_SpecularFace >> mip;
				const float roughness = GlslIbl::DE_PrefilterRoughness(static_cast<float>(mip));
				const PrefilterBands bands = MonteCarloBands(roughness);
				float worst = 0.0f;
				for (uint32_t face = 0; face < 6; ++face)
				{
					const std::vector<glm::vec3>& texels = readback.Pixels[mip * 6 + face];
					for (uint32_t y = 0; y < size; ++y)
					{
						for (uint32_t x = 0; x < size; ++x)
							worst = (std::max)(worst, RelativeError(texels[static_cast<size_t>(y) * size + x], FunkHeckePrefilter(TexelDirection(face, x, y, size), bands)));
					}
				}
				perMip += std::format(" mip {} {:.2f} %", mip, worst * 100.0f);
				if (mip == k_SpecularMips - 1)
					worstMip5 = worst;
			}
			Check(readback.Empty == 0 && worstMip5 < 0.03f, std::format("IBL 4c: mip 5 matches the Funk-Hecke form of the same GGX integral on every texel (worst relative error {:.3f} %; per mip:{})", worstMip5 * 100.0f, perMip));
		});
	}

	namespace
	{
		const Surface k_DiffuseSurface{ glm::vec3(0.5f), 0.0f, 1.0f };
		const Surface k_ChromeSurface{ glm::vec3(1.0f), 1.0f, 0.2f };
		const glm::vec3 k_Forward(0.0f, 0.0f, -1.0f);

		bool Near(const glm::vec3& value, const glm::vec3& expected, float relative, float absolute)
		{
			for (int i = 0; i < 3; ++i)
			{
				if (std::abs(value[i] - expected[i]) > relative * std::abs(expected[i]) + absolute)
					return false;
			}
			return true;
		}
	}

	void IblTest::CheckFurnace()
	{
		SceneEnvironment sky;
		sky.Environment = m_Uniform;
		const std::array<Surface, 4> surfaces = { {
			{ glm::vec3(0.9f, 0.6f, 0.3f), 0.0f, 0.5f },
			{ glm::vec3(0.9f, 0.6f, 0.3f), 1.0f, 0.25f },
			{ glm::vec3(0.9f, 0.6f, 0.3f), 1.0f, 1.0f },
			{ glm::vec3(0.5f), 0.0f, 1.0f } } };

		std::vector<Framebuffer*> targets;
		bool lit = true;
		for (const Surface& surface : surfaces)
		{
			Material* material = CreatePbr(surface.Base, surface.Metallic, surface.Roughness);
			Framebuffer* environmentTarget = NewTarget("IblTest furnace", k_ShotSize);
			Framebuffer* flatTarget = NewTarget("IblTest flat", k_ShotSize);
			lit = lit && ShotOfSphere(environmentTarget, k_Forward, &sky, material, 1.0f).EnvironmentLighting;
			ShotOfSphere(flatTarget, k_Forward, nullptr, material, 1.0f);
			targets.push_back(environmentTarget);
			targets.push_back(flatTarget);
		}

		ReadTargets(targets, [this, surfaces, lit](const std::vector<TexturePixels>& images)
		{
			float worstEnvironment = 0.0f;
			float worstFlat = 0.0f;
			float worstGap = 0.0f;
			std::string detail;
			for (size_t i = 0; i < surfaces.size(); ++i)
			{
				const glm::vec4 environment = CentrePixel(images[i * 2]);
				const glm::vec4 flat = CentrePixel(images[i * 2 + 1]);
				const glm::vec3 expectedEnvironment = SrgbEncode(FurnaceLighting(surfaces[i]));
				const glm::vec3 expectedFlat = SrgbEncode(FlatLighting(surfaces[i], 1.0f));
				worstEnvironment = (std::max)(worstEnvironment, MaxDifference(environment, expectedEnvironment));
				worstFlat = (std::max)(worstFlat, MaxDifference(flat, expectedFlat));
				if (surfaces[i].Metallic == 0.0f)
				{
					const float gap = MaxDifference(glm::vec4(expectedEnvironment, 1.0f), expectedFlat);
					worstGap = (std::max)(worstGap, (std::max)(MaxDifference(environment, glm::vec3(flat)) - gap, 0.0f));
				}
				detail += std::format(" [{}: got {} want {}]", i, Rgb(environment), Rgb(expectedEnvironment));
			}
			Check(lit && worstEnvironment < k_Tolerance, std::format("IBL 5a: a white furnace (radiance 1 everywhere, flat ambient off) lights PBR spheres as diffuse + F0 A + B from the LUT (worst {:.4f}, lit {};{})", worstEnvironment, lit, detail));
			Check(worstFlat < 0.01f && worstGap < k_Tolerance, std::format("IBL 5b: the flat AmbientColor 1 path is unchanged (worst {:.4f} off the EnvBRDFApprox pixel) and the dielectrics stay within the CPU-computed gap of the furnace (excess {:.4f})", worstFlat, worstGap));
		});
	}

	void IblTest::CheckAnalyticLighting()
	{
		SceneEnvironment sky;
		sky.Environment = m_Analytic;
		const std::array<glm::vec3, 2> diffuseViews = { glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f) };
		const std::array<glm::vec3, 2> chromeViews = { k_Forward, glm::vec3(0.0f, 1.0f, 0.0f) };

		std::vector<Framebuffer*> targets;
		bool lit = true;
		for (const glm::vec3& view : diffuseViews)
		{
			Framebuffer* target = NewTarget("IblTest analytic diffuse", k_ShotSize);
			lit = lit && ShotOfSphere(target, view, &sky, m_DiffuseMaterial).EnvironmentLighting;
			targets.push_back(target);
		}
		for (const glm::vec3& view : chromeViews)
		{
			Framebuffer* target = NewTarget("IblTest analytic chrome", k_ShotSize);
			lit = lit && ShotOfSphere(target, view, &sky, m_ChromeMaterial).EnvironmentLighting;
			targets.push_back(target);
		}

		ReadTargets(targets, [this, diffuseViews, chromeViews, lit](const std::vector<TexturePixels>& images)
		{
			float worstDiffuse = 0.0f;
			float worstChrome = 0.0f;
			std::string detail;
			for (size_t i = 0; i < 2; ++i)
			{
				const glm::vec4 diffuse = CentrePixel(images[i]);
				const glm::vec3 expectedDiffuse = SrgbEncode(CentreLighting(k_AnalyticModel, k_DiffuseSurface, 1.0f, -diffuseViews[i]));
				worstDiffuse = (std::max)(worstDiffuse, MaxDifference(diffuse, expectedDiffuse));
				const glm::vec4 chrome = CentrePixel(images[2 + i]);
				const glm::vec3 expectedChrome = SrgbEncode(CentreLighting(k_AnalyticModel, k_ChromeSurface, 1.0f, -chromeViews[i]));
				worstChrome = (std::max)(worstChrome, MaxDifference(chrome, expectedChrome));
				detail += std::format(" [diffuse {} got {} want {}] [chrome {} got {} want {}]", i, Rgb(diffuse), Rgb(expectedDiffuse), i, Rgb(chrome), Rgb(expectedChrome));
			}
			Check(lit && worstDiffuse < k_Tolerance, std::format("IBL 6a: the analytic environment lights a diffuse sphere seen along +Y and -Y as its closed-form irradiance (worst {:.4f})", worstDiffuse));
			Check(lit && worstChrome < k_Tolerance, std::format("IBL 6b: a chrome sphere's centre is the prefiltered sky it reflects times A + B (worst {:.4f};{})", worstChrome, detail));
		});
	}

	void IblTest::CheckIntensity()
	{
		const Surface surface{ glm::vec3(0.2f), 0.0f, 0.5f };
		Material* material = CreatePbr(surface.Base, surface.Metallic, surface.Roughness);
		auto makeSky = [this](float intensity, float lightingIntensity, bool lighting)
		{
			SceneEnvironment sky;
			sky.Environment = m_Analytic;
			sky.Intensity = intensity;
			sky.LightingIntensity = lightingIntensity;
			sky.Lighting = lighting;
			return sky;
		};
		const std::array<SceneEnvironment, 6> skies = { makeSky(1.0f, 1.0f, true), makeSky(2.0f, 1.0f, true), makeSky(1.0f, 0.5f, true), makeSky(2.0f, 0.5f, true), makeSky(0.0f, 1.0f, true), makeSky(1.0f, 1.0f, false) };

		std::vector<Framebuffer*> targets;
		std::array<bool, 6> lit = {};
		for (size_t i = 0; i < skies.size(); ++i)
		{
			Framebuffer* target = NewTarget("IblTest intensity", k_ShotSize);
			lit[i] = ShotOfSphere(target, k_Forward, &skies[i], material).EnvironmentLighting;
			targets.push_back(target);
		}
		Framebuffer* flatTarget = NewTarget("IblTest intensity flat", k_ShotSize);
		ShotOfSphere(flatTarget, k_Forward, nullptr, material);
		targets.push_back(flatTarget);

		ReadTargets(targets, [this, surface, lit](const std::vector<TexturePixels>& images)
		{
			std::array<glm::vec3, 6> linear;
			for (size_t i = 0; i < 6; ++i)
				linear[i] = SrgbDecode(glm::vec3(CentrePixel(images[i])));
			const glm::vec3 expected = CentreLighting(k_AnalyticModel, surface, 1.0f, -k_Forward);
			const bool base = lit[0] && Near(linear[0], expected, 0.06f, 0.01f);
			const bool scaled = lit[1] && lit[2] && lit[3] && Near(linear[1], linear[0] * 2.0f, 0.05f, 0.006f) && Near(linear[2], linear[0] * 0.5f, 0.05f, 0.006f) && Near(linear[3], linear[0], 0.05f, 0.006f);
			const glm::vec4 zero = CentrePixel(images[4]);
			const bool black = lit[4] && zero.r < 0.01f && zero.g < 0.01f && zero.b < 0.01f;
			Check(base && scaled && black, std::format("IBL 7a: Intensity and LightingIntensity multiply the lighting linearly: x1 {} (want {}), x2 {}, x0.5 {}, 2 x 0.5 {}, Intensity 0 {}",
				Rgb(linear[0]), Rgb(expected), Rgb(linear[1]), Rgb(linear[2]), Rgb(linear[3]), Rgb(zero)));
			const glm::vec4 off = CentrePixel(images[5]);
			const glm::vec4 flat = CentrePixel(images[6]);
			Check(!lit[5] && SamePixel(off, flat), std::format("IBL 7b: Lighting = false draws the flat ambient pixel for pixel (environment lighting {}, off {} flat {})", lit[5], Rgb(off), Rgb(flat)));
		});
	}

	void IblTest::CheckRotation()
	{
		struct Shot
		{
			glm::vec3 View;
			float Rotation;
		};
		const std::array<Shot, 4> shots = { { { k_Forward, 0.0f }, { k_Forward, 90.0f }, { k_Forward, -90.0f }, { glm::vec3(1.0f, 0.0f, 0.0f), 90.0f } } };

		std::vector<Framebuffer*> targets;
		bool lit = true;
		for (const Shot& shot : shots)
		{
			SceneEnvironment sky;
			sky.Environment = m_Azimuth;
			sky.Rotation = shot.Rotation;
			Framebuffer* target = NewTarget("IblTest rotation", k_ShotSize);
			lit = lit && ShotOfSphere(target, shot.View, &sky, m_ChromeMaterial).EnvironmentLighting;
			targets.push_back(target);
		}

		ReadTargets(targets, [this, shots, lit](const std::vector<TexturePixels>& images)
		{
			float worst = 0.0f;
			std::string detail;
			for (size_t i = 0; i < shots.size(); ++i)
			{
				const glm::vec4 pixel = CentrePixel(images[i]);
				const glm::vec3 expected = SrgbEncode(CentreLighting(k_AzimuthModel, k_ChromeSurface, 1.0f, -shots[i].View, shots[i].Rotation));
				worst = (std::max)(worst, MaxDifference(pixel, expected));
				detail += std::format(" [{} deg: got {} want {}]", shots[i].Rotation, Rgb(pixel), Rgb(expected));
			}
			const float apart = MaxDifference(CentrePixel(images[1]), glm::vec3(CentrePixel(images[2])));
			Check(lit && worst < k_Tolerance && apart > 0.05f, std::format("IBL 8: Rotation turns the lighting with the sky, so a chrome sphere keeps matching the sky it reflects (worst {:.4f}, +90 and -90 apart by {:.3f};{})", worst, apart, detail));
		});
	}

	void IblTest::CheckLitUnchanged()
	{
		SceneEnvironment sky;
		sky.Environment = m_Uniform;
		auto submit = [this](Renderer3D& renderer)
		{
			DirectionalLight sun;
			sun.Direction = { -0.3f, -1.0f, -0.6f };
			sun.Intensity = 1.0f;
			renderer.SubmitLight(sun);
			renderer.SetAmbientLight(glm::vec3(1.0f), 0.3f);
			renderer.SubmitMesh(m_Sphere, glm::translate(glm::mat4(1.0f), k_Forward * 3.0f), glm::vec4(0.3f, 0.6f, 0.9f, 1.0f));
		};
		std::vector<Framebuffer*> targets = { NewTarget("IblTest lit sky", k_ShotSize), NewTarget("IblTest lit none", k_ShotSize) };
		const Renderer3D::Statistics withStats = RenderShot(targets[0], ShotMatrix(k_Forward), &sky, submit);
		RenderShot(targets[1], ShotMatrix(k_Forward), nullptr, submit);
		ReadTargets(targets, [this, withStats](const std::vector<TexturePixels>& images)
		{
			uint32_t covered = 0;
			const uint32_t changed = CountDifferent(images[1], images[0], 0.005f, &covered);
			Check(withStats.EnvironmentLighting && covered > 100 && changed == 0, std::format("IBL 9: a lit (non-PBR) sphere draws the same with and without the environment ({} mesh pixels, {} changed, environment lighting {})", covered, changed, withStats.EnvironmentLighting));
		});
	}

	void IblTest::CheckTwoEnvironments()
	{
		SceneEnvironment analytic;
		analytic.Environment = m_Analytic;
		SceneEnvironment azimuth;
		azimuth.Environment = m_Azimuth;
		const std::array<bool, 6> useAzimuth = { false, true, false, false, true, true };
		const glm::vec3 view(-1.0f, 0.0f, 0.0f);

		std::vector<Framebuffer*> targets;
		std::array<uint32_t, 6> copies = {};
		bool lit = true;
		for (size_t i = 0; i < useAzimuth.size(); ++i)
		{
			Framebuffer* target = NewTarget("IblTest two environments", k_ShotSize);
			const Renderer3D::Statistics stats = ShotOfSphere(target, view, useAzimuth[i] ? &azimuth : &analytic, m_DiffuseMaterial);
			copies[i] = stats.EnvironmentLightingCopies;
			lit = lit && stats.EnvironmentLighting;
			targets.push_back(target);
		}

		ReadTargets(targets, [this, useAzimuth, copies, lit, view](const std::vector<TexturePixels>& images)
		{
			const bool copiesOk = copies[0] <= 1 && copies[1] == 1 && copies[2] == 1 && copies[3] == 0 && copies[4] == 1 && copies[5] == 0;
			Check(lit && copiesOk, std::format("IBL 10a: scenes of one frame copy the lighting only when the environment changes (copies per scene {} {} {} {} {} {}, want <=1 1 1 0 1 0)", copies[0], copies[1], copies[2], copies[3], copies[4], copies[5]));

			const glm::vec3 expected[2] = { SrgbEncode(CentreLighting(k_AnalyticModel, k_DiffuseSurface, 1.0f, -view)), SrgbEncode(CentreLighting(k_AzimuthModel, k_DiffuseSurface, 1.0f, -view)) };
			float worst = 0.0f;
			for (size_t i = 0; i < useAzimuth.size(); ++i)
				worst = (std::max)(worst, MaxDifference(CentrePixel(images[i]), expected[useAzimuth[i] ? 1 : 0]));
			const float apart = MaxDifference(CentrePixel(images[0]), glm::vec3(CentrePixel(images[1])));
			Check(worst < k_Tolerance && apart > 0.05f, std::format("IBL 10b: each scene is lit by its own environment (worst {:.4f}, the two environments differ by {:.3f})", worst, apart));
		});
	}

	void IblTest::CheckHotReload()
	{
		AssetManager& assets = Application::Get().GetAssetManager();
		const std::filesystem::path file = m_Folder / "reload.hdr";
		const glm::vec3 first(0.2f, 0.3f, 0.4f);
		const glm::vec3 second(1.0f, 0.5f, 0.25f);
		WritePanorama(file, [first](const glm::vec3&) { return first; });

		const AssetHandle handle = assets.Load(file);
		Environment* loaded = assets.GetEnvironment(handle);
		if (!loaded || !loaded->Prepare() || !loaded->IsReady())
		{
			Check(false, "IBL 11: a .hdr loaded through the AssetManager bakes");
			return;
		}

		SceneEnvironment sky;
		sky.Environment = loaded;
		Framebuffer* before = NewTarget("IblTest reload before", k_ShotSize);
		Framebuffer* after = NewTarget("IblTest reload after", k_ShotSize);
		const Renderer3D::Statistics beforeStats = ShotOfSphere(before, k_Forward, &sky, m_DiffuseMaterial);
		const uint64_t generation = Internal::EnvironmentAccess::GetBakeGeneration(loaded);

		WritePanorama(file, [second](const glm::vec3&) { return second; });
		const bool reloaded = assets.Reload(handle);
		const bool samePointer = assets.GetEnvironment(handle) == loaded;
		const bool rebaked = loaded->Prepare() && loaded->IsReady();
		const uint64_t newGeneration = Internal::EnvironmentAccess::GetBakeGeneration(loaded);
		const Renderer3D::Statistics afterStats = ShotOfSphere(after, k_Forward, &sky, m_DiffuseMaterial);

		ReadTargets({ before, after }, [this, first, second, reloaded, samePointer, rebaked, generation, newGeneration, beforeStats, afterStats](const std::vector<TexturePixels>& images)
		{
			const glm::vec3 expectedBefore = SrgbEncode(SurfaceLighting(k_DiffuseSurface, 1.0f, first, first));
			const glm::vec3 expectedAfter = SrgbEncode(SurfaceLighting(k_DiffuseSurface, 1.0f, second, second));
			const float beforeError = MaxDifference(CentrePixel(images[0]), expectedBefore);
			const float afterError = MaxDifference(CentrePixel(images[1]), expectedAfter);
			const bool brighter = CentrePixel(images[1]).r > CentrePixel(images[0]).r + 0.05f;
			Check(reloaded && samePointer && rebaked && newGeneration > generation && beforeStats.EnvironmentLighting && afterStats.EnvironmentLightingCopies > 0 && beforeError < k_Tolerance && afterError < k_Tolerance && brighter,
				std::format("IBL 11: a reloaded .hdr keeps its Environment, bakes again (generation {} -> {}), re-copies the lighting ({} copies) and lights the sphere with the new radiance (error before {:.4f}, after {:.4f}; red {:.3f} -> {:.3f})",
					generation, newGeneration, afterStats.EnvironmentLightingCopies, beforeError, afterError, CentrePixel(images[0]).r, CentrePixel(images[1]).r));
		});
	}

	void IblTest::CheckDeleted()
	{
		const glm::vec3 view(-1.0f, 0.0f, 0.0f);
		const std::filesystem::path azimuthFile = m_Folder / "azimuth.hdr";
		Environment* temporary = Environment::Create(EnvironmentParams().SetPath(m_AnalyticFile).SetDebugName("IblTest temporary"));
		if (!temporary || !temporary->Prepare() || !temporary->IsReady())
		{
			delete temporary;
			Check(false, "IBL 12: a second Environment loads and bakes");
			return;
		}

		Framebuffer* before = NewTarget("IblTest before delete", k_ShotSize);
		Framebuffer* during = NewTarget("IblTest during", k_ShotSize);
		Framebuffer* after = NewTarget("IblTest after delete", k_ShotSize);
		Framebuffer* replaced = NewTarget("IblTest replacement", k_ShotSize);
		ShotOfSphere(before, view, nullptr, m_DiffuseMaterial);
		SceneEnvironment sky;
		sky.Environment = temporary;
		const Renderer3D::Statistics duringStats = ShotOfSphere(during, view, &sky, m_DiffuseMaterial);
		delete temporary;
		const Renderer3D::Statistics afterStats = ShotOfSphere(after, view, nullptr, m_DiffuseMaterial);

		Renderer3D::Statistics replacedStats;
		Environment* replacement = Environment::Create(EnvironmentParams().SetPath(azimuthFile).SetDebugName("IblTest replacement"));
		if (replacement && replacement->Prepare() && replacement->IsReady())
		{
			sky.Environment = replacement;
			replacedStats = ShotOfSphere(replaced, view, &sky, m_DiffuseMaterial);
		}
		delete replacement;

		ReadTargets({ before, during, after, replaced }, [this, view, duringStats, afterStats, replacedStats](const std::vector<TexturePixels>& images)
		{
			const glm::vec3 expectedAnalytic = SrgbEncode(CentreLighting(k_AnalyticModel, k_DiffuseSurface, 1.0f, -view));
			const glm::vec3 expectedAzimuth = SrgbEncode(CentreLighting(k_AzimuthModel, k_DiffuseSurface, 1.0f, -view));
			const float duringError = MaxDifference(CentrePixel(images[1]), expectedAnalytic);
			const bool gone = !afterStats.EnvironmentLighting && SamePixel(CentrePixel(images[0]), CentrePixel(images[2]));
			Check(duringStats.EnvironmentLighting && duringError < k_Tolerance && gone, std::format("IBL 12a: an Environment lights a sphere, and once deleted the next scene is the flat ambient again (error {:.4f}, lighting after delete {}, flat before {} after {})",
				duringError, afterStats.EnvironmentLighting, Rgb(CentrePixel(images[0])), Rgb(CentrePixel(images[2]))));
			const float replacedError = MaxDifference(CentrePixel(images[3]), expectedAzimuth);
			Check(replacedStats.EnvironmentLighting && replacedStats.EnvironmentLightingCopies == 1 && replacedError < k_Tolerance, std::format("IBL 12b: a new Environment lights correctly after the old one is gone (error {:.4f}, copies {})", replacedError, replacedStats.EnvironmentLightingCopies));
		});
	}

	void IblTest::CheckOcclusion()
	{
		constexpr uint8_t k_Half = 128;
		const std::array<uint8_t, 16 * 4> texels = [] { std::array<uint8_t, 16 * 4> data = {}; for (size_t i = 0; i < 16; ++i) data[i * 4] = data[i * 4 + 1] = data[i * 4 + 2] = k_Half, data[i * 4 + 3] = 255; return data; }();
		Texture* halfMap = Texture::CreateFromData(4, 4, texels.data(), TextureFormat::RGBA8_UNORM, "IblTest_HalfOcclusion");
		if (!halfMap)
		{
			Check(false, "IBL 13: the occlusion map texture is created");
			return;
		}
		m_Textures.push_back(halfMap);

		const Surface surface{ glm::vec3(0.7f, 0.6f, 0.5f), 0.5f, 0.4f };
		Material* occluded = CreatePbr(PbrMaterialParams()
			.SetMaterial(MaterialParams().SetDebugName("IblTest_Occluded"))
			.SetBaseColor(glm::vec4(surface.Base, 1.0f))
			.SetMetallic(surface.Metallic)
			.SetRoughness(surface.Roughness)
			.SetOcclusionMap(halfMap));
		Material* open = CreatePbr(surface.Base, surface.Metallic, surface.Roughness);
		SceneEnvironment sky;
		sky.Environment = m_Analytic;
		const glm::vec3 view(-1.0f, 0.0f, 0.0f);
		Framebuffer* occludedTarget = NewTarget("IblTest occluded", k_ShotSize);
		Framebuffer* openTarget = NewTarget("IblTest open", k_ShotSize);
		const bool lit = ShotOfSphere(occludedTarget, view, &sky, occluded).EnvironmentLighting && ShotOfSphere(openTarget, view, &sky, open).EnvironmentLighting;

		ReadTargets({ occludedTarget, openTarget }, [this, surface, view, lit](const std::vector<TexturePixels>& images)
		{
			const float occlusion = static_cast<float>(k_Half) / 255.0f;
			const glm::vec3 expectedOccluded = SrgbEncode(CentreLighting(k_AnalyticModel, surface, occlusion, -view));
			const glm::vec3 expectedOpen = SrgbEncode(CentreLighting(k_AnalyticModel, surface, 1.0f, -view));
			const float occludedError = MaxDifference(CentrePixel(images[0]), expectedOccluded);
			const float openError = MaxDifference(CentrePixel(images[1]), expectedOpen);
			const float apart = MaxDifference(CentrePixel(images[0]), glm::vec3(CentrePixel(images[1])));
			Check(lit && occludedError < k_Tolerance && openError < k_Tolerance && apart > 0.05f, std::format("IBL 13: a material occlusion of {:.3f} scales the diffuse term and the specular occlusion (occluded error {:.4f}, open error {:.4f}, the two differ by {:.3f})", occlusion, occludedError, openError, apart));
		});
	}

	void IblTest::CheckPaths()
	{
		SceneEnvironment sky;
		sky.Environment = m_Analytic;
		sky.DrawSkybox = false;
		const glm::vec3 view = glm::normalize(glm::vec3(-1.0f, 0.4f, -0.6f));
		const glm::mat4 model = glm::translate(glm::mat4(1.0f), view * 3.0f);
		const glm::mat4 viewProjection = ShotMatrix(view);
		const glm::vec4 white(1.0f);
		const glm::vec3 base(0.8f, 0.5f, 0.3f);
		Material* plain = CreatePbr(base, 0.3f, 0.4f);
		Material* translucent = CreatePbr(PbrMaterialParams()
			.SetMaterial(MaterialParams().SetDebugName("IblTest_Translucent").SetTranslucent(true))
			.SetBaseColor(glm::vec4(base, 1.0f))
			.SetMetallic(0.3f)
			.SetRoughness(0.4f));
		m_StaticBatch = m_Renderer->CreateStaticBatch();
		if (m_StaticBatch)
			m_StaticBatch->Add(StaticMesh().SetMesh(m_Sphere).SetTransform(model).SetColor(white).SetMaterial(plain));

		auto shot = [&](const char* name, const std::function<void(Renderer3D&)>& submit, Renderer3D::Statistics& stats)
		{
			Framebuffer* target = NewTarget(name, k_PathShotSize);
			stats = RenderShot(target, viewProjection, &sky, [&submit](Renderer3D& renderer)
			{
				renderer.SetAmbientLight(glm::vec3(1.0f), 0.3f);
				submit(renderer);
			});
			return target;
		};
		Renderer3D::Statistics plainStats, skinnedStats, instancedStats, translucentStats, staticStats;
		std::vector<Framebuffer*> targets;
		targets.push_back(shot("IblTest path plain", [&](Renderer3D& renderer) { renderer.SubmitMesh(m_Sphere, model, white, plain); }, plainStats));
		targets.push_back(shot("IblTest path skinned", [&](Renderer3D& renderer) { renderer.SubmitSkinnedMesh(m_SkinnedSphere, model, std::span<const glm::mat4>(m_Palette), white, plain); }, skinnedStats));
		targets.push_back(shot("IblTest path instanced", [&](Renderer3D& renderer)
		{
			for (int i = 0; i < 8; ++i)
				renderer.SubmitMesh(m_Sphere, model, white, plain);
		}, instancedStats));
		targets.push_back(shot("IblTest path translucent", [&](Renderer3D& renderer) { renderer.SubmitMesh(m_Sphere, model, white, translucent); }, translucentStats));
		targets.push_back(shot("IblTest path static", [&](Renderer3D& renderer)
		{
			if (m_StaticBatch)
				renderer.SubmitStaticBatch(*m_StaticBatch);
		}, staticStats));

		ReadTargets(targets, [this, plainStats, skinnedStats, instancedStats, translucentStats, staticStats](const std::vector<TexturePixels>& images)
		{
			uint32_t covered = 0;
			CountDifferent(images[0], images[0], 0.01f, &covered);
			Check(plainStats.EnvironmentLighting && plainStats.PbrDraws > 0 && covered > 300, std::format("IBL 14: the plain PBR sphere is drawn and lit by the environment ({} mesh pixels)", covered));

			struct Path
			{
				const char* Name;
				const Renderer3D::Statistics& Stats;
				bool Taken;
			};
			const Path paths[4] = {
				{ "skinned", skinnedStats, skinnedStats.SkinnedDraws > 0 },
				{ "instanced", instancedStats, instancedStats.InstancedMeshes > 0 },
				{ "translucent", translucentStats, translucentStats.TranslucentMeshes > 0 },
				{ "static batch", staticStats, staticStats.DrawCalls > 0 } };
			for (size_t i = 0; i < 4; ++i)
			{
				uint32_t pathCovered = 0;
				const uint32_t differing = CountDifferent(images[0], images[i + 1], 0.01f, &pathCovered);
				const bool matches = differing != 0xFFFFFFFFu && differing <= (std::max)(2u, pathCovered / 100u);
				Check(paths[i].Taken && paths[i].Stats.EnvironmentLighting && matches, std::format("IBL 14: a {} PBR sphere matches the plain draw ({} of {} pixels differ, path taken {}, environment lighting {})",
					paths[i].Name, differing == 0xFFFFFFFFu ? 0u : differing, pathCovered, paths[i].Taken, paths[i].Stats.EnvironmentLighting));
			}
		});
	}

	void IblTest::CheckOrthographic()
	{
		SceneEnvironment sky;
		sky.Environment = m_Analytic;
		Framebuffer* target = NewTarget("IblTest orthographic", k_ShotSize);
		const Renderer3D::Statistics stats = RenderShot(target, glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, 0.1f, 100.0f), &sky, [this](Renderer3D& renderer)
		{
			renderer.SetAmbientLight(glm::vec3(1.0f), 0.3f);
			renderer.SubmitMesh(m_Sphere, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -4.0f)), glm::vec4(1.0f), m_DiffuseMaterial);
		});
		ReadTargets({ target }, [this, stats](const std::vector<TexturePixels>& images)
		{
			const glm::vec3 expected = SrgbEncode(CentreLighting(k_AnalyticModel, k_DiffuseSurface, 1.0f, -k_Forward));
			const glm::vec4 pixel = CentrePixel(images[0]);
			const float error = MaxDifference(pixel, expected);
			const glm::vec4 corner = images[0].Data.empty() ? glm::vec4(0.0f) : images[0].GetPixel(1, 1);
			Check(stats.EnvironmentLighting && !stats.Skybox && error < k_Tolerance && IsClear(corner), std::format("IBL 15: an orthographic camera is lit by the environment without drawing the sky (lighting {}, sky drawn {}, error {:.4f}, corner clear {})", stats.EnvironmentLighting, stats.Skybox, error, IsClear(corner)));
		});
	}

	void IblTest::CheckSceneDriven()
	{
		const glm::vec3 view = glm::normalize(glm::vec3(-1.0f, 0.4f, -0.6f));
		const glm::vec3 position = view * 3.0f;
		const glm::mat4 viewProjection = ShotMatrix(view);
		const Surface surface{ glm::vec3(0.7f, 0.6f, 0.5f), 0.5f, 0.4f };
		Material* material = CreatePbr(surface.Base, surface.Metallic, surface.Roughness);
		Environment* environment = m_Analytic;
		Mesh* sphere = m_Sphere;

		auto addSphere = [position, sphere, material](Scene& scene)
		{
			Entity entity = scene.CreateEntity("Sphere");
			entity.AddComponent<Transform3DComponent>(position);
			entity.AddComponent<MeshRendererComponent>(sphere).Material = material;
		};
		auto sceneShot = [&](const char* name, const SkyboxComponent* sky, Renderer3D::Statistics& stats)
		{
			Framebuffer* target = NewTarget(name, k_PathShotSize);
			stats = RenderShot(target, viewProjection, nullptr, [&](Renderer3D& renderer)
			{
				Scene scene("IBL Check");
				addSphere(scene);
				if (sky)
					scene.CreateEntity("Sky").AddComponent<SkyboxComponent>(*sky);
				scene.SubmitLights(renderer);
				scene.RenderEntities3D(renderer);
			});
			return target;
		};
		auto directShot = [&](const char* name, bool drawSkybox, Renderer3D::Statistics& stats)
		{
			Framebuffer* target = NewTarget(name, k_PathShotSize);
			stats = RenderShot(target, viewProjection, nullptr, [&](Renderer3D& renderer)
			{
				Scene scene("IBL Check");
				addSphere(scene);
				scene.SubmitLights(renderer);
				scene.RenderEntities3D(renderer);
				SceneEnvironment direct;
				direct.Environment = environment;
				direct.DrawSkybox = drawSkybox;
				renderer.SetEnvironment(direct);
			});
			return target;
		};

		SkyboxComponent visible(environment);
		SkyboxComponent hidden(environment);
		hidden.Visible = false;
		SkyboxComponent unlit(environment);
		unlit.Lighting = false;

		Renderer3D::Statistics directStats, sceneStats, directHiddenStats, sceneHiddenStats, unlitStats, flatStats;
		std::vector<Framebuffer*> targets = {
			directShot("IblTest scene direct", true, directStats),
			sceneShot("IblTest scene visible", &visible, sceneStats),
			directShot("IblTest scene direct hidden", false, directHiddenStats),
			sceneShot("IblTest scene hidden", &hidden, sceneHiddenStats),
			sceneShot("IblTest scene unlit", &unlit, unlitStats),
			sceneShot("IblTest scene flat", nullptr, flatStats) };

		ReadTargets(targets, [this, directStats, sceneStats, directHiddenStats, sceneHiddenStats, unlitStats, flatStats](const std::vector<TexturePixels>& images)
		{
			constexpr float k_OneStep = 1.5f / 255.0f;
			uint32_t covered = 0;
			const uint32_t visibleDiffering = CountDifferent(images[0], images[1], k_OneStep, &covered);
			Check(directStats.EnvironmentLighting && sceneStats.EnvironmentLighting && sceneStats.Skybox && visibleDiffering == 0 && covered > 300,
				std::format("IBL 16a: a SkyboxComponent lights a PBR MeshRendererComponent exactly as SetEnvironment does ({} of {} pixels differ, lighting {}, sky {})", visibleDiffering == 0xFFFFFFFFu ? 0u : visibleDiffering, covered, sceneStats.EnvironmentLighting, sceneStats.Skybox));

			uint32_t hiddenCovered = 0;
			const uint32_t hiddenDiffering = CountDifferent(images[2], images[3], k_OneStep, &hiddenCovered);
			const float litApart = MaxDifference(images[3].GetPixel(k_PathShotSize / 2, k_PathShotSize / 2), glm::vec3(images[5].GetPixel(k_PathShotSize / 2, k_PathShotSize / 2)));
			Check(directHiddenStats.EnvironmentLighting && sceneHiddenStats.EnvironmentLighting && !sceneHiddenStats.Skybox && hiddenDiffering == 0 && hiddenCovered > 300 && litApart > 0.02f,
				std::format("IBL 16b: Visible = false keeps the lighting and draws no sky ({} of {} pixels differ from the direct path, sky {}, lit against flat by {:.3f})", hiddenDiffering == 0xFFFFFFFFu ? 0u : hiddenDiffering, hiddenCovered, sceneHiddenStats.Skybox, litApart));

			const glm::vec4 unlitPixel = images[4].GetPixel(k_PathShotSize / 2, k_PathShotSize / 2);
			const glm::vec4 flatPixel = images[5].GetPixel(k_PathShotSize / 2, k_PathShotSize / 2);
			Check(!unlitStats.EnvironmentLighting && !flatStats.EnvironmentLighting && SamePixel(unlitPixel, flatPixel, k_OneStep),
				std::format("IBL 16c: Lighting = false falls back to the flat ambient (lighting {}, unlit {} flat {})", unlitStats.EnvironmentLighting, Rgb(unlitPixel), Rgb(flatPixel)));
		});
	}

	void IblTest::DrawView()
	{
		if (!m_Renderer || !m_Analytic || !m_Uniform || !m_Analytic->IsReady() || !m_Uniform->IsReady() || m_GridMaterials.size() != 10)
			return;

		Framebuffer* output = Renderer::GetRenderTarget();
		const float halfWidth = static_cast<float>(output ? output->GetWidth() : m_Width) * 0.5f;
		const float halfHeight = static_cast<float>(output ? output->GetHeight() : m_Height) * 0.5f;

		auto draw = [&](const Viewport& viewport, const SceneEnvironment* sky, const std::function<void(Renderer3D&)>& submit)
		{
			Renderer::SetViewport(viewport);
			PerspectiveCamera camera(60.0f, viewport.Width / viewport.Height, 0.1f, 100.0f);
			camera.SetPosition(glm::vec3(0.0f));
			camera.SetUp(glm::vec3(0.0f, 1.0f, 0.0f));
			camera.SetTarget(k_Forward);
			m_Renderer->BeginScene(camera.GetViewProjectionMatrix());
			m_Renderer->SetAmbientLight(glm::vec3(1.0f), 0.5f);
			submit(*m_Renderer);
			if (sky)
				m_Renderer->SetEnvironment(*sky);
			m_Renderer->EndScene();
		};

		auto grid = [this](Renderer3D& renderer)
		{
			for (size_t row = 0; row < 2; ++row)
			{
				for (size_t column = 0; column < 5; ++column)
				{
					const glm::vec3 centre(static_cast<float>(column) - 2.0f, row == 0 ? 0.55f : -0.55f, -4.5f);
					renderer.SubmitMesh(m_Sphere, glm::scale(glm::translate(glm::mat4(1.0f), centre), glm::vec3(0.45f)), glm::vec4(1.0f), m_GridMaterials[row * 5 + column]);
				}
			}
		};
		auto pair = [this](Renderer3D& renderer)
		{
			renderer.SubmitMesh(m_Sphere, glm::translate(glm::mat4(1.0f), glm::vec3(-1.2f, 0.0f, -4.0f)), glm::vec4(1.0f), m_DiffuseMaterial);
			renderer.SubmitMesh(m_Sphere, glm::translate(glm::mat4(1.0f), glm::vec3(1.2f, 0.0f, -4.0f)), glm::vec4(1.0f), m_ChromeMaterial);
		};
		auto bump = [this](Renderer3D& renderer)
		{
			renderer.SubmitMesh(m_Sphere, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -4.0f)), glm::vec4(1.0f), m_BumpMaterial);
		};

		SceneEnvironment analytic;
		analytic.Environment = m_Analytic;
		SceneEnvironment furnace;
		furnace.Environment = m_Uniform;
		SceneEnvironment flat = analytic;
		flat.Lighting = false;

		draw({ 0.0f, 0.0f, halfWidth, halfHeight }, &analytic, grid);
		draw({ halfWidth, 0.0f, halfWidth, halfHeight }, &furnace, grid);
		draw({ 0.0f, halfHeight, halfWidth, halfHeight }, &analytic, pair);
		draw({ halfWidth, halfHeight, halfWidth * 0.5f, halfHeight }, &analytic, bump);
		draw({ halfWidth * 1.5f, halfHeight, halfWidth * 0.5f, halfHeight }, &flat, bump);
		Renderer::ResetViewport();
	}

	void IblTest::Cleanup()
	{
		m_Alive.reset();
		for (Framebuffer*& target : m_Targets)
			DestroyAndDelete(target);
		m_Targets.clear();
		m_StaticBatch.reset();
		for (Material*& material : m_Materials)
			DestroyAndDelete(material);
		m_Materials.clear();
		m_GridMaterials.clear();
		m_DiffuseMaterial = nullptr;
		m_ChromeMaterial = nullptr;
		m_BumpMaterial = nullptr;
		for (Texture*& texture : m_Textures)
			DestroyAndDelete(texture);
		m_Textures.clear();
		if (m_Renderer)
		{
			m_Renderer->Shutdown();
			delete m_Renderer;
			m_Renderer = nullptr;
		}
		DestroyAndDelete(m_Sphere);
		DestroyAndDelete(m_SkinnedSphere);
		delete m_Uniform;
		m_Uniform = nullptr;
		delete m_Analytic;
		m_Analytic = nullptr;
		delete m_Azimuth;
		m_Azimuth = nullptr;
		std::error_code error;
		std::filesystem::remove_all(m_Folder, error);
		m_Checks.clear();
	}

	void IblTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
		m_Width = width;
		m_Height = height;
	}

	void IblTest::ImGuiRender()
	{
		ImGui::TextWrapped("Top left: a roughness x metallic grid under the analytic sky. Top right: the same grid in the white furnace. Bottom left: diffuse and chrome spheres. Bottom right: a normal-mapped occluded sphere lit by the environment (left) and by the flat ambient (right).");
		GraphicsTest::ImGuiRender();

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

}
