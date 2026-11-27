#include "depch.h"
#include "DingoEngine/Graphics/Environment.h"
#include "DingoEngine/Asset/AssetPath.h"
#include "DingoEngine/Core/FileReaders.h"
#include "DingoEngine/Core/FileSystem.h"
#include "DingoEngine/Core/PathText.h"
#include "DingoEngine/Graphics/ComputePass.h"
#include "DingoEngine/Graphics/EnvironmentAccess.h"
#include "DingoEngine/Graphics/EnvironmentLayout.h"
#include "DingoEngine/Graphics/EnvironmentShaders.h"
#include "DingoEngine/Graphics/GraphicsDevice.h"
#include "DingoEngine/Graphics/HalfFloat.h"
#include "DingoEngine/Graphics/Renderer.h"

#include <algorithm>
#include <atomic>
#include <cctype>

namespace Dingo
{

	namespace
	{
		uint64_t AllocateEnvironmentId()
		{
			static std::atomic<uint64_t> s_NextId{ 1 };
			return s_NextId.fetch_add(1, std::memory_order_relaxed);
		}

		bool HasHdrExtension(const std::filesystem::path& path)
		{
			std::string extension = Internal::PathToUtf8(path.extension());
			std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return extension == ".hdr";
		}

		TextureParams CubeParams(uint32_t faceSize, const std::string& debugName)
		{
			return TextureParams()
				.SetDebugName(debugName + " (cube)")
				.SetWidth(faceSize)
				.SetHeight(faceSize)
				.SetMipLevels(GetMipLevelCount(faceSize, faceSize))
				.SetFormat(TextureFormat::RGBA16F)
				.SetDimension(TextureDimension::TextureCube)
				.SetIsStorage(true);
		}

		Texture* CreateEquirect(uint32_t width, uint32_t height, const std::string& debugName)
		{
			return Texture::Create(TextureParams()
				.SetDebugName(debugName + " (equirect)")
				.SetWidth(width)
				.SetHeight(height)
				.SetFormat(TextureFormat::RGBA16F)
				.SetDimension(TextureDimension::Texture2D));
		}
	}

	Environment* Environment::Create(const EnvironmentParams& params)
	{
		if (!Internal::RequireGraphicsDevice("Environment::Create"))
			return nullptr;

		if (!HasHdrExtension(params.Path))
		{
			DE_CORE_ERROR("Environment::Create: '{}' is not a .hdr file.", Internal::PathToUtf8(params.Path));
			return nullptr;
		}
		if (params.FaceSize != 0 && Internal::ChooseEnvironmentFaceSize(1, params.FaceSize) == 0)
		{
			DE_CORE_ERROR("Environment::Create: '{}': FaceSize {} is not a power of two from {} to {}.", params.DebugName, params.FaceSize, Internal::k_EnvironmentFaceMin, Internal::k_EnvironmentFaceMax);
			return nullptr;
		}

		std::vector<uint16_t> halves;
		uint32_t width = 0, height = 0;
		if (!Internal::EnvironmentAccess::DecodeHdr(params.Path, halves, width, height))
			return nullptr;

		const uint32_t faceSize = Internal::ChooseEnvironmentFaceSize(width, params.FaceSize);
		return Internal::EnvironmentAccess::Create(std::move(halves), width, height, faceSize, params.DebugName);
	}

	Environment::~Environment()
	{
		DestroyAndDelete(m_Cube);
		ReleaseEquirect();
	}

	bool Environment::Build(std::vector<uint16_t>&& halves, uint32_t width, uint32_t height, uint32_t faceSize, const std::string& debugName)
	{
		if (!Internal::RequireGraphicsDevice("Environment") || width == 0 || height == 0 || halves.size() != static_cast<size_t>(width) * height * 4)
			return false;
		if (faceSize == 0 || Internal::ChooseEnvironmentFaceSize(width, faceSize) != faceSize)
		{
			DE_CORE_ERROR("Environment '{}': face size {} is not a power of two from {} to {}.", debugName, faceSize, Internal::k_EnvironmentFaceMin, Internal::k_EnvironmentFaceMax);
			return false;
		}

		m_Cube = Texture::Create(CubeParams(faceSize, debugName));
		m_Equirect = CreateEquirect(width, height, debugName);
		if (!m_Cube || !m_Equirect)
			return false;

		m_Halves = std::move(halves);
		m_Width = width;
		m_Height = height;
		m_FaceSize = faceSize;
		m_MipLevels = Dingo::GetMipLevelCount(faceSize, faceSize);
		m_Id = AllocateEnvironmentId();
		m_Ready = false;
		return true;
	}

	void Environment::ReleaseEquirect()
	{
		DestroyAndDelete(m_Equirect);
	}

	bool Environment::Prepare()
	{
		if (m_Ready)
			return true;
		if (m_BakeFailed || !m_Cube || !m_Equirect || Renderer::IsFrameSkipped() || !Renderer::TryGetRecordingCommandList())
			return false;

		Shader* bake = Internal::GetEnvironmentShader(Internal::EnvironmentShaderVariant::EquirectBake);
		Shader* downsample = Internal::GetEnvironmentShader(Internal::EnvironmentShaderVariant::Downsample);
		Sampler* equirectSampler = Internal::GetEquirectSampler();
		if (!bake || !downsample || !equirectSampler)
		{
			if (!m_ReportedFailure)
			{
				m_ReportedFailure = true;
				DE_CORE_ERROR("Environment::Prepare: the bake shaders are not valid; the environment stays unbaked.");
			}
			return false;
		}

		DE_PROFILE_SCOPE("Environment::Prepare");
		Renderer::BeginGpuTimer("Environment");
		m_Equirect->UploadSubresource(m_Halves.data(), 0, 0);

		std::vector<ComputePass*> passes;
		auto groups = [](uint32_t size) { return (size + 7u) / 8u; };

		ComputePass* equirectPass = ComputePass::Create(ComputePassParams().SetDebugName("Environment equirect bake").SetShader(bake));
		passes.push_back(equirectPass);
		equirectPass->SetTexture(1, m_Equirect);
		equirectPass->SetSampler(2, equirectSampler);
		equirectPass->SetStorageTexture(3, m_Cube, 0);
		uint32_t push[4] = { m_FaceSize, 0, 0, 0 };
		equirectPass->SetPushConstants(push, sizeof(push));
		bool recorded = Renderer::Dispatch(equirectPass, groups(m_FaceSize), groups(m_FaceSize), 6);

		for (uint32_t mip = 1; recorded && mip < m_MipLevels; ++mip)
		{
			const uint32_t size = (std::max)(m_FaceSize >> mip, 1u);
			ComputePass* pass = ComputePass::Create(ComputePassParams().SetDebugName("Environment downsample").SetShader(downsample));
			passes.push_back(pass);
			pass->SetTexture(1, m_Cube, mip - 1, 1);
			pass->SetSampler(2, Renderer::GetClampSampler());
			pass->SetStorageTexture(3, m_Cube, mip);
			push[0] = size;
			pass->SetPushConstants(push, sizeof(push));
			recorded = Renderer::Dispatch(pass, groups(size), groups(size), 6);
		}

		Renderer::EndGpuTimer();

		Texture* equirect = nullptr;
		if (recorded)
		{
			equirect = m_Equirect;
			m_Equirect = nullptr;
			m_Ready = true;
			m_Halves = {};
		}
		else
		{
			m_BakeFailed = true;
			DE_CORE_ERROR("Environment::Prepare: recording the bake failed; the environment stays unbaked.");
		}

		Renderer::RunAfterFrame([equirect, passes = std::move(passes)]() mutable
		{
			for (ComputePass*& pass : passes)
				DestroyAndDelete(pass);
			DestroyAndDelete(equirect);
		});
		return recorded;
	}

	bool Internal::EnvironmentAccess::DecodeHdr(const std::filesystem::path& path, std::vector<uint16_t>& halves, uint32_t& width, uint32_t& height)
	{
		const std::filesystem::path resolvedPath = Internal::ResolveRawAssetPath(path);
		const std::optional<std::vector<uint8_t>> bytes = FileSystem::ReadFileBytes(resolvedPath);
		if (!bytes || bytes->empty())
		{
			DE_CORE_ERROR("Environment: failed to read '{}'.", Internal::PathToUtf8(resolvedPath));
			return false;
		}
		if (!Internal::IsHdrImage(bytes->data(), bytes->size()))
		{
			DE_CORE_ERROR("Environment: '{}' is not a Radiance .hdr image.", Internal::PathToUtf8(resolvedPath));
			return false;
		}

		const float* pixels = Internal::ReadImageHdrFromMemory(bytes->data(), bytes->size(), &width, &height, true);
		if (!pixels)
			return false;

		halves.resize(static_cast<size_t>(width) * height * 4);
		for (size_t i = 0; i < halves.size(); ++i)
			halves[i] = Internal::FloatToHalf(pixels[i]);
		Internal::FreeImageHdr(pixels);
		return true;
	}

	Environment* Internal::EnvironmentAccess::Create(std::vector<uint16_t>&& halves, uint32_t width, uint32_t height, uint32_t faceSize, const std::string& debugName)
	{
		Environment* environment = new Environment();
		if (!environment->Build(std::move(halves), width, height, faceSize, debugName))
		{
			delete environment;
			return nullptr;
		}
		return environment;
	}

	bool Internal::EnvironmentAccess::Reload(Environment* environment, std::vector<uint16_t>&& halves, uint32_t width, uint32_t height, uint32_t faceSize)
	{
		if (!environment || !environment->m_Cube || width == 0 || height == 0 || halves.size() != static_cast<size_t>(width) * height * 4)
			return false;
		if (faceSize == 0 || ChooseEnvironmentFaceSize(width, faceSize) != faceSize)
			return false;

		Texture* equirect = CreateEquirect(width, height, "Environment");
		if (!equirect)
			return false;

		TextureParams cube = environment->m_Cube->GetParams();
		cube.SetWidth(faceSize).SetHeight(faceSize).SetMipLevels(GetMipLevelCount(faceSize, faceSize));
		environment->m_Cube->Reinitialize(cube);
		if (environment->m_Cube->GetParams().Width != faceSize)
		{
			DestroyAndDelete(equirect);
			return false;
		}

		environment->ReleaseEquirect();
		environment->m_Equirect = equirect;
		environment->m_Halves = std::move(halves);
		environment->m_Width = width;
		environment->m_Height = height;
		environment->m_FaceSize = faceSize;
		environment->m_MipLevels = cube.MipLevels;
		environment->m_Ready = false;
		environment->m_ReportedFailure = false;
		environment->m_BakeFailed = false;
		return true;
	}

}
