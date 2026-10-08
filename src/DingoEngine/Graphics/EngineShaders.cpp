#include "depch.h"
#include "DingoEngine/Graphics/EngineShaders.h"

namespace
{
#include "Renderer3D_Lit.glsl.inl"
#include "Fullscreen.glsl.inl"
#include "ToneMapping.glsl.inl"
#include "PostToneMap.glsl.inl"
#include "PostBloom.glsl.inl"
#include "PostAmbientOcclusion.glsl.inl"
#include "Shadows.glsl.inl"
#include "Skinning.glsl.inl"
#include "Renderer3D_Shadow.glsl.inl"
#include "Renderer3D_ShadowProbe.glsl.inl"

	struct EmbeddedShader
	{
		std::string_view Name;
		std::string_view Source;
	};

	template<size_t N>
	constexpr std::string_view View(const unsigned char (&bytes)[N])
	{
		return std::string_view(reinterpret_cast<const char*>(bytes), N);
	}

	// Every file in Graphics/Shaders: premake embeds them all, and a file missing here can't be found
	// by name or #included in a build without the source tree.
	const EmbeddedShader s_EmbeddedShaders[] = {
		{ "Renderer3D_Lit.glsl", View(k_Renderer3D_Lit_glsl) },
		{ "Fullscreen.glsl", View(k_Fullscreen_glsl) },
		{ "ToneMapping.glsl", View(k_ToneMapping_glsl) },
		{ "PostToneMap.glsl", View(k_PostToneMap_glsl) },
		{ "PostBloom.glsl", View(k_PostBloom_glsl) },
		{ "PostAmbientOcclusion.glsl", View(k_PostAmbientOcclusion_glsl) },
		{ "Shadows.glsl", View(k_Shadows_glsl) },
		{ "Skinning.glsl", View(k_Skinning_glsl) },
		{ "Renderer3D_Shadow.glsl", View(k_Renderer3D_Shadow_glsl) },
		{ "Renderer3D_ShadowProbe.glsl", View(k_Renderer3D_ShadowProbe_glsl) },
	};
}

namespace Dingo::Internal
{

	std::string_view FindEmbeddedEngineShader(std::string_view fileName)
	{
		for (const EmbeddedShader& shader : s_EmbeddedShaders)
		{
			if (shader.Name == fileName)
				return shader.Source;
		}
		return {};
	}

	std::filesystem::path FindEngineShaderFile(std::string_view fileName)
	{
#ifdef DE_ENGINE_SHADER_DIR
		const std::filesystem::path path = std::filesystem::path(u8"" DE_ENGINE_SHADER_DIR) / std::filesystem::path(fileName);
		std::error_code ec;
		if (std::filesystem::exists(path, ec))
			return path;
#else
		(void)fileName;
#endif
		return {};
	}

	Shader* CreateEngineShader(ShaderParams params, std::string_view fileName)
	{
		const std::filesystem::path path = FindEngineShaderFile(fileName);
		if (!path.empty())
			return Shader::Create(params.SetFilePath(path).SetSourceCode({}));

		const std::string_view source = FindEmbeddedEngineShader(fileName);
		DE_CORE_ASSERT(!source.empty(), "Engine shader is not embedded; add it to s_EmbeddedShaders in EngineShaders.cpp.");
		return Shader::Create(params.SetFilePath({}).SetSourceCode(std::string(source)));
	}

}
