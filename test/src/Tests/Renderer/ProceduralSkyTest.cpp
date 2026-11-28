#include "ProceduralSkyTest.h"

#include "DingoEngine/Graphics/EnvironmentAccess.h"
#include "DingoEngine/Graphics/Renderer3DAccess.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <string_view>

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max

namespace GlslAtmosphere
{
	using glm::vec2;
	using glm::vec3;
	using glm::vec4;
	using glm::abs;
	using glm::acos;
	using glm::clamp;
	using glm::cos;
	using glm::exp;
	using glm::max;
	using glm::pow;
	using glm::sqrt;

#include "DingoEngine/Graphics/Shaders/Atmosphere.glsl"
}

namespace GlslCube
{
	using uint = uint32_t;
	using glm::vec2;
	using glm::vec3;
	using glm::asin;
	using glm::atan;
	using glm::clamp;
	using glm::cos;
	using glm::normalize;
	using glm::sin;

#include "DingoEngine/Graphics/Shaders/CubeMap.glsl"
}

#pragma pop_macro("max")
#pragma pop_macro("min")

namespace Dingo
{

	namespace
	{
		constexpr uint32_t k_ShotSize = 32;
		constexpr uint32_t k_FramesBeforeChecks = 3;
		constexpr uint32_t k_CostFrames = 30;
		const glm::vec4 k_Clear(1.0f, 0.0f, 1.0f, 1.0f);
		using CubeFaces = std::array<std::vector<glm::vec3>, 6>;

		glm::vec3 SrgbEncode(const glm::vec3& linear)
		{
			glm::vec3 out;
			for (int i = 0; i < 3; ++i)
			{
				const float c = (std::max)(linear[i], 0.0f);
				out[i] = c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
			}
			return out;
		}

		glm::vec3 SrgbDecode(const glm::vec3& encoded)
		{
			glm::vec3 out;
			for (int i = 0; i < 3; ++i)
			{
				const float c = encoded[i];
				out[i] = c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
			}
			return out;
		}

		glm::vec3 ToSun(float elevation, float azimuth)
		{
			const float e = glm::radians(elevation);
			const float a = glm::radians(azimuth);
			return glm::vec3(std::cos(e) * std::sin(a), std::sin(e), std::cos(e) * std::cos(a));
		}

		glm::vec3 Travel(float elevation, float azimuth)
		{
			return -ToSun(elevation, azimuth);
		}

		glm::vec3 Tilt(const glm::vec3& v, float degrees)
		{
			const glm::vec3 axis = glm::normalize(glm::cross(v, glm::vec3(0.0f, 1.0f, 0.0f)));
			return glm::vec3(glm::rotate(glm::mat4(1.0f), glm::radians(degrees), axis) * glm::vec4(v, 0.0f));
		}

		ProceduralSky MakeSky(float elevation, float azimuth)
		{
			ProceduralSky sky;
			sky.SunDirection = Travel(elevation, azimuth);
			return sky;
		}

		glm::vec3 Pixel(const TexturePixels& image, uint32_t x, uint32_t y)
		{
			return glm::vec3(image.GetPixel(x, y));
		}

		float MaxDifference(const glm::vec3& a, const glm::vec3& b)
		{
			return (std::max)({ std::abs(a.r - b.r), std::abs(a.g - b.g), std::abs(a.b - b.b) });
		}

		bool SameImage(const TexturePixels& a, const TexturePixels& b, uint32_t& differing)
		{
			differing = 0;
			if (a.Data.empty() || b.Data.empty() || a.Width != b.Width || a.Height != b.Height)
			{
				differing = ~0u;
				return false;
			}
			for (uint32_t y = 0; y < a.Height; ++y)
			{
				for (uint32_t x = 0; x < a.Width; ++x)
					differing += MaxDifference(Pixel(a, x, y), Pixel(b, x, y)) < 0.005f ? 0 : 1;
			}
			return differing == 0;
		}

		struct CubeSet
		{
			std::vector<CubeFaces> Cubes;
			size_t Remaining = 0;
			uint32_t Empty = 0;
			std::function<void(const CubeSet&)> Done;
		};

		void ReadCubes(const std::vector<Environment*>& environments, uint32_t mip, const std::weak_ptr<int>& alive, std::function<void(const CubeSet&)> done)
		{
			auto set = std::make_shared<CubeSet>();
			set->Cubes.resize(environments.size());
			set->Remaining = environments.size() * 6;
			set->Done = std::move(done);
			for (size_t e = 0; e < environments.size(); ++e)
			{
				const uint32_t size = (std::max)(environments[e]->GetFaceSize() >> mip, 1u);
				for (uint32_t face = 0; face < 6; ++face)
				{
					environments[e]->GetCubeMap()->ReadSubresource(mip, face, [alive, set, e, face, size](const TexturePixels& pixels)
					{
						if (alive.expired())
							return;
						std::vector<glm::vec3>& target = set->Cubes[e][face];
						if (pixels.Data.empty() || pixels.Width != size || pixels.Height != size)
							++set->Empty;
						else
						{
							target.reserve(static_cast<size_t>(size) * size);
							for (uint32_t y = 0; y < size; ++y)
							{
								for (uint32_t x = 0; x < size; ++x)
									target.push_back(glm::vec3(pixels.GetPixel(x, y)));
							}
						}
						if (--set->Remaining == 0)
							set->Done(*set);
					});
				}
			}
		}

		glm::vec3 TexelDirection(uint32_t face, uint32_t x, uint32_t y, uint32_t size)
		{
			const float s = static_cast<float>(size);
			return glm::normalize(GlslCube::DE_CubeFaceDirection(face, glm::vec2((static_cast<float>(x) + 0.5f) / s, (static_cast<float>(y) + 0.5f) / s)));
		}

		uint32_t FaceLooking(const glm::vec3& direction)
		{
			uint32_t best = 0;
			float bestDot = -2.0f;
			for (uint32_t face = 0; face < 6; ++face)
			{
				const float d = glm::dot(glm::normalize(GlslCube::DE_CubeFaceDirection(face, glm::vec2(0.5f))), direction);
				if (d > bestDot)
				{
					bestDot = d;
					best = face;
				}
			}
			return best;
		}

		double ClosedFormDepth(double rayleigh, double mieExtinction, double ozone)
		{
			const double depthKm = GlslAtmosphere::DE_ATMOSPHERE_TOP - GlslAtmosphere::DE_PLANET_RADIUS;
			return rayleigh * GlslAtmosphere::DE_RAYLEIGH_HEIGHT * (1.0 - std::exp(-depthKm / GlslAtmosphere::DE_RAYLEIGH_HEIGHT))
				+ mieExtinction * GlslAtmosphere::DE_MIE_HEIGHT * (1.0 - std::exp(-depthKm / GlslAtmosphere::DE_MIE_HEIGHT))
				+ ozone * GlslAtmosphere::DE_OZONE_HALF_WIDTH;
		}

		float PixelAngleDegrees(float offsetX, float offsetY, float fov)
		{
			const float tangent = std::sqrt(offsetX * offsetX + offsetY * offsetY) * 2.0f * std::tan(glm::radians(fov * 0.5f)) / static_cast<float>(k_ShotSize);
			return glm::degrees(std::atan(tangent));
		}

		struct StateReads
		{
			std::vector<uint8_t> Bytes[2];
			uint32_t Remaining = 2;
		};
	}

	void ProceduralSkyTest::Initialize()
	{
		m_Checks.clear();
		m_Stage = Stage::Waiting;
		m_Step = 0;
		m_FramesBeforeChecks = 0;
		m_CostPrepared = 0;
		m_Time = 0.0f;
		m_Alive = std::make_shared<int>(0);

		Renderer3DParams params;
		params.Capabilities.Volumetrics = true;
		params.Capabilities.VolumetricGrid = glm::uvec3(32, 18, 16);
		m_Renderer = Renderer3D::Create(params);
		if (m_Renderer)
		{
			m_Diffuse = m_Renderer->CreatePbrMaterial(PbrMaterialParams()
				.SetMaterial(MaterialParams().SetDebugName("ProceduralSkyTest_Diffuse"))
				.SetBaseColor({ 0.6f, 0.6f, 0.6f, 1.0f })
				.SetMetallic(0.0f)
				.SetRoughness(1.0f));
			m_Chrome = m_Renderer->CreatePbrMaterial(PbrMaterialParams()
				.SetMaterial(MaterialParams().SetDebugName("ProceduralSkyTest_Chrome"))
				.SetBaseColor({ 1.0f, 1.0f, 1.0f, 1.0f })
				.SetMetallic(1.0f)
				.SetRoughness(0.05f));
		}

		ProceduralSky sunset = MakeSky(4.0f, 0.0f);
		sunset.SunDiscRadius = 1.5f;
		m_Sunset = MakeEnvironment("ProceduralSkyTest sunset", sunset, 256);
		m_Noon = MakeEnvironment("ProceduralSkyTest noon", MakeSky(70.0f, 30.0f), 128);
		m_Sweep = MakeEnvironment("ProceduralSkyTest sweep", MakeSky(30.0f, 60.0f), 128);
		m_Fogged = MakeEnvironment("ProceduralSkyTest fogged", MakeSky(12.0f, 20.0f), 128);
	}

	Environment* ProceduralSkyTest::MakeEnvironment(const char* name, const ProceduralSky& sky, uint32_t faceSize)
	{
		Environment* environment = Environment::CreateProcedural(ProceduralEnvironmentParams().SetFaceSize(faceSize).SetDebugName(name).SetSky(sky));
		if (environment)
			m_Environments.push_back(environment);
		return environment;
	}

	Framebuffer* ProceduralSkyTest::NewTarget(const char* name)
	{
		Framebuffer* target = Framebuffer::Create(FramebufferParams()
			.SetDebugName(name)
			.SetWidth(static_cast<int32_t>(k_ShotSize))
			.SetHeight(static_cast<int32_t>(k_ShotSize))
			.SetEnableDepth(true)
			.AddAttachment({ TextureFormat::RGBA8_UNORM }));
		m_Targets.push_back(target);
		return target;
	}

	glm::mat4 ProceduralSkyTest::ShotMatrix(const glm::vec3& direction, float aspect, float fov) const
	{
		PerspectiveCamera camera(fov, aspect, 0.1f, 100.0f);
		camera.SetPosition(glm::vec3(0.0f));
		camera.SetUp(std::abs(direction.y) > 0.9f ? glm::vec3(0.0f, 0.0f, -1.0f) : glm::vec3(0.0f, 1.0f, 0.0f));
		camera.SetTarget(direction);
		return camera.GetViewProjectionMatrix();
	}

	Renderer3D::Statistics ProceduralSkyTest::RenderShot(Framebuffer* target, const glm::mat4& viewProjection, const SceneEnvironment& sky, const std::function<void(Renderer3D&)>& submit)
	{
		Framebuffer* previous = Renderer::GetRenderTarget();
		Renderer::SetRenderTarget(target);
		m_Renderer->BeginScene(viewProjection);
		m_Renderer->Clear(k_Clear);
		if (submit)
			submit(*m_Renderer);
		m_Renderer->SetEnvironment(sky);
		m_Renderer->EndScene();
		Renderer::SetRenderTarget(previous);
		return m_Renderer->GetStatistics();
	}

	void ProceduralSkyTest::ReadTargets(std::vector<Framebuffer*> targets, std::function<void(const std::vector<TexturePixels>&)> done)
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

	void ProceduralSkyTest::SubmitLight(Renderer3D& renderer, const glm::vec3& travel, bool shadowed)
	{
		DirectionalLight light;
		light.Direction = travel;
		light.Intensity = 1.0f;
		light.CastShadows = shadowed;
		renderer.SubmitLight(light);
		renderer.SetAmbientLight(glm::vec3(0.0f), 0.0f);
	}

	void ProceduralSkyTest::SubmitSpheres(Renderer3D& renderer, bool both)
	{
		auto sphere = [](const glm::vec3& center, float scale) { return glm::scale(glm::translate(glm::mat4(1.0f), center), glm::vec3(scale)); };
		if (both)
		{
			renderer.SubmitMesh(renderer.GetSphereMesh(), sphere({ -1.1f, -0.2f, -4.0f }, 1.6f), glm::vec4(1.0f), m_Diffuse);
			renderer.SubmitMesh(renderer.GetSphereMesh(), sphere({ 1.1f, -0.2f, -4.0f }, 1.6f), glm::vec4(1.0f), m_Chrome);
		}
		else
			renderer.SubmitMesh(renderer.GetSphereMesh(), sphere({ 0.0f, 0.0f, -4.0f }, 3.0f), glm::vec4(1.0f), m_Diffuse);
	}

	void ProceduralSkyTest::RunChecks()
	{
		if (!m_Renderer || !m_Diffuse || !m_Chrome || !m_Sunset || !m_Noon || !m_Sweep || !m_Fogged)
		{
			Check(false, "Procedural Sky 0: the renderer, the materials and the procedural environments are created");
			return;
		}
		CheckRejections();
		CheckLut();
		CheckZenith();
		CheckSunPosition();
		CheckDisc();
		CheckHorizon();
		CheckDiscIsolation();
		CheckTint();
		CheckNight();
	}

	void ProceduralSkyTest::CheckRejections()
	{
		ProceduralSky sky = MakeSky(40.0f, 0.0f);
		Environment* environment = MakeEnvironment("ProceduralSkyTest reject", sky, 32);
		if (!environment)
		{
			Check(false, "Procedural Sky 12: a procedural environment is created");
			return;
		}

		ProceduralSky nan = sky;
		nan.SunIntensity = std::numeric_limits<float>::quiet_NaN();
		ProceduralSky negative = sky;
		negative.Turbidity = -1.0f;
		const bool nanRejected = !environment->SetSky(nan);
		const bool negativeRejected = !environment->SetSky(negative);
		const ProceduralSky* kept = environment->GetSky();
		const bool kept1 = kept && kept->SunIntensity == sky.SunIntensity && kept->Turbidity == sky.Turbidity;

		uint32_t created = 0;
		const uint32_t badSizes[] = { 8, 100, 4096, 0 };
		for (uint32_t size : badSizes)
		{
			Environment* bad = Environment::CreateProcedural(ProceduralEnvironmentParams().SetFaceSize(size).SetSky(sky));
			created += bad ? 1 : 0;
			delete bad;
		}
		Environment* badSky = Environment::CreateProcedural(ProceduralEnvironmentParams().SetFaceSize(32).SetSky(nan));
		created += badSky ? 1 : 0;
		delete badSky;

		Check(nanRejected && negativeRejected && kept1 && created == 0,
			std::format("Procedural Sky 12: SetSky keeps the old sky for NaN and negative fields, CreateProcedural returns nullptr for face sizes 8, 100, 4096, 0 and a NaN sky (created {}; expect 5 errors and 1 warning logged)", created));
	}

	void ProceduralSkyTest::CheckLut()
	{
		Environment* environment = MakeEnvironment("ProceduralSkyTest lut", MakeSky(45.0f, 0.0f), 32);
		if (!environment || !environment->Prepare())
		{
			Check(false, "Procedural Sky 1: the sky bakes");
			return;
		}

		const glm::vec3 noon = environment->GetSunTransmittance(Travel(80.0f, 0.0f));
		const glm::vec3 below = environment->GetSunTransmittance(Travel(-3.0f, 0.0f));
		const bool sunOk = noon.x > 0.5f && below.x == 0.0f && below.y == 0.0f && below.z == 0.0f;

		const std::weak_ptr<int> alive = m_Alive;
		Texture* lut = Internal::EnvironmentAccess::GetSkyTransmittance(environment);
		lut->ReadSubresource(0, 0, [this, alive, sunOk](const TexturePixels& pixels)
		{
			if (alive.expired())
				return;
			const double mie = 3.996e-3 + 4.4e-3;
			const double expected[3] = {
				std::exp(-ClosedFormDepth(5.802e-3, mie, 0.65e-3)),
				std::exp(-ClosedFormDepth(13.558e-3, mie, 1.881e-3)),
				std::exp(-ClosedFormDepth(33.1e-3, mie, 0.085e-3)),
			};
			const glm::vec4 got = pixels.GetPixel(0, 0);
			bool close = !pixels.Data.empty();
			for (int i = 0; i < 3; ++i)
				close = close && std::abs(static_cast<double>(got[i]) - expected[i]) <= 0.01 * expected[i];
			Check(close && sunOk,
				std::format("Procedural Sky 1: the transmittance LUT looking straight up from the ground is the closed form within 1 % (got {:.4f} {:.4f} {:.4f}, want {:.4f} {:.4f} {:.4f}), and the sun at -3 degrees has none (sun {})",
					got.x, got.y, got.z, expected[0], expected[1], expected[2], sunOk));
		});
	}

	void ProceduralSkyTest::CheckZenith()
	{
		ProceduralSky sky = MakeSky(90.0f, 0.0f);
		sky.SunDirection = glm::vec3(0.0f, -1.0f, 0.0f);
		sky.SunIntensity = 1.0f;
		sky.Turbidity = 0.0f;
		sky.OzoneScale = 0.0f;
		sky.NightColor = glm::vec3(0.0f);
		Environment* environment = MakeEnvironment("ProceduralSkyTest zenith", sky, 32);
		if (!environment)
		{
			Check(false, "Procedural Sky 2: the zenith environment is created");
			return;
		}
		Internal::EnvironmentAccess::SetMultipleScattering(environment, false);
		if (!environment->Prepare())
		{
			Check(false, "Procedural Sky 2: the sky bakes");
			return;
		}

		const std::weak_ptr<int> alive = m_Alive;
		const uint32_t face = FaceLooking(glm::vec3(0.0f, 1.0f, 0.0f));
		environment->GetCubeMap()->ReadSubresource(0, face, [this, alive](const TexturePixels& pixels)
		{
			if (alive.expired())
				return;
			const double beta[3] = { 5.802e-3, 13.558e-3, 33.1e-3 };
			glm::vec3 got(0.0f);
			for (uint32_t i = 0; i < 4; ++i)
				got += glm::vec3(pixels.GetPixel(15 + (i & 1), 15 + (i >> 1))) * 0.25f;
			bool close = !pixels.Data.empty();
			glm::vec3 want(0.0f);
			for (int c = 0; c < 3; ++c)
			{
				const double tau = beta[c] * 8.0 * (1.0 - std::exp(-12.5));
				want[c] = static_cast<float>(0.375 * (1.0 - std::exp(-2.0 * tau)) * 0.5);
				close = close && std::abs(got[c] - want[c]) <= 0.02f * want[c];
			}
			Check(close, std::format("Procedural Sky 2: single scattering, vertical sun and view: zenith radiance is E P_R (1 - e^-2tau) / 2 within 2 % (got {:.4f} {:.4f} {:.4f}, want {:.4f} {:.4f} {:.4f})",
				got.x, got.y, got.z, want.x, want.y, want.z));
		});
	}

	void ProceduralSkyTest::CheckSunPosition()
	{
		const float elevations[4] = { 60.0f, 45.0f, 35.0f, 50.0f };
		const float azimuths[4] = { 20.0f, 110.0f, 200.0f, 290.0f };
		std::vector<Environment*> environments;
		for (int i = 0; i < 4; ++i)
			environments.push_back(MakeEnvironment("ProceduralSkyTest sun position", MakeSky(elevations[i], azimuths[i]), 32));
		environments.push_back(MakeEnvironment("ProceduralSkyTest turned", MakeSky(elevations[0], azimuths[0] + 90.0f), 32));
		for (Environment* environment : environments)
		{
			if (!environment || !environment->Prepare())
			{
				Check(false, "Procedural Sky 3: the sky bakes");
				return;
			}
		}

		const std::weak_ptr<int> alive = m_Alive;
		ReadCubes(environments, 1, alive, [this, elevations, azimuths](const CubeSet& set)
		{
			constexpr uint32_t size = 16;
			float worst = 0.0f;
			float angles[4] = {};
			for (int i = 0; i < 4; ++i)
			{
				float brightest = -1.0f;
				glm::vec3 direction(0.0f);
				for (uint32_t face = 0; face < 6; ++face)
				{
					const std::vector<glm::vec3>& texels = set.Cubes[i][face];
					for (uint32_t y = 0; y < size && texels.size() == size * size; ++y)
					{
						for (uint32_t x = 0; x < size; ++x)
						{
							const glm::vec3 texel = TexelDirection(face, x, y, size);
							if (std::abs(glm::degrees(std::asin(texel.y)) - elevations[i]) > 5.0f)
								continue;
							const glm::vec3 value = texels[static_cast<size_t>(y) * size + x];
							if (value.r + value.g + value.b > brightest)
							{
								brightest = value.r + value.g + value.b;
								direction = texel;
							}
						}
					}
				}
				const float angle = brightest < 0.0f ? 180.0f : glm::degrees(std::acos(glm::clamp(glm::dot(direction, ToSun(elevations[i], azimuths[i])), -1.0f, 1.0f)));
				angles[i] = angle;
				worst = (std::max)(worst, angle);
			}
			Check(set.Empty == 0 && worst < 9.0f, std::format("Procedural Sky 3: the brightest texel of mip 1 at the sun's own elevation is within 9 degrees of the sun for four suns (off by {:.1f} {:.1f} {:.1f} {:.1f} degrees, {} empty readbacks)", angles[0], angles[1], angles[2], angles[3], set.Empty));

			uint32_t wrong = 0;
			uint32_t total = 0;
			for (uint32_t face = 0; face < 6; ++face)
			{
				const std::vector<glm::vec3>& turned = set.Cubes[4][face];
				if (turned.size() != size * size)
				{
					wrong += size * size;
					continue;
				}
				for (uint32_t y = 0; y < size; ++y)
				{
					for (uint32_t x = 0; x < size; ++x)
					{
						const glm::vec3 d = TexelDirection(face, x, y, size);
						const glm::vec3 source(-d.z, d.y, d.x);
						float best = -2.0f;
						glm::vec3 match(0.0f);
						for (uint32_t f2 = 0; f2 < 6; ++f2)
						{
							const std::vector<glm::vec3>& original = set.Cubes[0][f2];
							for (uint32_t y2 = 0; y2 < size && original.size() == size * size; ++y2)
							{
								for (uint32_t x2 = 0; x2 < size; ++x2)
								{
									const float dot = glm::dot(TexelDirection(f2, x2, y2, size), source);
									if (dot > best)
									{
										best = dot;
										match = original[static_cast<size_t>(y2) * size + x2];
									}
								}
							}
						}
						const glm::vec3 got = turned[static_cast<size_t>(y) * size + x];
						++total;
						for (int c = 0; c < 3; ++c)
						{
							if (std::abs(got[c] - match[c]) > 3e-3f * (std::max)(std::abs(match[c]), std::abs(got[c])) + 2e-4f)
							{
								++wrong;
								break;
							}
						}
					}
				}
			}
			Check(wrong == 0, std::format("Procedural Sky 3: a sun turned 90 degrees about +Y gives the cube with its texels permuted ({} of {} texels differ)", wrong, total));
		});
	}

	void ProceduralSkyTest::CheckDisc()
	{
		constexpr float fov = 8.0f;
		const float limbOffset = 5.5f;
		const float limbAngle = PixelAngleDegrees(limbOffset, 0.5f, fov);
		const float radius = limbAngle / 0.7f;

		ProceduralSky withDisc = MakeSky(20.0f, 0.0f);
		withDisc.SunIntensity = 1.0f;
		withDisc.SunDiscIntensity = 0.3f;
		withDisc.SunDiscRadius = radius;
		ProceduralSky noDisc = withDisc;
		noDisc.SunDiscRadius = 0.0f;
		Environment* disc = MakeEnvironment("ProceduralSkyTest disc", withDisc, 32);
		Environment* plain = MakeEnvironment("ProceduralSkyTest no disc", noDisc, 32);
		if (!disc || !plain)
		{
			Check(false, "Procedural Sky 6: the disc environments are created");
			return;
		}

		SceneEnvironment sky;
		sky.FollowSun = false;
		sky.TintSun = false;
		sky.Lighting = false;
		const glm::mat4 view = ShotMatrix(ToSun(20.0f, 0.0f), 1.0f, fov);
		Framebuffer* first = NewTarget("ProceduralSkyTest disc");
		Framebuffer* second = NewTarget("ProceduralSkyTest no disc");
		sky.Environment = disc;
		RenderShot(first, view, sky);
		sky.Environment = plain;
		RenderShot(second, view, sky);

		const glm::vec3 transmittance = disc->GetSunTransmittance(Travel(20.0f, 0.0f));
		ReadTargets({ first, second }, [this, transmittance, radius, withDisc](const std::vector<TexturePixels>& images)
		{
			if (images[0].Data.empty() || images[1].Data.empty())
			{
				Check(false, "Procedural Sky 6: the disc shots read back");
				return;
			}
			const float sinRadius = std::sin(glm::radians(radius));
			auto expected = [&](uint32_t x, uint32_t y)
			{
				const float ox = static_cast<float>(x) + 0.5f - 16.0f;
				const float oy = static_cast<float>(y) + 0.5f - 16.0f;
				const float theta = PixelAngleDegrees(ox, oy, 8.0f);
				glm::vec3 linear = SrgbDecode(Pixel(images[1], x, y));
				if (theta < radius)
				{
					const float sinTheta = std::sin(glm::radians(theta));
					linear += SrgbDecode(withDisc.SunColor) * (withDisc.SunIntensity * withDisc.SunDiscIntensity) * transmittance * GlslAtmosphere::DE_SunLimb((sinTheta * sinTheta) / (sinRadius * sinRadius));
				}
				return glm::min(SrgbEncode(linear), glm::vec3(1.0f));
			};
			const float centre = MaxDifference(Pixel(images[0], 16, 16), expected(16, 16));
			const float limb = MaxDifference(Pixel(images[0], 21, 16), expected(21, 16));
			const float outside = MaxDifference(Pixel(images[0], 28, 16), Pixel(images[1], 28, 16));
			const float lift = MaxDifference(Pixel(images[0], 16, 16), Pixel(images[1], 16, 16));
			Check(centre < 2.0f / 255.0f && lift > 0.02f, std::format("Procedural Sky 6: the disc's centre is the encoded sum of the cube and the disc (off by {:.4f}, lifted by {:.3f})", centre, lift));
			Check(limb < 0.02f, std::format("Procedural Sky 6: at 0.7 of the disc's radius the disc follows the limb darkening (off by {:.4f})", limb));
			Check(outside < 1.0f / 255.0f, std::format("Procedural Sky 6: at 1.5 of the disc's radius the sky is the cube alone (off by {:.4f})", outside));
		});
	}

	void ProceduralSkyTest::CheckHorizon()
	{
		ProceduralSky withDisc = MakeSky(-3.0f, 0.0f);
		withDisc.SunDiscRadius = 0.5f;
		ProceduralSky noDisc = withDisc;
		noDisc.SunDiscRadius = 0.0f;
		Environment* disc = MakeEnvironment("ProceduralSkyTest horizon disc", withDisc, 32);
		Environment* plain = MakeEnvironment("ProceduralSkyTest horizon plain", noDisc, 32);
		if (!disc || !plain)
		{
			Check(false, "Procedural Sky 7: the horizon environments are created");
			return;
		}

		SceneEnvironment sky;
		sky.FollowSun = false;
		sky.TintSun = false;
		sky.Lighting = false;
		const glm::mat4 view = ShotMatrix(ToSun(-3.0f, 0.0f));
		Framebuffer* first = NewTarget("ProceduralSkyTest horizon disc");
		Framebuffer* second = NewTarget("ProceduralSkyTest horizon plain");
		sky.Environment = disc;
		RenderShot(first, view, sky);
		sky.Environment = plain;
		RenderShot(second, view, sky);

		SceneEnvironment lit;
		lit.Environment = disc;
		lit.Lighting = false;
		lit.DrawSkybox = false;
		const glm::mat4 front = ShotMatrix(glm::vec3(0.0f, 0.0f, -1.0f));
		Framebuffer* tinted = NewTarget("ProceduralSkyTest below tinted");
		Framebuffer* untinted = NewTarget("ProceduralSkyTest below untinted");
		const glm::vec3 travel = Travel(-3.0f, 0.0f);
		auto submit = [this, travel](Renderer3D& renderer)
		{
			SubmitLight(renderer, travel);
			SubmitSpheres(renderer, false);
		};
		lit.TintSun = true;
		RenderShot(tinted, front, lit, submit);
		lit.TintSun = false;
		RenderShot(untinted, front, lit, submit);

		ReadTargets({ first, second, tinted, untinted }, [this](const std::vector<TexturePixels>& images)
		{
			uint32_t differing = 0;
			const bool same = SameImage(images[0], images[1], differing);
			Check(same, std::format("Procedural Sky 7: a sun at -3 degrees draws no disc ({} of {} pixels differ)", differing, k_ShotSize * k_ShotSize));
			const glm::vec3 dark = images[2].Data.empty() ? glm::vec3(1.0f) : Pixel(images[2], 16, 16);
			const glm::vec3 bright = images[3].Data.empty() ? glm::vec3(0.0f) : Pixel(images[3], 16, 16);
			Check(MaxDifference(dark, glm::vec3(0.0f)) <= 1.0f / 255.0f && bright.g > 0.1f,
				std::format("Procedural Sky 7: a sphere lit only by the tinted light reads black ({:.4f}) where the untinted light lights it ({:.3f})", dark.g, bright.g));
		});
	}

	void ProceduralSkyTest::CheckDiscIsolation()
	{
		ProceduralSky dark = MakeSky(30.0f, 0.0f);
		dark.SunDiscIntensity = 0.0f;
		ProceduralSky blinding = dark;
		blinding.SunDiscIntensity = 1000.0f;
		Environment* first = MakeEnvironment("ProceduralSkyTest disc 0", dark, 32);
		Environment* second = MakeEnvironment("ProceduralSkyTest disc 1000", blinding, 32);
		if (!first || !second)
		{
			Check(false, "Procedural Sky 8: the disc intensity environments are created");
			return;
		}

		const glm::mat4 view = ShotMatrix(glm::vec3(0.0f, 0.0f, -1.0f));
		const glm::vec3 travel = Travel(30.0f, 0.0f);
		auto submit = [this, travel](Renderer3D& renderer)
		{
			SubmitLight(renderer, travel);
			SubmitSpheres(renderer, true);
		};
		Framebuffer* shots[2] = { NewTarget("ProceduralSkyTest disc 0"), NewTarget("ProceduralSkyTest disc 1000") };
		SceneEnvironment sky;
		sky.Environment = first;
		RenderShot(shots[0], view, sky, submit);
		sky.Environment = second;
		RenderShot(shots[1], view, sky, submit);

		const std::weak_ptr<int> alive = m_Alive;
		auto reads = std::make_shared<StateReads>();
		Environment* environments[2] = { first, second };
		for (int i = 0; i < 2; ++i)
		{
			Internal::EnvironmentAccess::GetIrradiance(environments[i])->ReadBack([this, alive, reads, i](const std::vector<uint8_t>& bytes)
			{
				if (alive.expired())
					return;
				reads->Bytes[i] = bytes;
				if (--reads->Remaining == 0)
					Check(!reads->Bytes[0].empty() && reads->Bytes[0] == reads->Bytes[1], std::format("Procedural Sky 8: the spherical harmonics baked with a disc intensity of 0 and of 1000 are identical ({} bytes)", reads->Bytes[0].size()));
			});
		}
		ReadTargets({ shots[0], shots[1] }, [this](const std::vector<TexturePixels>& images)
		{
			uint32_t differing = 0;
			const bool same = SameImage(images[0], images[1], differing);
			Check(same, std::format("Procedural Sky 8: a diffuse and a chrome sphere draw the same with a disc intensity of 0 and of 1000 ({} of {} pixels differ)", differing, k_ShotSize * k_ShotSize));
		});
	}

	void ProceduralSkyTest::CheckTint()
	{
		Environment* environment = MakeEnvironment("ProceduralSkyTest tint", MakeSky(15.0f, 0.0f), 32);
		if (!environment)
		{
			Check(false, "Procedural Sky 9: the tint environment is created");
			return;
		}

		const glm::vec3 travel = Travel(15.0f, 0.0f);
		const glm::mat4 view = ShotMatrix(glm::vec3(0.0f, 0.0f, -1.0f));
		SceneEnvironment sky;
		sky.Environment = environment;
		sky.Lighting = false;
		sky.DrawSkybox = false;
		auto submit = [this, travel](Renderer3D& renderer)
		{
			SubmitLight(renderer, travel);
			SubmitSpheres(renderer, false);
		};
		Framebuffer* tinted = NewTarget("ProceduralSkyTest tinted");
		Framebuffer* untinted = NewTarget("ProceduralSkyTest untinted");
		sky.TintSun = true;
		RenderShot(tinted, view, sky, submit);
		sky.TintSun = false;
		RenderShot(untinted, view, sky, submit);

		const glm::vec3 transmittance = environment->GetSunTransmittance(travel);
		ReadTargets({ tinted, untinted }, [this, transmittance](const std::vector<TexturePixels>& images)
		{
			if (images[0].Data.empty() || images[1].Data.empty())
			{
				Check(false, "Procedural Sky 9: the tint shots read back");
				return;
			}
			const glm::vec3 got = Pixel(images[0], 16, 16);
			const glm::vec3 want = SrgbEncode(SrgbDecode(Pixel(images[1], 16, 16)) * transmittance);
			const float error = MaxDifference(got, want);
			Check(error < 1.5f / 255.0f && MaxDifference(got, Pixel(images[1], 16, 16)) > 0.02f,
				std::format("Procedural Sky 9: TintSun scales a lit sphere by the transmittance {:.3f} {:.3f} {:.3f} (off by {:.4f})", transmittance.x, transmittance.y, transmittance.z, error));
		});

		CheckFogTint(environment, travel, transmittance);
	}

	void ProceduralSkyTest::CheckFogTint(Environment* environment, const glm::vec3& travel, const glm::vec3& transmittance)
	{
		if (!m_Renderer->GetVolumetricGrid().x)
		{
			Check(false, "Procedural Sky 9: the renderer has a froxel volume");
			return;
		}

		VolumetricFog fog;
		fog.Density = 0.05f;
		fog.Temporal = false;
		fog.AmbientIntensity = 0.0f;
		SceneEnvironment sky;
		sky.Environment = environment;
		sky.Lighting = false;
		sky.DrawSkybox = false;
		const glm::mat4 view = ShotMatrix(glm::vec3(0.0f, 0.0f, -1.0f));
		auto submit = [this, travel, fog](Renderer3D& renderer)
		{
			SubmitLight(renderer, travel);
			renderer.SetVolumetricFog(fog);
			SubmitSpheres(renderer, false);
		};

		const std::weak_ptr<int> alive = m_Alive;
		auto scatter = std::make_shared<std::array<glm::vec3, 2>>();
		auto remaining = std::make_shared<int>(2);
		Framebuffer* target = NewTarget("ProceduralSkyTest fog");
		for (int i = 0; i < 2; ++i)
		{
			sky.TintSun = i == 0;
			RenderShot(target, view, sky, submit);
			Texture* volume = Internal::Renderer3DAccess::VolumetricScatter(*m_Renderer);
			if (!volume)
			{
				Check(false, "Procedural Sky 9: the froxel scatter volume exists");
				return;
			}
			volume->ReadSubresource(0, 8, [this, alive, scatter, remaining, i, transmittance](const TexturePixels& pixels)
			{
				if (alive.expired())
					return;
				(*scatter)[i] = pixels.Data.empty() ? glm::vec3(0.0f) : glm::vec3(pixels.GetPixel(16, 9));
				if (--*remaining != 0)
					return;
				const glm::vec3 want = SrgbEncode(transmittance);
				float worst = 0.0f;
				for (int c = 0; c < 3; ++c)
				{
					const float ratio = (*scatter)[1][c] > 0.0f ? (*scatter)[0][c] / (*scatter)[1][c] : 0.0f;
					worst = (std::max)(worst, std::abs(ratio - want[c]) / want[c]);
				}
				Check(worst < 0.02f && (*scatter)[1].g > 0.0f,
					std::format("Procedural Sky 9: a froxel's in-scatter is scaled by the encoded transmittance within 2 % (off by {:.4f}, untinted {:.5f})", worst, (*scatter)[1].g));
			});
		}
	}

	void ProceduralSkyTest::CheckNight()
	{
		ProceduralSky sky = MakeSky(-20.0f, 0.0f);
		Environment* environment = MakeEnvironment("ProceduralSkyTest night", sky, 32);
		if (!environment || !environment->Prepare())
		{
			Check(false, "Procedural Sky 10: the night sky bakes");
			return;
		}

		const glm::vec3 night = sky.NightColor;
		ReadCubes({ environment }, 1, m_Alive, [this, night](const CubeSet& set)
		{
			constexpr uint32_t size = 16;
			glm::vec3 sum(0.0f);
			uint32_t count = 0;
			for (uint32_t face = 0; face < 6; ++face)
			{
				const std::vector<glm::vec3>& texels = set.Cubes[0][face];
				for (uint32_t y = 0; y < size && texels.size() == size * size; ++y)
				{
					for (uint32_t x = 0; x < size; ++x)
					{
						if (TexelDirection(face, x, y, size).y > 0.05f)
						{
							sum += texels[static_cast<size_t>(y) * size + x];
							++count;
						}
					}
				}
			}
			const glm::vec3 mean = count ? sum / static_cast<float>(count) : glm::vec3(-1.0f);
			bool close = count > 0;
			for (int c = 0; c < 3; ++c)
				close = close && std::abs(mean[c] - night[c]) <= 0.1f * night[c] + 1e-4f;
			Check(close, std::format("Procedural Sky 10: a sun at -20 degrees leaves the upper hemisphere at NightColor (mean {:.5f} {:.5f} {:.5f}, want {:.5f} {:.5f} {:.5f})", mean.x, mean.y, mean.z, night.x, night.y, night.z));
		});
	}

	void ProceduralSkyTest::StepRebake()
	{
		if (!m_Rebake)
		{
			ProceduralSky sky = MakeSky(35.0f, 20.0f);
			sky.RebakeAngle = 0.25f;
			m_Rebake = MakeEnvironment("ProceduralSkyTest rebake", sky, 32);
			if (!m_Rebake)
			{
				Check(false, "Procedural Sky 4: the re-bake environment is created");
				m_Stage = Stage::Follow;
				m_Step = 0;
				return;
			}
			m_RebakeTarget = NewTarget("ProceduralSkyTest rebake");
		}

		const glm::vec3 baked = Travel(35.0f, 20.0f);
		const glm::mat4 view = ShotMatrix(glm::vec3(0.0f, 0.0f, -1.0f));
		SceneEnvironment sky;
		sky.Environment = m_Rebake;
		auto scene = [this](const glm::vec3& travel)
		{
			return [this, travel](Renderer3D& renderer)
			{
				SubmitLight(renderer, travel);
				SubmitSpheres(renderer, false);
			};
		};

		// DrawView leaves another environment as the renderer's lighting source, which would count as a copy.
		auto prime = [&](const glm::vec3& travel) { RenderShot(m_RebakeTarget, view, sky, scene(travel)); };

		switch (m_Step)
		{
		case 0:
		{
			const Renderer3D::Statistics stats = RenderShot(m_RebakeTarget, view, sky, scene(baked));
			Check(stats.EnvironmentBakes == 1 && stats.EnvironmentLightingCopies == 1,
				std::format("Procedural Sky 4: the first scene bakes the sky once and copies its lighting once (bakes {}, copies {})", stats.EnvironmentBakes, stats.EnvironmentLightingCopies));
			break;
		}
		case 1:
		{
			prime(baked);
			const Renderer3D::Statistics stats = RenderShot(m_RebakeTarget, view, sky, scene(Tilt(baked, 0.1f)));
			Check(stats.EnvironmentBakes == 0 && stats.EnvironmentLightingCopies == 0,
				std::format("Procedural Sky 4: a sun 0.1 degrees away does not re-bake (bakes {}, copies {})", stats.EnvironmentBakes, stats.EnvironmentLightingCopies));
			break;
		}
		case 2:
		{
			prime(baked);
			m_RebakeSun = Tilt(baked, 0.5f);
			const Renderer3D::Statistics stats = RenderShot(m_RebakeTarget, view, sky, scene(m_RebakeSun));
			Check(stats.EnvironmentBakes == 1 && stats.EnvironmentLightingCopies == 1,
				std::format("Procedural Sky 4: a sun 0.5 degrees away re-bakes once and copies the lighting once (bakes {}, copies {})", stats.EnvironmentBakes, stats.EnvironmentLightingCopies));
			break;
		}
		case 3:
		{
			prime(m_RebakeSun);
			ProceduralSky changed = *m_Rebake->GetSky();
			changed.Turbidity = 2.0f;
			const bool accepted = m_Rebake->SetSky(changed);
			const Renderer3D::Statistics stats = RenderShot(m_RebakeTarget, view, sky, scene(m_RebakeSun));
			Check(accepted && stats.EnvironmentBakes == 1 && stats.EnvironmentLightingCopies == 1,
				std::format("Procedural Sky 4: a change other than the sun re-bakes (accepted {}, bakes {}, copies {})", accepted, stats.EnvironmentBakes, stats.EnvironmentLightingCopies));
			break;
		}
		case 4:
		{
			const glm::vec3 moved = Tilt(m_RebakeSun, 1.0f);
			const Renderer3D::Statistics first = RenderShot(m_RebakeTarget, view, sky, scene(moved));
			const Renderer3D::Statistics second = RenderShot(m_RebakeTarget, view, sky, scene(moved));
			Check(first.EnvironmentBakes + second.EnvironmentBakes == 1,
				std::format("Procedural Sky 4: two scenes of one frame that follow a moved sun bake once (bakes {} and {})", first.EnvironmentBakes, second.EnvironmentBakes));
			m_Stage = Stage::Follow;
			m_Step = 0;
			return;
		}
		}
		++m_Step;
	}

	void ProceduralSkyTest::StepFollow()
	{
		if (!m_FollowEnvironment)
		{
			m_FollowEnvironment = MakeEnvironment("ProceduralSkyTest follow", MakeSky(70.0f, 0.0f), 32);
			if (!m_FollowEnvironment)
			{
				Check(false, "Procedural Sky 5: the follow environment is created");
				m_Stage = Stage::Scene;
				m_Step = 0;
				return;
			}
			m_FollowTarget = NewTarget("ProceduralSkyTest follow");
		}

		const glm::vec3 a = Travel(25.0f, 100.0f);
		const glm::vec3 b = Travel(40.0f, 250.0f);
		SceneEnvironment sky;
		sky.Environment = m_FollowEnvironment;
		sky.TintSun = false;
		const bool shadowedSecond = m_Step == 0;
		RenderShot(m_FollowTarget, ShotMatrix(glm::vec3(0.0f, 0.0f, -1.0f)), sky, [this, a, b, shadowedSecond](Renderer3D& renderer)
		{
			SubmitLight(renderer, a);
			DirectionalLight second;
			second.Direction = b;
			second.CastShadows = shadowedSecond;
			renderer.SubmitLight(second);
		});

		const ProceduralSky* current = m_FollowEnvironment->GetSky();
		const glm::vec3 want = shadowedSecond ? glm::normalize(b) : glm::normalize(a);
		const bool ok = current && glm::dot(glm::normalize(current->SunDirection), want) > 0.9999f;
		Check(ok, std::format("Procedural Sky 5: FollowSun follows {}", shadowedSecond ? "the light that casts shadows" : "the first light when none casts shadows"));

		if (++m_Step == 2)
		{
			m_Stage = Stage::Scene;
			m_Step = 0;
		}
	}

	void ProceduralSkyTest::StepScene()
	{
		if (!m_SceneDirect)
		{
			m_SceneDirect = MakeEnvironment("ProceduralSkyTest scene direct", MakeSky(30.0f, 60.0f), 32);
			m_SceneSky = MakeEnvironment("ProceduralSkyTest scene component", MakeSky(30.0f, 60.0f), 32);
			if (!m_SceneDirect || !m_SceneSky)
			{
				Check(false, "Procedural Sky 11: the Scene path environments are created");
				m_Stage = Stage::Cost;
				m_Step = 0;
				return;
			}
			m_SceneDirectTarget = NewTarget("ProceduralSkyTest scene direct");
			m_SceneSkyTarget = NewTarget("ProceduralSkyTest scene component");
		}

		const glm::vec3 travel = Travel(30.0f, 60.0f);
		const glm::mat4 view = ShotMatrix(glm::vec3(0.0f, 0.0f, -1.0f));
		auto shot = [&](Framebuffer* target, const glm::vec3& direction, Environment* environment, bool component)
		{
			Framebuffer* previous = Renderer::GetRenderTarget();
			Renderer::SetRenderTarget(target);
			m_Renderer->BeginScene(view);
			m_Renderer->Clear(k_Clear);
			Scene scene("Procedural Sky Check");
			Entity sphere = scene.CreateEntity("Sphere");
			sphere.AddComponent<Transform3DComponent>(Transform3DComponent(glm::vec3(0.0f, 0.0f, -4.0f), glm::vec3(3.0f)));
			sphere.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_Renderer->GetSphereMesh())).Material = m_Diffuse;
			DirectionalLightComponent& light = scene.CreateEntity("Sun").AddComponent<DirectionalLightComponent>();
			light.Direction = direction;
			light.CastShadows = true;
			if (component)
				scene.CreateEntity("Sky").AddComponent<SkyboxComponent>(SkyboxComponent(environment));
			scene.SubmitLights(*m_Renderer);
			scene.RenderEntities3D(*m_Renderer);
			if (!component)
			{
				SceneEnvironment direct;
				direct.Environment = environment;
				m_Renderer->SetEnvironment(direct);
			}
			m_Renderer->EndScene();
			Renderer::SetRenderTarget(previous);
			return m_Renderer->GetStatistics();
		};

		switch (m_Step)
		{
		case 0:
		{
			shot(m_SceneDirectTarget, travel, m_SceneDirect, false);
			const Renderer3D::Statistics stats = shot(m_SceneSkyTarget, travel, m_SceneSky, true);
			ReadTargets({ m_SceneDirectTarget, m_SceneSkyTarget }, [this, stats](const std::vector<TexturePixels>& images)
			{
				if (images[0].Data.empty() || images[1].Data.empty() || images[0].Width != images[1].Width || images[0].Height != images[1].Height)
				{
					Check(false, "Procedural Sky 11: the Scene path shots read back");
					return;
				}
				uint32_t differing = 0;
				for (uint32_t y = 0; y < images[0].Height; ++y)
				{
					for (uint32_t x = 0; x < images[0].Width; ++x)
						differing += MaxDifference(Pixel(images[0], x, y), Pixel(images[1], x, y)) <= 1.5f / 255.0f ? 0 : 1;
				}
				const glm::vec3 corner = Pixel(images[1], 2, 2);
				Check(differing == 0 && stats.Skybox && stats.EnvironmentLighting && corner.g > 0.02f,
					std::format("Procedural Sky 11: a SkyboxComponent with FollowSun and TintSun draws as SetEnvironment ({} px differ, sky {}, lighting {}, corner green {:.3f})", differing, stats.Skybox, stats.EnvironmentLighting, corner.g));
			});
			break;
		}
		case 1:
		{
			const Renderer3D::Statistics stats = shot(m_SceneSkyTarget, Tilt(travel, 2.0f), m_SceneSky, true);
			m_SceneBakes = stats.EnvironmentBakes;
			break;
		}
		case 2:
		{
			const Renderer3D::Statistics stats = shot(m_SceneSkyTarget, Tilt(travel, 2.0f), m_SceneSky, true);
			Check(m_SceneBakes == 1 && stats.EnvironmentBakes == 0,
				std::format("Procedural Sky 11: moving the light's Direction re-bakes once (bakes {}, then {})", m_SceneBakes, stats.EnvironmentBakes));
			m_Stage = Stage::Cost;
			m_Step = 0;
			return;
		}
		}
		++m_Step;
	}

	void ProceduralSkyTest::StepCost()
	{
		if (!m_Cost)
		{
			m_Cost = MakeEnvironment("ProceduralSkyTest cost", MakeSky(30.0f, 0.0f), 512);
			if (!m_Cost)
			{
				Check(false, "Procedural Sky 13: the 512 texel environment is created");
				m_Stage = Stage::Done;
				return;
			}
		}

		if (m_CostPrepared < k_CostFrames)
		{
			ProceduralSky sky = *m_Cost->GetSky();
			sky.SunDirection = Travel(10.0f + 2.0f * static_cast<float>(m_CostPrepared), 0.0f);
			m_Cost->SetSky(sky);
			m_Cost->Prepare();
			++m_CostPrepared;
			return;
		}

		if (++m_Step < 10)
			return;
		m_Stage = Stage::Done;
		for (const GpuTimerStats& timer : Renderer::GetGpuTimers())
		{
			if (timer.Name && std::string_view(timer.Name) == "Environment" && timer.Samples > 0)
			{
				DE_INFO("Procedural sky cost: Environment {:.3f} ms mean, {:.3f} ms last, {:.3f} ms max over {} samples (30 re-bakes of a 512 texel face)", timer.MeanMs, timer.LastMs, timer.MaxMs, timer.Samples);
				Check(true, std::format("Procedural Sky 13: the Environment GPU timer over 30 re-bakes at 512: {:.3f} ms mean, {:.3f} ms max ({} samples)", timer.MeanMs, timer.MaxMs, timer.Samples));
				return;
			}
		}
		DE_INFO("Procedural sky cost: the Environment GPU timer has no samples on this backend.");
		Check(true, "Procedural Sky 13: the Environment GPU timer has no samples on this backend");
	}

	void ProceduralSkyTest::Update(float deltaTime)
	{
		m_Time += deltaTime;
		if (!Renderer::IsFrameSkipped())
		{
			if (m_Stage == Stage::Waiting && ++m_FramesBeforeChecks > k_FramesBeforeChecks)
			{
				RunChecks();
				m_Stage = Stage::Rebake;
				m_Step = 0;
			}
			else if (m_Stage == Stage::Rebake)
				StepRebake();
			else if (m_Stage == Stage::Follow)
				StepFollow();
			else if (m_Stage == Stage::Scene)
				StepScene();
			else if (m_Stage == Stage::Cost)
				StepCost();
		}

		Renderer2D& renderer2D = Application::Get().GetRenderer2D();
		renderer2D.BeginScene(glm::ortho(-m_AspectRatio, m_AspectRatio, -1.0f, 1.0f, -1.0f, 1.0f));
		renderer2D.Clear(m_ClearColor);
		renderer2D.EndScene();
		DrawView();
	}

	void ProceduralSkyTest::DrawView()
	{
		if (!m_Renderer || !m_Diffuse || !m_Chrome || !m_Sunset || !m_Noon || !m_Sweep || !m_Fogged)
			return;

		Framebuffer* output = Renderer::GetRenderTarget();
		const float halfWidth = static_cast<float>(output ? output->GetWidth() : m_Width) * 0.5f;
		const float halfHeight = static_cast<float>(output ? output->GetHeight() : m_Height) * 0.5f;

		{
			const Viewport viewport{ 0.0f, 0.0f, halfWidth, halfHeight };
			SceneEnvironment sky;
			sky.Environment = m_Sunset;
			sky.FollowSun = false;
			sky.TintSun = false;
			PostProcessSettings settings;
			settings.Enabled = true;
			settings.Tone.Operator = ToneMapOperator::Neutral;
			settings.Bloom.Enabled = true;
			Renderer::SetViewport(viewport);
			PostProcessStack& post = Renderer::GetPostProcessStack();
			post.Begin(settings);
			m_Renderer->BeginScene(ShotMatrix(glm::normalize(glm::vec3(0.0f, 0.12f, 1.0f)), viewport.Width / viewport.Height));
			m_Renderer->Clear(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
			m_Renderer->SetEnvironment(sky);
			m_Renderer->EndScene();
			post.End();
		}

		{
			SceneEnvironment sky;
			sky.Environment = m_Noon;
			sky.FollowSun = false;
			sky.TintSun = false;
			const glm::vec3 axes[6] = { { 0, 0, -1 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 } };
			const float cellWidth = halfWidth / 3.0f;
			for (int i = 0; i < 6; ++i)
			{
				const Viewport cell{ halfWidth + static_cast<float>(i % 3) * cellWidth, static_cast<float>(i / 3) * halfHeight * 0.5f, cellWidth, halfHeight * 0.5f };
				Renderer::SetViewport(cell);
				m_Renderer->BeginScene(ShotMatrix(axes[i], cell.Width / cell.Height));
				m_Renderer->SetEnvironment(sky);
				m_Renderer->EndScene();
			}
		}

		{
			const Viewport viewport{ 0.0f, halfHeight, halfWidth, halfHeight };
			const float elevation = 30.0f + 40.0f * std::sin(m_Time * 0.25f);
			const glm::vec3 travel = Travel(elevation, 60.0f);
			SceneEnvironment sky;
			sky.Environment = m_Sweep;
			Renderer::SetViewport(viewport);
			m_Renderer->BeginScene(ShotMatrix(glm::normalize(glm::vec3(0.4f, 0.1f, -1.0f)), viewport.Width / viewport.Height));
			SubmitLight(*m_Renderer, travel);
			SubmitSpheres(*m_Renderer, true);
			m_Renderer->SetEnvironment(sky);
			m_Renderer->EndScene();
		}

		{
			const Viewport viewport{ halfWidth, halfHeight, halfWidth, halfHeight };
			VolumetricFog fog;
			fog.Density = 0.03f;
			fog.Temporal = false;
			SceneEnvironment sky;
			sky.Environment = m_Fogged;
			Renderer::SetViewport(viewport);
			m_Renderer->BeginScene(ShotMatrix(glm::normalize(glm::vec3(0.0f, 0.08f, -1.0f)), viewport.Width / viewport.Height));
			SubmitLight(*m_Renderer, Travel(12.0f, 20.0f));
			m_Renderer->SetVolumetricFog(fog);
			SubmitSpheres(*m_Renderer, true);
			m_Renderer->SetEnvironment(sky);
			m_Renderer->EndScene();
		}
		Renderer::ResetViewport();
	}

	void ProceduralSkyTest::Cleanup()
	{
		m_Alive.reset();
		for (Framebuffer*& target : m_Targets)
			DestroyAndDelete(target);
		m_Targets.clear();
		DestroyAndDelete(m_Diffuse);
		DestroyAndDelete(m_Chrome);
		if (m_Renderer)
		{
			m_Renderer->Shutdown();
			delete m_Renderer;
			m_Renderer = nullptr;
		}
		for (Environment* environment : m_Environments)
			delete environment;
		m_Environments.clear();
		m_Rebake = m_FollowEnvironment = m_Cost = m_SceneDirect = m_SceneSky = m_Sunset = m_Noon = m_Sweep = m_Fogged = nullptr;
		m_RebakeTarget = m_FollowTarget = m_SceneDirectTarget = m_SceneSkyTarget = nullptr;
		m_Checks.clear();
	}

	void ProceduralSkyTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
		m_Width = width;
		m_Height = height;
	}

	void ProceduralSkyTest::ImGuiRender()
	{
		ImGui::TextWrapped("Top left: a sunset with the disc and bloom. Top right: the six faces at noon. Bottom left: a sun sweeping the sky with its light. Bottom right: chrome and diffuse spheres under a tinted sun in froxel fog.");
		GraphicsTest::ImGuiRender();

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

}
