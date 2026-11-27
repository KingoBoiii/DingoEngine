#pragma once
#include "DingoEngine/Graphics/Environment.h"

namespace Dingo::Internal
{

	constexpr uint32_t k_IblFaceSize = 128;
	constexpr uint32_t k_IblMipLevels = 6;
	constexpr uint64_t k_IblIrradianceBytes = 9 * 4 * sizeof(float);

	struct EnvironmentAccess
	{
		// Reads a Radiance .hdr through the VFS into RGBA16F halves, top row last as the loader leaves
		// them. No graphics device needed, so a loader thread may call it. Logs and returns false on failure.
		static bool DecodeHdr(const std::filesystem::path& path, std::vector<uint16_t>& halves, uint32_t& width, uint32_t& height);

		// width x height RGBA16F texels, top row last as the loader leaves them. nullptr, with an
		// error, for a bad face size or when the textures can't be made.
		static Environment* Create(std::vector<uint16_t>&& halves, uint32_t width, uint32_t height, uint32_t faceSize, const std::string& debugName);

		// Replaces the pixels in place: the cube keeps its Texture::GetId and the environment must be
		// baked again. The previous content stays when it fails.
		static bool Reload(Environment* environment, std::vector<uint16_t>&& halves, uint32_t width, uint32_t height, uint32_t faceSize);

		// RGBA16F cube, face 128 and k_PrefilterMips mips: mip m holds the radiance prefiltered with GGX at
		// roughness m / (k_PrefilterMips - 1). Undefined until IsReady.
		static Texture* GetSpecular(const Environment* environment) { return environment->m_Specular; }

		// 9 vec4 (144 B) of irradiance / pi as spherical-harmonic coefficients (rgb used), written by the
		// bake: diffuse = diffuseColor * sum(c_i * Y_i(n)). Undefined until IsReady.
		static GraphicsBuffer* GetIrradiance(const Environment* environment) { return environment->m_Irradiance; }

		// Counts completed bakes, so a consumer's copy of the specular cube and irradiance goes stale when
		// it changes (a reload re-bakes and bumps it).
		static uint64_t GetBakeGeneration(const Environment* environment) { return environment->m_BakeGeneration; }
	};

}
