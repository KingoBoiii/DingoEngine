#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Dingo
{

	// Textures beyond one 2D image: cube maps, mip chains, 3D textures and HDR files, through
	// Texture::UploadSubresource/ReadSubresource/CreateFromHdrFile and ComputePass::SetTexture/
	// SetStorageTexture with a mip. On start it checks by readback that
	//  1. a cube of 16 x 16 RGBA8 and RGBA16F with five mips round-trips all 30 subresources exactly;
	//  2. a pixel shader sampling that cube by direction (DE_CubeFaceDirection) finds each texel, at mips 0 and 2;
	//  3. a compute shader writes every mip of a storage cube through image2DArray, and the same
	//     sampling then sees it;
	//  4. a 32 x 16 RGBA32F mip chain is built by compute passes that read mip N - 1 and write mip N;
	//  5. a compute shader writes a 16 x 16 x 8 RGBA16F texture through image3D, per-slice readback
	//     matches, texelFetch reads it back and linear sampling blends between slices;
	//  6. a material's binding set sees what a compute pass wrote between two draws;
	//  7. a 4 x 2 RGBE .hdr written by the test loads as RGBA16F (clamped) and RGBA32F.
	class TextureSubresourceTest : public GraphicsTest
	{
	public:
		TextureSubresourceTest() = default;
		virtual ~TextureSubresourceTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

	private:
		using SubresourceCheck = std::function<bool(uint32_t mip, uint32_t layer, const TexturePixels&)>;

		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }
		void ReadSubresources(Texture* texture, uint32_t mips, uint32_t layers, SubresourceCheck verify, const std::string& name);
		void ReadBack(Texture* texture, std::function<void(const TexturePixels&)> check);
		void DrawTiles(Material* material, Framebuffer* target, const float (&push)[4]);

		void RunChecks();
		void CheckCubeRoundTrip();
		void CheckCubeSampling();
		void CheckCubeCompute();
		void CheckMipChain();
		void CheckVolume();
		void CheckRebinding();
		void CheckHdr();

	private:
		TestChecks m_Checks;
		bool m_ChecksDone = false;
		std::shared_ptr<int> m_Alive;
		std::filesystem::path m_HdrFolder;

		static constexpr uint32_t k_CubeSize = 16;
		static constexpr uint32_t k_CubeMips = 5;
		static constexpr uint32_t k_ChainWidth = 32;
		static constexpr uint32_t k_ChainHeight = 16;
		static constexpr uint32_t k_ChainMips = 6;
		static constexpr uint32_t k_ProbeMips = k_ChainMips - 1;
		static constexpr uint32_t k_ProbeWidth = 16;
		static constexpr uint32_t k_ProbeRows = 8;
		static constexpr uint32_t k_VolumeSize = 16;
		static constexpr uint32_t k_VolumeDepth = 8;
		static constexpr uint32_t k_BindSize = 8;

		Texture* m_UploadedCube = nullptr;
		Texture* m_HalfCube = nullptr;
		Texture* m_ComputeCube = nullptr;
		Texture* m_Chain = nullptr;
		Texture* m_ChainProbe = nullptr;
		Texture* m_Volume = nullptr;
		Texture* m_Bound = nullptr;
		Texture* m_HdrHalf = nullptr;
		Texture* m_HdrFloat = nullptr;

		Shader* m_CubeWriteShader = nullptr;
		Shader* m_CubeSampleShader = nullptr;
		Shader* m_ChainShader = nullptr;
		Shader* m_ChainProbeShader = nullptr;
		Shader* m_VolumeWriteShader = nullptr;
		Shader* m_VolumeSampleShader = nullptr;
		Shader* m_BindFillShader = nullptr;
		Shader* m_BindSampleShader = nullptr;

		ComputePass* m_CubeWritePasses[k_CubeMips] = {};
		ComputePass* m_ChainPasses[k_ChainMips - 1] = {};
		ComputePass* m_ChainProbePass = nullptr;
		ComputePass* m_VolumeWritePass = nullptr;
		ComputePass* m_BindFillPass = nullptr;

		Material* m_UploadedCubeMaterial = nullptr;
		Material* m_ComputeCubeMaterial = nullptr;
		Material* m_VolumeMaterial = nullptr;
		Material* m_BindMaterial = nullptr;

		Framebuffer* m_UploadedMip0 = nullptr;
		Framebuffer* m_UploadedMip2 = nullptr;
		Framebuffer* m_ComputeMip0 = nullptr;
		Framebuffer* m_ComputeMip2 = nullptr;
		Framebuffer* m_VolumeFetch = nullptr;
		Framebuffer* m_VolumeLinear = nullptr;
		Framebuffer* m_BindTargets[3] = {};
	};

}
