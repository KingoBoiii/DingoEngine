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
#include "Instancing.glsl.inl"
#include "Renderer3D_Shadow.glsl.inl"
#include "Renderer3D_ShadowProbe.glsl.inl"
#include "PostDepthCopy.glsl.inl"
#include "ParticleCommon.glsl.inl"
#include "ParticleSimulate.glsl.inl"
#include "ParticleDraw.glsl.inl"
#include "DingoUI.glsl.inl"
#include "CubeMap.glsl.inl"
#include "Renderer3D_MeshVertex.glsl.inl"
#include "Renderer3DCamera.glsl.inl"
#include "Renderer3DScene.glsl.inl"
#include "Brdf.glsl.inl"
#include "Ibl.glsl.inl"
#include "Renderer3D_Pbr.glsl.inl"
#include "Environment.glsl.inl"
#include "EnvironmentBake.glsl.inl"
#include "Skybox.glsl.inl"
#include "Atmosphere.glsl.inl"
#include "ProceduralSky.glsl.inl"
#include "VolumetricMath.glsl.inl"
#include "Volumetrics.glsl.inl"
#include "VolumetricMedia.glsl.inl"
#include "VolumetricInject.glsl.inl"
#include "VolumetricIntegrate.glsl.inl"
#include "VolumeRaymarch.glsl.inl"
#include "VolumeRaymarchComposite.glsl.inl"
#include "VolumetricSlice.glsl.inl"

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
		{ "Instancing.glsl", View(k_Instancing_glsl) },
		{ "Renderer3D_Shadow.glsl", View(k_Renderer3D_Shadow_glsl) },
		{ "Renderer3D_ShadowProbe.glsl", View(k_Renderer3D_ShadowProbe_glsl) },
		{ "PostDepthCopy.glsl", View(k_PostDepthCopy_glsl) },
		{ "ParticleCommon.glsl", View(k_ParticleCommon_glsl) },
		{ "ParticleSimulate.glsl", View(k_ParticleSimulate_glsl) },
		{ "ParticleDraw.glsl", View(k_ParticleDraw_glsl) },
		{ "DingoUI.glsl", View(k_DingoUI_glsl) },
		{ "CubeMap.glsl", View(k_CubeMap_glsl) },
		{ "Renderer3D_MeshVertex.glsl", View(k_Renderer3D_MeshVertex_glsl) },
		{ "Renderer3DCamera.glsl", View(k_Renderer3DCamera_glsl) },
		{ "Renderer3DScene.glsl", View(k_Renderer3DScene_glsl) },
		{ "Brdf.glsl", View(k_Brdf_glsl) },
		{ "Ibl.glsl", View(k_Ibl_glsl) },
		{ "Renderer3D_Pbr.glsl", View(k_Renderer3D_Pbr_glsl) },
		{ "Environment.glsl", View(k_Environment_glsl) },
		{ "EnvironmentBake.glsl", View(k_EnvironmentBake_glsl) },
		{ "Skybox.glsl", View(k_Skybox_glsl) },
		{ "Atmosphere.glsl", View(k_Atmosphere_glsl) },
		{ "ProceduralSky.glsl", View(k_ProceduralSky_glsl) },
		{ "VolumetricMath.glsl", View(k_VolumetricMath_glsl) },
		{ "Volumetrics.glsl", View(k_Volumetrics_glsl) },
		{ "VolumetricMedia.glsl", View(k_VolumetricMedia_glsl) },
		{ "VolumetricInject.glsl", View(k_VolumetricInject_glsl) },
		{ "VolumetricIntegrate.glsl", View(k_VolumetricIntegrate_glsl) },
		{ "VolumeRaymarch.glsl", View(k_VolumeRaymarch_glsl) },
		{ "VolumeRaymarchComposite.glsl", View(k_VolumeRaymarchComposite_glsl) },
		{ "VolumetricSlice.glsl", View(k_VolumetricSlice_glsl) },
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

	bool IsEmbeddedEngineShaderSource(std::string_view source)
	{
		for (const EmbeddedShader& shader : s_EmbeddedShaders)
		{
			if (shader.Source == source)
				return true;
		}
		return false;
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
