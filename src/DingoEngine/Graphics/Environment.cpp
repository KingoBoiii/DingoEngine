#include "depch.h"
#include "DingoEngine/Graphics/Environment.h"
#include "DingoEngine/Asset/AssetPath.h"
#include "DingoEngine/Core/FileReaders.h"
#include "DingoEngine/Core/FileSystem.h"
#include "DingoEngine/Core/PathText.h"
#include "DingoEngine/Graphics/ComputePass.h"
#include "DingoEngine/Graphics/EnvironmentAccess.h"
#include "DingoEngine/Graphics/EnvironmentLayout.h"
#include "DingoEngine/Graphics/EnvironmentShaders.h"
#include "DingoEngine/Graphics/GraphicsDevice.h"
#include "DingoEngine/Graphics/HalfFloat.h"
#include "DingoEngine/Graphics/Renderer.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
namespace
{
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
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace Dingo
{

	namespace Internal
	{
		struct ProceduralBake
		{
			ComputePass* Transmittance = nullptr;
			ComputePass* MultiScatter = nullptr;
			ComputePass* SkyView = nullptr;
			ComputePass* SkyCube = nullptr;
			std::vector<ComputePass*> Derived;
			bool LutsBaked = false;

			~ProceduralBake()
			{
				DestroyAndDelete(Transmittance);
				DestroyAndDelete(MultiScatter);
				DestroyAndDelete(SkyView);
				DestroyAndDelete(SkyCube);
				for (ComputePass*& pass : Derived)
					DestroyAndDelete(pass);
			}
		};
	}

	namespace
	{
		uint64_t AllocateEnvironmentId()
		{
			static std::atomic<uint64_t> s_NextId{ 1 };
			return s_NextId.fetch_add(1, std::memory_order_relaxed);
		}

		bool HasHdrExtension(const std::filesystem::path& path)
		{
			std::string extension = Internal::PathToUtf8(path.extension());
			std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return extension == ".hdr";
		}

		TextureParams CubeParams(uint32_t faceSize, const std::string& debugName)
		{
			return TextureParams()
				.SetDebugName(debugName + " (cube)")
				.SetWidth(faceSize)
				.SetHeight(faceSize)
				.SetMipLevels(GetMipLevelCount(faceSize, faceSize))
				.SetFormat(TextureFormat::RGBA16F)
				.SetDimension(TextureDimension::TextureCube)
				.SetIsStorage(true);
		}

		constexpr uint32_t k_ShSourceSize = 32;
		constexpr uint32_t k_TransmittanceWidth = 256;
		constexpr uint32_t k_TransmittanceHeight = 64;
		constexpr uint32_t k_MultiScatterSize = 32;
		constexpr uint32_t k_SkyViewWidth = 192;
		constexpr uint32_t k_SkyViewHeight = 108;

		struct SkyPush
		{
			glm::vec4 Rayleigh;
			glm::vec4 Ozone;
			glm::vec4 Ground;
			glm::vec4 Sun;
			glm::vec4 Illuminance;
			glm::vec4 Night;
			uint32_t Size[4];
			glm::vec4 Reserved;
		};
		static_assert(sizeof(SkyPush) == 128, "the sky kernels' push block is 8 vec4");

		float SrgbToLinear(float c)
		{
			return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
		}

		glm::vec3 SrgbToLinear(const glm::vec3& c)
		{
			return glm::vec3(SrgbToLinear(c.x), SrgbToLinear(c.y), SrgbToLinear(c.z));
		}

		bool InRange(float value, float low, float high)
		{
			return std::isfinite(value) && value >= low && value <= high;
		}

		bool InRange(const glm::vec3& value, float low, float high)
		{
			return InRange(value.x, low, high) && InRange(value.y, low, high) && InRange(value.z, low, high);
		}

		const char* FindSkyProblem(const ProceduralSky& sky)
		{
			const float huge = 1.0e6f;
			if (!InRange(sky.SunDirection, -huge, huge) || glm::dot(sky.SunDirection, sky.SunDirection) < 1.0e-12f)
				return "SunDirection must be finite and not zero";
			if (!InRange(sky.SunColor, 0.0f, huge))
				return "SunColor must be finite and at least 0";
			if (!InRange(sky.SunIntensity, 0.0f, huge))
				return "SunIntensity must be finite and at least 0";
			if (!InRange(sky.SunDiscRadius, 0.0f, 10.0f))
				return "SunDiscRadius must be from 0 to 10 degrees";
			if (!InRange(sky.SunDiscIntensity, 0.0f, huge))
				return "SunDiscIntensity must be finite and at least 0";
			if (!InRange(sky.Turbidity, 0.0f, 50.0f))
				return "Turbidity must be from 0 to 50";
			if (!InRange(sky.MieAnisotropy, 0.0f, 0.99f))
				return "MieAnisotropy must be from 0 to 0.99";
			if (!InRange(sky.RayleighScale, 0.0f, 100.0f) || !InRange(sky.OzoneScale, 0.0f, 100.0f))
				return "RayleighScale and OzoneScale must be from 0 to 100";
			if (!InRange(sky.GroundAlbedo, 0.0f, 1.0f))
				return "GroundAlbedo must be from 0 to 1";
			if (!InRange(sky.NightColor, 0.0f, huge))
				return "NightColor must be finite and at least 0";
			if (!InRange(sky.Altitude, 0.0f, 20000.0f))
				return "Altitude must be from 0 to 20000 metres";
			if (!InRange(sky.RebakeAngle, 0.0f, 45.0f))
				return "RebakeAngle must be from 0 to 45 degrees";
			return nullptr;
		}

		GlslAtmosphere::DE_Atmosphere AtmosphereOf(const ProceduralSky& sky)
		{
			return GlslAtmosphere::DE_EarthAtmosphere(sky.RayleighScale, sky.OzoneScale, sky.Turbidity, sky.MieAnisotropy);
		}

		float ObserverRadius(const ProceduralSky& sky)
		{
			return GlslAtmosphere::DE_PLANET_RADIUS + sky.Altitude * 0.001f;
		}

		float AngleBetween(const glm::vec3& a, const glm::vec3& b)
		{
			const glm::vec3 unitA = glm::normalize(a);
			const glm::vec3 unitB = glm::normalize(b);
			return glm::degrees(2.0f * std::asin((std::min)(0.5f * glm::length(unitA - unitB), 1.0f)));
		}

		Texture* CreateSkyLut(uint32_t width, uint32_t height, const std::string& name)
		{
			return Texture::Create(TextureParams()
				.SetDebugName(name)
				.SetWidth(width)
				.SetHeight(height)
				.SetFormat(TextureFormat::RGBA16F)
				.SetDimension(TextureDimension::Texture2D)
				.SetIsStorage(true));
		}

		Texture* CreateEquirect(uint32_t width, uint32_t height, const std::string& debugName)
		{
			return Texture::Create(TextureParams()
				.SetDebugName(debugName + " (equirect)")
				.SetWidth(width)
				.SetHeight(height)
				.SetFormat(TextureFormat::RGBA16F)
				.SetDimension(TextureDimension::Texture2D));
		}
	}

	Environment* Environment::Create(const EnvironmentParams& params)
	{
		if (!Internal::RequireGraphicsDevice("Environment::Create"))
			return nullptr;

		if (!HasHdrExtension(params.Path))
		{
			DE_CORE_ERROR("Environment::Create: '{}' is not a .hdr file.", Internal::PathToUtf8(params.Path));
			return nullptr;
		}
		if (params.FaceSize != 0 && Internal::ChooseEnvironmentFaceSize(1, params.FaceSize) == 0)
		{
			DE_CORE_ERROR("Environment::Create: '{}': FaceSize {} is not a power of two from {} to {}.", params.DebugName, params.FaceSize, Internal::k_EnvironmentFaceMin, Internal::k_EnvironmentFaceMax);
			return nullptr;
		}

		std::vector<uint16_t> halves;
		uint32_t width = 0, height = 0;
		if (!Internal::EnvironmentAccess::DecodeHdr(params.Path, halves, width, height))
			return nullptr;

		const uint32_t faceSize = Internal::ChooseEnvironmentFaceSize(width, params.FaceSize);
		return Internal::EnvironmentAccess::Create(std::move(halves), width, height, faceSize, params.DebugName);
	}

	Environment* Environment::CreateProcedural(const ProceduralEnvironmentParams& params)
	{
		if (!Internal::RequireGraphicsDevice("Environment::CreateProcedural"))
			return nullptr;

		if (params.FaceSize == 0 || Internal::ChooseEnvironmentFaceSize(1, params.FaceSize) == 0)
		{
			DE_CORE_ERROR("Environment::CreateProcedural: '{}': FaceSize {} is not a power of two from {} to {}.", params.DebugName, params.FaceSize, Internal::k_EnvironmentFaceMin, Internal::k_EnvironmentFaceMax);
			return nullptr;
		}
		if (const char* problem = FindSkyProblem(params.Sky))
		{
			DE_CORE_ERROR("Environment::CreateProcedural: '{}': {}.", params.DebugName, problem);
			return nullptr;
		}

		Environment* environment = new Environment();
		if (!environment->BuildProcedural(params.Sky, params.FaceSize, params.DebugName))
		{
			delete environment;
			return nullptr;
		}
		return environment;
	}

	Environment::~Environment()
	{
		DestroyAndDelete(m_Cube);
		DestroyAndDelete(m_Specular);
		DestroyAndDelete(m_Irradiance);
		ReleaseEquirect();
		delete m_SkyBake;
		DestroyAndDelete(m_SkyTransmittance);
		DestroyAndDelete(m_SkyMultiScatter);
		DestroyAndDelete(m_SkyView);
	}

	bool Environment::BuildProcedural(const ProceduralSky& sky, uint32_t faceSize, const std::string& debugName)
	{
		m_Cube = Texture::Create(CubeParams(faceSize, debugName));
		m_Specular = Texture::Create(TextureParams()
			.SetDebugName(debugName + " (specular)")
			.SetWidth(Internal::k_IblFaceSize)
			.SetHeight(Internal::k_IblFaceSize)
			.SetMipLevels(Internal::k_IblMipLevels)
			.SetFormat(TextureFormat::RGBA16F)
			.SetDimension(TextureDimension::TextureCube)
			.SetIsStorage(true));
		m_Irradiance = GraphicsBuffer::CreateStorageBuffer(Internal::k_IblIrradianceBytes, debugName + " (irradiance)");
		m_SkyTransmittance = CreateSkyLut(k_TransmittanceWidth, k_TransmittanceHeight, debugName + " (transmittance LUT)");
		m_SkyMultiScatter = CreateSkyLut(k_MultiScatterSize, k_MultiScatterSize, debugName + " (multiple scattering LUT)");
		m_SkyView = CreateSkyLut(k_SkyViewWidth, k_SkyViewHeight, debugName + " (sky view LUT)");
		if (!m_Cube || !m_Specular || !m_Irradiance || !m_SkyTransmittance || !m_SkyMultiScatter || !m_SkyView)
			return false;

		m_Sky = sky;
		m_Procedural = true;
		m_FaceSize = faceSize;
		m_MipLevels = Dingo::GetMipLevelCount(faceSize, faceSize);
		m_Id = AllocateEnvironmentId();
		m_Ready = false;
		return true;
	}

	bool Environment::SetSky(const ProceduralSky& sky)
	{
		if (!m_Procedural)
		{
			DE_CORE_WARN("Environment::SetSky: this environment is a panorama, not a procedural sky.");
			return false;
		}
		if (const char* problem = FindSkyProblem(sky))
		{
			if (!m_SkyWarned)
			{
				m_SkyWarned = true;
				DE_CORE_WARN("Environment::SetSky: the sky was ignored: {}.", problem);
			}
			return false;
		}

		ProceduralSky next = sky;
		const bool sunMoved = AngleBetween(m_Sky.SunDirection, sky.SunDirection) > 0.0f
			&& AngleBetween(m_Sky.SunDirection, sky.SunDirection) >= sky.RebakeAngle;
		if (!sunMoved)
			next.SunDirection = m_Sky.SunDirection;

		const bool atmosphereChanged = next.RayleighScale != m_Sky.RayleighScale || next.OzoneScale != m_Sky.OzoneScale
			|| next.Turbidity != m_Sky.Turbidity || next.GroundAlbedo != m_Sky.GroundAlbedo;
		const bool skyChanged = atmosphereChanged || next.SunColor != m_Sky.SunColor || next.SunIntensity != m_Sky.SunIntensity
			|| next.MieAnisotropy != m_Sky.MieAnisotropy || next.NightColor != m_Sky.NightColor || next.Altitude != m_Sky.Altitude;
		m_LutsDirty = m_LutsDirty || atmosphereChanged;
		m_SkyDirty = m_SkyDirty || sunMoved || skyChanged;
		m_Sky = next;
		return true;
	}

	glm::vec3 Environment::GetSunTransmittance(const glm::vec3& travelDirection) const
	{
		if (!m_Procedural)
			return glm::vec3(1.0f);
		const float length = glm::length(travelDirection);
		if (!(length > 1.0e-6f) || !std::isfinite(length))
			return glm::vec3(0.0f);

		const float r = ObserverRadius(m_Sky);
		const float mu = -travelDirection.y / length;
		const float horizon = GlslAtmosphere::DE_HorizonCos(r);
		const float fade = glm::smoothstep(horizon - 0.02f, horizon, mu);
		if (fade <= 0.0f)
			return glm::vec3(0.0f);
		return GlslAtmosphere::DE_TransmittanceMarch(AtmosphereOf(m_Sky), r, (std::max)(mu, horizon)) * fade;
	}

	bool Environment::Build(std::vector<uint16_t>&& halves, uint32_t width, uint32_t height, uint32_t faceSize, const std::string& debugName)
	{
		if (!Internal::RequireGraphicsDevice("Environment") || width == 0 || height == 0 || halves.size() != static_cast<size_t>(width) * height * 4)
			return false;
		if (faceSize == 0 || Internal::ChooseEnvironmentFaceSize(width, faceSize) != faceSize)
		{
			DE_CORE_ERROR("Environment '{}': face size {} is not a power of two from {} to {}.", debugName, faceSize, Internal::k_EnvironmentFaceMin, Internal::k_EnvironmentFaceMax);
			return false;
		}

		m_Cube = Texture::Create(CubeParams(faceSize, debugName));
		m_Equirect = CreateEquirect(width, height, debugName);
		m_Specular = Texture::Create(TextureParams()
			.SetDebugName(debugName + " (specular)")
			.SetWidth(Internal::k_IblFaceSize)
			.SetHeight(Internal::k_IblFaceSize)
			.SetMipLevels(Internal::k_IblMipLevels)
			.SetFormat(TextureFormat::RGBA16F)
			.SetDimension(TextureDimension::TextureCube)
			.SetIsStorage(true));
		m_Irradiance = GraphicsBuffer::CreateStorageBuffer(Internal::k_IblIrradianceBytes, debugName + " (irradiance)");
		if (!m_Cube || !m_Equirect || !m_Specular || !m_Irradiance)
			return false;

		m_Halves = std::move(halves);
		m_Width = width;
		m_Height = height;
		m_FaceSize = faceSize;
		m_MipLevels = Dingo::GetMipLevelCount(faceSize, faceSize);
		m_Id = AllocateEnvironmentId();
		m_Ready = false;
		return true;
	}

	void Environment::ReleaseEquirect()
	{
		DestroyAndDelete(m_Equirect);
	}

	bool Environment::Prepare()
	{
		return m_Procedural ? PrepareProcedural() : PrepareHdr();
	}

	bool Environment::PrepareHdr()
	{
		if (m_Ready)
			return true;
		if (m_BakeFailed || !m_Cube || !m_Equirect || Renderer::IsFrameSkipped() || !Renderer::TryGetRecordingCommandList())
			return false;

		Shader* bake = Internal::GetEnvironmentShader(Internal::EnvironmentShaderVariant::EquirectBake);
		Shader* downsample = Internal::GetEnvironmentShader(Internal::EnvironmentShaderVariant::Downsample);
		Shader* shProject = Internal::GetEnvironmentShader(Internal::EnvironmentShaderVariant::ShProject);
		Shader* prefilter = Internal::GetEnvironmentShader(Internal::EnvironmentShaderVariant::SpecularPrefilter);
		Sampler* equirectSampler = Internal::GetEquirectSampler();
		if (!bake || !downsample || !shProject || !prefilter || !equirectSampler)
		{
			if (!m_ReportedFailure)
			{
				m_ReportedFailure = true;
				DE_CORE_ERROR("Environment::Prepare: the bake shaders are not valid; the environment stays unbaked.");
			}
			return false;
		}

		DE_PROFILE_SCOPE("Environment::Prepare");
		Renderer::BeginGpuTimer("Environment");
		m_Equirect->UploadSubresource(m_Halves.data(), 0, 0);

		std::vector<ComputePass*> passes;
		auto groups = [](uint32_t size) { return (size + 7u) / 8u; };

		ComputePass* equirectPass = ComputePass::Create(ComputePassParams().SetDebugName("Environment equirect bake").SetShader(bake));
		passes.push_back(equirectPass);
		equirectPass->SetTexture(1, m_Equirect);
		equirectPass->SetSampler(2, equirectSampler);
		equirectPass->SetStorageTexture(3, m_Cube, 0);
		uint32_t push[4] = { m_FaceSize, 0, 0, 0 };
		equirectPass->SetPushConstants(push, sizeof(push));
		const char* stage = "cube";
		bool recorded = Renderer::Dispatch(equirectPass, groups(m_FaceSize), groups(m_FaceSize), 6);
		recorded = recorded && RecordDerivedStages(passes, passes, false, stage);

		Renderer::EndGpuTimer();

		Texture* equirect = nullptr;
		if (recorded)
		{
			equirect = m_Equirect;
			m_Equirect = nullptr;
			m_Ready = true;
			++m_BakeGeneration;
			m_Halves = {};
		}
		else
		{
			m_BakeFailed = true;
			DE_CORE_ERROR("Environment::Prepare: recording the bake failed at the {} stage; the environment stays unbaked.", stage);
		}

		Renderer::RunAfterFrame([equirect, passes = std::move(passes)]() mutable
		{
			for (ComputePass*& pass : passes)
				DestroyAndDelete(pass);
			DestroyAndDelete(equirect);
		});
		return recorded;
	}

	bool Environment::PrepareProcedural()
	{
		const uint64_t frame = Renderer::GetFrameIndex();
		if (m_Ready && (!m_SkyDirty || m_LastBakeFrame == frame))
			return true;
		if (m_BakeFailed || !m_Cube || Renderer::IsFrameSkipped() || !Renderer::TryGetRecordingCommandList())
			return false;

		using Variant = Internal::EnvironmentShaderVariant;
		Shader* transmittance = Internal::GetEnvironmentShader(Variant::SkyTransmittance);
		Shader* multiScatter = Internal::GetEnvironmentShader(Variant::SkyMultiScatter);
		Shader* skyView = Internal::GetEnvironmentShader(Variant::SkyView);
		Shader* skyCube = Internal::GetEnvironmentShader(Variant::SkyCube);
		Shader* downsample = Internal::GetEnvironmentShader(Variant::Downsample);
		Shader* shProject = Internal::GetEnvironmentShader(Variant::ShProject);
		Shader* prefilter = Internal::GetEnvironmentShader(Variant::SpecularPrefilter);
		if (!transmittance || !multiScatter || !skyView || !skyCube || !downsample || !shProject || !prefilter)
		{
			if (!m_ReportedFailure)
			{
				m_ReportedFailure = true;
				DE_CORE_ERROR("Environment::Prepare: the sky bake shaders are not valid; the environment stays unbaked.");
			}
			return false;
		}

		if (!m_SkyBake)
		{
			Sampler* clamp = Renderer::GetClampSampler();
			Internal::ProceduralBake* bake = new Internal::ProceduralBake();
			bake->Transmittance = ComputePass::Create(ComputePassParams().SetDebugName("Sky transmittance LUT").SetShader(transmittance));
			bake->MultiScatter = ComputePass::Create(ComputePassParams().SetDebugName("Sky multiple scattering LUT").SetShader(multiScatter));
			bake->SkyView = ComputePass::Create(ComputePassParams().SetDebugName("Sky view LUT").SetShader(skyView));
			bake->SkyCube = ComputePass::Create(ComputePassParams().SetDebugName("Sky cube").SetShader(skyCube));
			if (!bake->Transmittance || !bake->MultiScatter || !bake->SkyView || !bake->SkyCube)
			{
				delete bake;
				m_BakeFailed = true;
				DE_CORE_ERROR("Environment::Prepare: creating the sky bake passes failed; the environment stays unbaked.");
				return false;
			}
			bake->Transmittance->SetStorageTexture(3, m_SkyTransmittance, 0);
			bake->MultiScatter->SetTexture(1, m_SkyTransmittance);
			bake->MultiScatter->SetSampler(2, clamp);
			bake->MultiScatter->SetStorageTexture(3, m_SkyMultiScatter, 0);
			bake->SkyView->SetTexture(1, m_SkyTransmittance);
			bake->SkyView->SetTexture(5, m_SkyMultiScatter);
			bake->SkyView->SetSampler(2, clamp);
			bake->SkyView->SetStorageTexture(3, m_SkyView, 0);
			bake->SkyCube->SetTexture(1, m_SkyView);
			bake->SkyCube->SetSampler(2, clamp);
			bake->SkyCube->SetStorageTexture(3, m_Cube, 0);
			m_SkyBake = bake;
		}

		const GlslAtmosphere::DE_Atmosphere atmosphere = AtmosphereOf(m_Sky);
		const glm::vec3 toSun = -glm::normalize(m_Sky.SunDirection);
		SkyPush push = {};
		push.Rayleigh = glm::vec4(atmosphere.RayleighScattering, atmosphere.MieScattering);
		push.Ozone = glm::vec4(atmosphere.OzoneAbsorption, atmosphere.MieAbsorption);
		push.Ground = glm::vec4(SrgbToLinear(m_Sky.GroundAlbedo), m_Sky.MieAnisotropy);
		push.Sun = glm::vec4(toSun, ObserverRadius(m_Sky));
		push.Illuminance = glm::vec4(SrgbToLinear(m_Sky.SunColor) * (m_Sky.SunIntensity * glm::pi<float>()), m_MultiScatter ? 1.0f : 0.0f);
		push.Night = glm::vec4(m_Sky.NightColor, 0.0f);
		push.Size[0] = m_FaceSize;
		for (ComputePass* pass : { m_SkyBake->Transmittance, m_SkyBake->MultiScatter, m_SkyBake->SkyView, m_SkyBake->SkyCube })
			pass->SetPushConstants(&push, sizeof(push));

		DE_PROFILE_SCOPE("Environment::Prepare");
		Renderer::BeginGpuTimer("Environment");
		auto groups = [](uint32_t size) { return (size + 7u) / 8u; };
		const char* stage = "transmittance LUT";
		bool recorded = true;
		if (!m_SkyBake->LutsBaked || m_LutsDirty)
		{
			recorded = Renderer::Dispatch(m_SkyBake->Transmittance, k_TransmittanceWidth / 32, k_TransmittanceHeight / 8, 1);
			stage = "multiple scattering LUT";
			recorded = recorded && Renderer::Dispatch(m_SkyBake->MultiScatter, k_MultiScatterSize, k_MultiScatterSize, 1);
		}
		stage = "sky view LUT";
		recorded = recorded && Renderer::Dispatch(m_SkyBake->SkyView, groups(k_SkyViewWidth), groups(k_SkyViewHeight), 1);
		stage = "cube";
		recorded = recorded && Renderer::Dispatch(m_SkyBake->SkyCube, groups(m_FaceSize), groups(m_FaceSize), 6);
		std::vector<ComputePass*> transient;
		recorded = recorded && RecordDerivedStages(m_SkyBake->Derived, transient, true, stage);
		Renderer::EndGpuTimer();

		if (recorded)
		{
			m_SkyBake->LutsBaked = true;
			m_LutsDirty = false;
			m_SkyDirty = false;
			m_Ready = true;
			m_LastBakeFrame = frame;
			++m_BakeGeneration;
		}
		else
		{
			m_BakeFailed = true;
			DE_CORE_ERROR("Environment::Prepare: recording the sky bake failed at the {} stage; the environment stays as it was.", stage);
		}

		if (!transient.empty())
		{
			Renderer::RunAfterFrame([transient = std::move(transient)]() mutable
			{
				for (ComputePass*& pass : transient)
					DestroyAndDelete(pass);
			});
		}
		return recorded;
	}

	bool Environment::RecordDerivedStages(std::vector<ComputePass*>& passes, std::vector<ComputePass*>& transient, bool reuse, const char*& stage)
	{
		using Variant = Internal::EnvironmentShaderVariant;
		Shader* downsample = Internal::GetEnvironmentShader(Variant::Downsample);
		Shader* shProject = Internal::GetEnvironmentShader(Variant::ShProject);
		Shader* prefilter = Internal::GetEnvironmentShader(Variant::SpecularPrefilter);
		auto groups = [](uint32_t size) { return (size + 7u) / 8u; };

		size_t next = 0;
		auto acquire = [&](Shader* shader, const char* name, bool& created) -> ComputePass*
		{
			created = !reuse || next >= passes.size();
			if (created)
				passes.push_back(ComputePass::Create(ComputePassParams().SetDebugName(name).SetShader(shader)));
			return reuse ? passes[next++] : passes.back();
		};

		uint32_t push[4] = { m_FaceSize, 0, 0, 0 };
		stage = "downsample";
		bool recorded = true;
		for (uint32_t mip = 1; recorded && mip < m_MipLevels; ++mip)
		{
			const uint32_t size = (std::max)(m_FaceSize >> mip, 1u);
			bool created = false;
			ComputePass* pass = acquire(downsample, "Environment downsample", created);
			if (created)
			{
				pass->SetTexture(1, m_Cube, mip - 1, 1);
				pass->SetSampler(2, Renderer::GetClampSampler());
				pass->SetStorageTexture(3, m_Cube, mip);
				push[0] = size;
				pass->SetPushConstants(push, sizeof(push));
			}
			recorded = Renderer::Dispatch(pass, groups(size), groups(size), 6);
		}

		stage = "BRDF LUT";
		recorded = recorded && Internal::EnsureBrdfLutBaked(transient);

		if (recorded)
		{
			stage = "SH projection";
			const uint32_t shSize = (std::min)(m_FaceSize, k_ShSourceSize);
			uint32_t shMip = 0;
			while ((m_FaceSize >> shMip) > shSize)
				++shMip;
			bool created = false;
			ComputePass* shPass = acquire(shProject, "Environment irradiance SH", created);
			if (created)
			{
				shPass->SetTexture(1, m_Cube, shMip, 1);
				shPass->SetSampler(2, Renderer::GetClampSampler());
				shPass->SetStorageBuffer(4, m_Irradiance);
				push[0] = shSize;
				shPass->SetPushConstants(push, sizeof(push));
			}
			recorded = Renderer::Dispatch(shPass, 1, 1, 1);
		}

		stage = "specular prefilter";
		for (uint32_t mip = 0; recorded && mip < Internal::k_IblMipLevels; ++mip)
		{
			const uint32_t size = (std::max)(Internal::k_IblFaceSize >> mip, 1u);
			bool created = false;
			ComputePass* pass = acquire(prefilter, "Environment specular prefilter", created);
			if (created)
			{
				pass->SetTexture(1, m_Cube);
				pass->SetSampler(2, Renderer::GetClampSampler());
				pass->SetStorageTexture(3, m_Specular, mip);
				push[0] = size;
				push[1] = mip;
				push[2] = m_FaceSize;
				pass->SetPushConstants(push, sizeof(push));
			}
			recorded = Renderer::Dispatch(pass, groups(size), groups(size), 6);
		}
		return recorded;
	}

	bool Internal::EnvironmentAccess::SetSunDirection(Environment* environment, const glm::vec3& travelDirection)
	{
		if (!environment || !environment->m_Procedural || !(glm::dot(travelDirection, travelDirection) > 1.0e-12f) || !std::isfinite(travelDirection.x + travelDirection.y + travelDirection.z))
			return false;
		const float angle = AngleBetween(environment->m_Sky.SunDirection, travelDirection);
		if (!(angle > 0.0f) || angle < environment->m_Sky.RebakeAngle)
			return false;
		environment->m_Sky.SunDirection = travelDirection;
		environment->m_SkyDirty = true;
		return true;
	}

	float Internal::EnvironmentAccess::GetGroundRatio(const ProceduralSky& sky)
	{
		return GlslAtmosphere::DE_PLANET_RADIUS / ObserverRadius(sky);
	}

	void Internal::EnvironmentAccess::SetMultipleScattering(Environment* environment, bool enabled)
	{
		if (!environment || environment->m_MultiScatter == enabled)
			return;
		environment->m_MultiScatter = enabled;
		environment->m_SkyDirty = true;
	}

	bool Internal::EnvironmentAccess::DecodeHdr(const std::filesystem::path& path, std::vector<uint16_t>& halves, uint32_t& width, uint32_t& height)
	{
		const std::filesystem::path resolvedPath = Internal::ResolveRawAssetPath(path);
		const std::optional<std::vector<uint8_t>> bytes = FileSystem::ReadFileBytes(resolvedPath);
		if (!bytes || bytes->empty())
		{
			DE_CORE_ERROR("Environment: failed to read '{}'.", Internal::PathToUtf8(resolvedPath));
			return false;
		}
		if (!Internal::IsHdrImage(bytes->data(), bytes->size()))
		{
			DE_CORE_ERROR("Environment: '{}' is not a Radiance .hdr image.", Internal::PathToUtf8(resolvedPath));
			return false;
		}

		const float* pixels = Internal::ReadImageHdrFromMemory(bytes->data(), bytes->size(), &width, &height, true);
		if (!pixels)
			return false;

		halves.resize(static_cast<size_t>(width) * height * 4);
		for (size_t i = 0; i < halves.size(); ++i)
			halves[i] = Internal::FloatToHalf(pixels[i]);
		Internal::FreeImageHdr(pixels);
		return true;
	}

	Environment* Internal::EnvironmentAccess::Create(std::vector<uint16_t>&& halves, uint32_t width, uint32_t height, uint32_t faceSize, const std::string& debugName)
	{
		Environment* environment = new Environment();
		if (!environment->Build(std::move(halves), width, height, faceSize, debugName))
		{
			delete environment;
			return nullptr;
		}
		return environment;
	}

	bool Internal::EnvironmentAccess::Reload(Environment* environment, std::vector<uint16_t>&& halves, uint32_t width, uint32_t height, uint32_t faceSize)
	{
		if (!environment || environment->m_Procedural || !environment->m_Cube || width == 0 || height == 0 || halves.size() != static_cast<size_t>(width) * height * 4)
			return false;
		if (faceSize == 0 || ChooseEnvironmentFaceSize(width, faceSize) != faceSize)
			return false;

		Texture* equirect = CreateEquirect(width, height, "Environment");
		if (!equirect)
			return false;

		TextureParams cube = environment->m_Cube->GetParams();
		cube.SetWidth(faceSize).SetHeight(faceSize).SetMipLevels(GetMipLevelCount(faceSize, faceSize));
		environment->m_Cube->Reinitialize(cube);
		if (environment->m_Cube->GetParams().Width != faceSize)
		{
			DestroyAndDelete(equirect);
			return false;
		}

		environment->ReleaseEquirect();
		environment->m_Equirect = equirect;
		environment->m_Halves = std::move(halves);
		environment->m_Width = width;
		environment->m_Height = height;
		environment->m_FaceSize = faceSize;
		environment->m_MipLevels = cube.MipLevels;
		environment->m_Ready = false;
		environment->m_ReportedFailure = false;
		environment->m_BakeFailed = false;
		return true;
	}

}
