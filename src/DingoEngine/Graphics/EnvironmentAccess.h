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

		// True while Prepare would record a bake: it has never baked, or (procedural) a SetSky or SetSunDirection
		// change is pending.
		static bool NeedsBake(const Environment* environment) { return !environment->m_Ready || environment->m_SkyDirty; }

		// A procedural environment's sun follows a light: `travelDirection` replaces the baked sun once it is
		// RebakeAngle degrees from it, and marks the sky dirty; a smaller move changes nothing, so a slow sun
		// re-bakes at the threshold rather than every frame. Returns whether it marked the sky dirty.
		static bool SetSunDirection(Environment* environment, const glm::vec3& travelDirection);

		// Test only: turns the multiple-scattering term of the next bake off or on (the closed-form check
		// needs single scattering), and marks the sky dirty.
		static void SetMultipleScattering(Environment* environment, bool enabled);

		// Planet radius over the observer's radius for `sky.Altitude`: the same constants the bake uses, so the
		// sun disc's horizon clip matches the baked sky.
		static float GetGroundRatio(const ProceduralSky& sky);

		// The sky's lookup tables, nullptr for a panorama: transmittance 256 x 64, multiple scattering 32 x 32 and
		// sky view 192 x 108, all RGBA16F. Undefined until IsReady.
		static Texture* GetSkyTransmittance(const Environment* environment) { return environment->m_SkyTransmittance; }
		static Texture* GetSkyMultiScatter(const Environment* environment) { return environment->m_SkyMultiScatter; }
		static Texture* GetSkyView(const Environment* environment) { return environment->m_SkyView; }
	};

}
