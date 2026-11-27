#include "depch.h"
#include "DingoEngine/Graphics/EnvironmentShaders.h"
#include "DingoEngine/Asset/UnmanagedShaderWatch.h"
#include "DingoEngine/Graphics/EngineShaders.h"
#include "DingoEngine/Graphics/Renderer.h"

#include <array>

namespace
{
	constexpr size_t k_VariantCount = 3;

	// A variant that failed to compile stays here, invalid and watched, so it is neither rebuilt nor
	// logged again on every call and a hot reload that fixes it makes it usable.
	std::array<Dingo::Shader*, k_VariantCount> s_Shaders = {};
	std::array<bool, k_VariantCount> s_Attempted = {};
	Dingo::Sampler* s_EquirectSampler = nullptr;
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
	}

}
