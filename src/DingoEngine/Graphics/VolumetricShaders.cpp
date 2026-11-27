#include "depch.h"
#include "DingoEngine/Graphics/VolumetricShaders.h"
#include "DingoEngine/Asset/UnmanagedShaderWatch.h"
#include "DingoEngine/Graphics/EngineShaders.h"
#include "DingoEngine/Graphics/Renderer.h"

#include <array>

namespace
{
	constexpr size_t k_KindCount = 2;

	// A kernel that failed to compile stays here, invalid and watched, so it is neither rebuilt nor
	// logged again on every call and a hot reload that fixes it makes it usable.
	std::array<Dingo::Shader*, k_KindCount> s_Shaders = {};
	std::array<bool, k_KindCount> s_Attempted = {};
}

namespace Dingo::Internal
{

	Shader* GetVolumetricShader(VolumetricShaderKind kind)
	{
		const size_t index = static_cast<size_t>(kind);
		Shader*& shader = s_Shaders[index];
		if (s_Attempted[index])
			return shader && shader->IsValid() ? shader : nullptr;
		s_Attempted[index] = true;

		const bool inject = kind == VolumetricShaderKind::Inject;
		shader = CreateEngineShader(ShaderParams().SetName(inject ? "VolumetricInjectShader" : "VolumetricIntegrateShader"),
			inject ? "VolumetricInject.glsl" : "VolumetricIntegrate.glsl");
		if (shader)
			WatchUnmanagedShader(shader);
		return shader && shader->IsValid() ? shader : nullptr;
	}

	void DestroyVolumetricShaders()
	{
		for (Shader*& shader : s_Shaders)
		{
			if (!shader)
				continue;
			UnwatchUnmanagedShader(shader);
			DestroyAndDelete(shader);
		}
		s_Attempted = {};
	}

}
