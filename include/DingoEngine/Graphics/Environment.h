#pragma once
#include "DingoEngine/Graphics/Texture.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Dingo::Internal
{
	struct EnvironmentAccess;
}

namespace Dingo
{

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

	/// A high-dynamic-range panorama converted to a cube map of linear RGBA16F radiance with a full
	/// mip chain, the source of a skybox and of image-based lighting. Orientation: the panorama's
	/// centre looks down -Z, +X is a quarter turn to its right and the top row is +Y. The caller owns
	/// it, or the AssetManager does for one it loaded; delete it before Renderer::Destroy. The cube is
	/// filled on the GPU by Prepare, inside a frame, which the first scene that uses it also calls.
	class Environment
	{
	public:
		/// Reads the .hdr and keeps its pixels until the first bake; nothing reaches the GPU yet. Returns
		/// nullptr (with an error) when there is no graphics device, as in a headless app, the file is
		/// not a readable Radiance .hdr, or FaceSize is not valid. The caller owns the result.
		[[nodiscard]] static Environment* Create(const EnvironmentParams& params);

		/// Frees the cube map and any pixels not yet baked.
		~Environment();
		/// Not copyable: it owns its cube map.
		Environment(const Environment&) = delete;
		/// Not copyable: it owns its cube map.
		Environment& operator=(const Environment&) = delete;

		/// Bakes the cube map in the frame being recorded; true once it is ready (at once if it
		/// already was). False outside a frame, in a frame that renders nothing (Renderer::IsFrameSkipped),
		/// or when the bake shader failed to compile. Call it from a loading screen to pay the cost
		/// before the first scene that uses the environment.
		bool Prepare();
		/// Whether Prepare has recorded the whole bake, so the cube map holds the panorama and the
		/// irradiance and specular lighting derived from it are filled in.
		bool IsReady() const { return m_Ready; }

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
		void ReleaseEquirect();

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
	};

	/// What Renderer3D::SetEnvironment hands a scene: which Environment to draw as the sky, and how.
	struct SceneEnvironment
	{
		/// The panorama to draw; it must stay alive until the scene's EndScene. Null draws no sky.
		Dingo::Environment* Environment = nullptr;
		/// Linear multiplier on the radiance, 0 or more; 1 shows the panorama as stored.
		float Intensity = 1.0f;
		/// Degrees the sky is turned about +Y: a positive angle carries what was at -Z toward -X.
		float Rotation = 0.0f;
		/// False keeps the environment for lighting without drawing the sky.
		bool DrawSkybox = true;
		/// True lights PBR materials from the environment (diffuse from its irradiance, specular from
		/// its reflections) instead of SceneLights::AmbientColor; false keeps the flat ambient.
		bool Lighting = true;
		/// Linear multiplier on the environment's lighting only, 0 or more; the sky keeps Intensity.
		/// The lighting uses Intensity x LightingIntensity.
		float LightingIntensity = 1.0f;
	};

}
