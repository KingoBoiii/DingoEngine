#pragma once
#include "DingoEngine/Graphics/Texture.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Dingo::Internal
{
	struct EnvironmentAccess;
	struct ProceduralBake;
}

namespace Dingo
{

	class ComputePass;
	class GraphicsBuffer;

	/// Describes an Environment for Environment::Create.
	struct EnvironmentParams
	{
		/// A Radiance .hdr equirectangular panorama, looked up like Texture::CreateFromFile.
		std::filesystem::path Path;
		/// Edge of each cube face in texels: a power of two from 16 to 2048, or 0 for a quarter of the
		/// panorama's width rounded down to a power of two, within 32 to 1024.
		uint32_t FaceSize = 0;
		std::string DebugName = "Environment"; ///< Name shown in graphics debuggers and logs.

		/// Each setter sets the field of the same name and returns *this, for chaining.
		///@{
		EnvironmentParams& SetPath(const std::filesystem::path& path)
		{
			Path = path;
			return *this;
		}

		EnvironmentParams& SetFaceSize(uint32_t faceSize)
		{
			FaceSize = faceSize;
			return *this;
		}

		EnvironmentParams& SetDebugName(const std::string& name)
		{
			DebugName = name;
			return *this;
		}
		///@}
	};

	/// The sun and atmosphere of a procedural sky (Environment::CreateProcedural). The sky is what a clear
	/// planet with Earth's air looks like from `Altitude`, lit by one sun: blue by day, red and orange at
	/// sunrise and sunset, dark at night.
	struct ProceduralSky
	{
		/// World-space direction the sun's light travels, as DirectionalLight::Direction (it need not be
		/// unit length, and must not be zero). The sun sits in the opposite direction.
		glm::vec3 SunDirection{ -0.4f, -1.0f, -0.35f };
		/// Colour of the sunlight above the atmosphere, in sRGB like a colour picker; each channel is 0 or more.
		glm::vec3 SunColor{ 1.0f };
		/// Linear brightness of the sun, 0 or more: a white floor facing an unveiled sun has radiance
		/// SunIntensity. It scales the whole sky.
		float SunIntensity = 2.0f;
		/// Angular radius of the sun's disc in degrees, 0 to 10; 0 draws no disc. The real sun is 0.27, and a
		/// larger disc reads better on a screen.
		float SunDiscRadius = 0.5f;
		/// How much brighter the disc is than SunIntensity times the sunlight that reaches the camera,
		/// 0 or more. Only Renderer3D's skybox draws it: the cube and the lighting derived from it hold
		/// scattered light, never the disc.
		float SunDiscIntensity = 20.0f;
		/// Scales the haze: the aerosols' scattering and absorption, 0 to 50. 1 is a clear day, larger is
		/// hazier and redder at the horizon, 0 is air alone.
		float Turbidity = 1.0f;
		/// How strongly the haze scatters toward the sun, from 0 (evenly) to 0.99 (a tight glow around the
		/// sun); 0.8 is the usual aerosol value.
		float MieAnisotropy = 0.8f;
		/// Scales the density of the air's own scattering, 0 to 100; 1 is Earth's. More makes a bluer, denser sky.
		float RayleighScale = 1.0f;
		/// Scales the ozone layer's absorption, 0 to 100; 1 is Earth's. It keeps the sky blue at sunset.
		float OzoneScale = 1.0f;
		/// Colour of the ground below the horizon, in sRGB, each channel 0 to 1.
		glm::vec3 GroundAlbedo{ 0.3f };
		/// Linear radiance added to the whole sky, 0 or more, so a moonless night is not black.
		glm::vec3 NightColor{ 0.0015f, 0.002f, 0.004f };
		/// Height of the camera over the ground in metres, 0 to 20000; it moves the horizon down.
		float Altitude = 0.0f;
		/// How far in degrees the sun may move before the sky is baked again, 0 to 45; 0 re-bakes on every
		/// change. A smaller angle follows a moving sun more smoothly and costs more.
		float RebakeAngle = 0.25f;
	};

	/// Describes a procedural Environment for Environment::CreateProcedural.
	struct ProceduralEnvironmentParams
	{
		/// Edge of each cube face in texels: a power of two from 16 to 2048. 256 costs about 4 MB; 512 about 17 MB.
		uint32_t FaceSize = 256;
		std::string DebugName = "Procedural Environment"; ///< Name shown in graphics debuggers and logs.
		ProceduralSky Sky; ///< The sky to bake first; it must be valid, as for Environment::SetSky.

		/// Each setter sets the field of the same name and returns *this, for chaining.
		///@{
		ProceduralEnvironmentParams& SetFaceSize(uint32_t faceSize)
		{
			FaceSize = faceSize;
			return *this;
		}

		ProceduralEnvironmentParams& SetDebugName(const std::string& name)
		{
			DebugName = name;
			return *this;
		}

		ProceduralEnvironmentParams& SetSky(const ProceduralSky& sky)
		{
			Sky = sky;
			return *this;
		}
		///@}
	};

	/// A high-dynamic-range panorama, or a procedural sky, converted to a cube map of linear RGBA16F
	/// radiance with a full mip chain, the source of a skybox and of image-based lighting. Orientation: the panorama's
	/// centre looks down -Z, +X is a quarter turn to its right and the top row is +Y. The caller owns
	/// it, or the AssetManager does for one it loaded; delete it before Renderer::Destroy. The cube is
	/// filled on the GPU by Prepare, inside a frame, which the first scene that uses it also calls.
	/// A procedural environment (CreateProcedural) has no panorama: its cube is the sky of a ProceduralSky,
	/// baked again by Prepare whenever SetSky or a scene that follows the sun changes it.
	class Environment
	{
	public:
		/// Reads the .hdr and keeps its pixels until the first bake; nothing reaches the GPU yet. Returns
		/// nullptr (with an error) when there is no graphics device, as in a headless app, the file is
		/// not a readable Radiance .hdr, or FaceSize is not valid. The caller owns the result.
		[[nodiscard]] static Environment* Create(const EnvironmentParams& params);

		/// Makes a sky from `params.Sky`; nothing reaches the GPU until the first Prepare. Returns nullptr
		/// (with an error) when there is no graphics device, FaceSize is not a power of two from 16 to 2048,
		/// or the sky is not valid (see SetSky). The caller owns the result.
		[[nodiscard]] static Environment* CreateProcedural(const ProceduralEnvironmentParams& params);

		/// Frees the cube map and any pixels not yet baked.
		~Environment();
		/// Not copyable: it owns its cube map.
		Environment(const Environment&) = delete;
		/// Not copyable: it owns its cube map.
		Environment& operator=(const Environment&) = delete;

		/// Bakes the cube map in the frame being recorded; true once it is ready (at once if it
		/// already was). False outside a frame, in a frame that renders nothing (Renderer::IsFrameSkipped),
		/// or when the bake shader failed to compile. Call it from a loading screen to pay the cost
		/// before the first scene that uses the environment. A procedural environment with a pending
		/// SetSky change bakes again, at most once in a frame, and stays ready throughout.
		bool Prepare();
		/// Whether Prepare has recorded the whole bake, so the cube map holds the panorama and the
		/// irradiance and specular lighting derived from it are filled in. A procedural environment stays
		/// ready while it waits for a re-bake.
		bool IsReady() const { return m_Ready; }

		/// Whether this environment was made by CreateProcedural.
		bool IsProcedural() const { return m_Procedural; }
		/// Replaces the sky of a procedural environment; the next Prepare bakes it, once the sun has moved
		/// by RebakeAngle or any other field has changed. Returns false and keeps the old sky when a field is
		/// not finite or is out of the range its declaration gives (with a warning, once), or when this
		/// environment is not procedural.
		bool SetSky(const ProceduralSky& sky);
		/// The sky last accepted, with SunDirection as last accepted (FollowSun updates it before the bake); nullptr for a panorama.
		const ProceduralSky* GetSky() const { return m_Procedural ? &m_Sky : nullptr; }
		/// The fraction of the sun's light that reaches the camera when it travels along `travelDirection`
		/// (as DirectionalLight::Direction), per linear colour channel: near 1 at noon, red at sunset, and
		/// falling to 0 over the first degree below the horizon. {1, 1, 1} for a panorama.
		glm::vec3 GetSunTransmittance(const glm::vec3& travelDirection) const;

		/// The cube map: RGBA16F, GetMipLevelCount() mips, linear radiance. Owned by the Environment;
		/// its contents are undefined until IsReady.
		Texture* GetCubeMap() const { return m_Cube; }
		/// Edge of each cube face in texels.
		uint32_t GetFaceSize() const { return m_FaceSize; }
		/// Mip levels of the cube map: the full chain down to 1 x 1.
		uint32_t GetMipLevelCount() const { return m_MipLevels; }
		/// Never reused, unlike the address.
		uint64_t GetId() const { return m_Id; }

	private:
		Environment() = default;
		bool Build(std::vector<uint16_t>&& halves, uint32_t width, uint32_t height, uint32_t faceSize, const std::string& debugName);
		bool BuildProcedural(const ProceduralSky& sky, uint32_t faceSize, const std::string& debugName);
		void ReleaseEquirect();
		bool PrepareHdr();
		bool PrepareProcedural();
		bool RecordDerivedStages(std::vector<ComputePass*>& passes, std::vector<ComputePass*>& transient, bool reuse, const char*& stage);

		friend struct Internal::EnvironmentAccess;

	private:
		Texture* m_Cube = nullptr;
		Texture* m_Equirect = nullptr;
		Texture* m_Specular = nullptr;
		GraphicsBuffer* m_Irradiance = nullptr;
		uint64_t m_BakeGeneration = 0;
		std::vector<uint16_t> m_Halves;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		uint32_t m_FaceSize = 0;
		uint32_t m_MipLevels = 0;
		uint64_t m_Id = 0;
		bool m_Ready = false;
		bool m_ReportedFailure = false;
		bool m_BakeFailed = false;
		Texture* m_SkyTransmittance = nullptr;
		Texture* m_SkyMultiScatter = nullptr;
		Texture* m_SkyView = nullptr;
		Internal::ProceduralBake* m_SkyBake = nullptr;
		ProceduralSky m_Sky;
		uint64_t m_LastBakeFrame = 0;
		bool m_Procedural = false;
		bool m_SkyDirty = false;
		bool m_LutsDirty = true;
		bool m_MultiScatter = true;
		bool m_SkyWarned = false;
	};

	/// What Renderer3D::SetEnvironment hands a scene: which Environment to draw as the sky, and how.
	struct SceneEnvironment
	{
		/// The panorama or procedural sky to draw; it must stay alive until the scene's EndScene. Null draws no sky.
		Dingo::Environment* Environment = nullptr;
		/// Linear multiplier on the radiance, 0 or more; 1 shows the panorama as stored.
		float Intensity = 1.0f;
		/// Degrees the sky is turned about +Y: a positive angle carries what was at -Z toward -X. A procedural
		/// environment ignores it: its sun and the lights must keep agreeing.
		float Rotation = 0.0f;
		/// False keeps the environment for lighting without drawing the sky.
		bool DrawSkybox = true;
		/// True lights PBR materials from the environment (diffuse from its irradiance, specular from
		/// its reflections) instead of SceneLights::AmbientColor; false keeps the flat ambient.
		bool Lighting = true;
		/// Linear multiplier on the environment's lighting only, 0 or more; the sky keeps Intensity.
		/// The lighting uses Intensity x LightingIntensity.
		float LightingIntensity = 1.0f;
		/// For a procedural environment, true points its sun along the scene's main directional light (the
		/// one that casts shadows, else the first) and bakes the sky again when it moves; false keeps the
		/// sun it has. Ignored for a panorama.
		bool FollowSun = true;
		/// For a procedural environment, true multiplies the main directional light's colour by the
		/// transmittance of the atmosphere (Environment::GetSunTransmittance), so lit surfaces, fog and
		/// shafts redden at sunset and go dark at night; it needs FollowSun to agree with the sky.
		/// Ignored for a panorama.
		bool TintSun = true;
	};

}
