#include "depch.h"
#include "DingoEngine/Graphics/EnvironmentShaders.h"
#include "DingoEngine/Asset/UnmanagedShaderWatch.h"
#include "DingoEngine/Graphics/ComputePass.h"
#include "DingoEngine/Graphics/EngineShaders.h"
#include "DingoEngine/Graphics/Renderer.h"

#include <array>

namespace
{
	constexpr size_t k_VariantCount = 6;
	constexpr uint32_t k_BrdfLutSize = 128;

	// A variant that failed to compile stays here, invalid and watched, so it is neither rebuilt nor
	// logged again on every call and a hot reload that fixes it makes it usable.
	std::array<Dingo::Shader*, k_VariantCount> s_Shaders = {};
	std::array<bool, k_VariantCount> s_Attempted = {};
	Dingo::Sampler* s_EquirectSampler = nullptr;
	Dingo::Texture* s_BrdfLut = nullptr;
	bool s_LutBaked = false;
	bool s_LutReportedFailure = false;
	bool s_LutCreateFailed = false;
}

namespace Dingo::Internal
{

	Shader* GetEnvironmentShader(EnvironmentShaderVariant variant)
	{
		const size_t index = static_cast<size_t>(variant);
		Shader*& shader = s_Shaders[index];
		if (s_Attempted[index])
			return shader && shader->IsValid() ? shader : nullptr;
		s_Attempted[index] = true;

		ShaderParams params;
		switch (variant)
		{
		case EnvironmentShaderVariant::EquirectBake: params.SetName("EnvironmentEquirectBakeShader").AddDefine("DE_ENV_EQUIRECT"); break;
		case EnvironmentShaderVariant::Downsample:   params.SetName("EnvironmentDownsampleShader").AddDefine("DE_ENV_DOWNSAMPLE"); break;
		case EnvironmentShaderVariant::Skybox:       params.SetName("SkyboxShader"); break;
		case EnvironmentShaderVariant::ShProject:    params.SetName("EnvironmentShShader").AddDefine("DE_ENV_SH"); break;
		case EnvironmentShaderVariant::SpecularPrefilter: params.SetName("EnvironmentPrefilterShader").AddDefine("DE_ENV_PREFILTER"); break;
		case EnvironmentShaderVariant::BrdfLut:      params.SetName("IblBrdfLutShader").AddDefine("DE_IBL_BRDF_LUT"); break;
		}
		shader = CreateEngineShader(params, variant == EnvironmentShaderVariant::Skybox ? "Skybox.glsl" : "EnvironmentBake.glsl");
		if (shader)
			WatchUnmanagedShader(shader);
		return shader && shader->IsValid() ? shader : nullptr;
	}

	Sampler* GetEquirectSampler()
	{
		if (!s_EquirectSampler)
		{
			s_EquirectSampler = Sampler::Create(SamplerParams()
				.SetAddressModes(SamplerAddressMode::Repeat, SamplerAddressMode::Clamp));
		}
		return s_EquirectSampler;
	}

	Texture* GetBrdfLut()
	{
		if (!s_BrdfLut && !s_LutCreateFailed)
		{
			s_BrdfLut = Texture::Create(TextureParams()
				.SetDebugName("IBL BRDF LUT")
				.SetWidth(k_BrdfLutSize)
				.SetHeight(k_BrdfLutSize)
				.SetFormat(TextureFormat::RGBA16F)
				.SetDimension(TextureDimension::Texture2D)
				.SetIsStorage(true));
			if (!s_BrdfLut)
			{
				s_LutCreateFailed = true;
				DE_CORE_ERROR("IBL: creating the BRDF LUT texture failed; image-based lighting stays unavailable.");
			}
		}
		return s_BrdfLut;
	}

	bool EnsureBrdfLutBaked(std::vector<ComputePass*>& passes)
	{
		if (s_LutBaked)
			return true;
		Shader* shader = GetEnvironmentShader(EnvironmentShaderVariant::BrdfLut);
		Texture* lut = GetBrdfLut();
		if (!shader || !lut)
		{
			if (!s_LutReportedFailure)
			{
				s_LutReportedFailure = true;
				DE_CORE_ERROR("IBL: the BRDF LUT bake shader or texture is not valid; image-based lighting stays unbaked.");
			}
			return false;
		}

		ComputePass* pass = ComputePass::Create(ComputePassParams().SetDebugName("IBL BRDF LUT bake").SetShader(shader));
		passes.push_back(pass);
		pass->SetStorageTexture(3, lut, 0);
		constexpr uint32_t groups = k_BrdfLutSize / 8;
		const bool recorded = Renderer::Dispatch(pass, groups, groups, 1);
		if (recorded)
			s_LutBaked = true;
		else
			DE_CORE_ERROR("IBL: recording the BRDF LUT bake failed.");
		return recorded;
	}

	void DestroyEnvironmentShaders()
	{
		for (Shader*& shader : s_Shaders)
		{
			if (!shader)
				continue;
			UnwatchUnmanagedShader(shader);
			DestroyAndDelete(shader);
		}
		s_Attempted = {};
		DestroyAndDelete(s_EquirectSampler);
		DestroyAndDelete(s_BrdfLut);
		s_LutBaked = false;
		s_LutReportedFailure = false;
		s_LutCreateFailed = false;
	}

}
