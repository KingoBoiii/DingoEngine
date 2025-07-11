#include "depch.h"
#include "NvrhiTexture.h"

#include "DingoEngine/Core/FileSystem.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

namespace DingoEngine
{

	namespace Utils
	{

		static nvrhi::Format GetTextureFormat(const TextureFormat format)
		{
			switch (format)
			{
				case TextureFormat::RGBA: return nvrhi::Format::RGBA8_UNORM;
				case TextureFormat::RGB: return nvrhi::Format::RGBA8_UNORM;
				default: break;
			}

			return nvrhi::Format::UNKNOWN;
		}

		static nvrhi::TextureDimension GetTextureDimension(const TextureDimension dimension)
		{
			switch (dimension)
			{
				case TextureDimension::Texture1D: return nvrhi::TextureDimension::Texture1D;
				case TextureDimension::Texture2D: return nvrhi::TextureDimension::Texture2D;
				case TextureDimension::Texture3D: return nvrhi::TextureDimension::Texture3D;
				default: break;
			}

			return nvrhi::TextureDimension::Unknown;
		}

		static uint32_t GetImageFormatBPP(TextureFormat format)
		{
			switch (format)
			{
				case TextureFormat::RGB: return 3;
				case TextureFormat::RGBA: return 4;
			}
			return 0;
		}

		static uint32_t GetImageMemoryRowPitch(TextureFormat format, uint32_t width)
		{
			return width * GetImageFormatBPP(format);
		}

	}

	void NvrhiTexture::Initialize()
	{
		nvrhi::TextureDesc textureDesc = nvrhi::TextureDesc()
			.setDebugName(m_Params.DebugName)
			.setWidth(m_Params.Width)
			.setHeight(m_Params.Height)
			.setFormat(Utils::GetTextureFormat(m_Params.Format))
			.setDimension(Utils::GetTextureDimension(m_Params.Dimension))
			.setDepth(1)
			.setMipLevels(1)
			.setArraySize(1)
			.setInitialState(nvrhi::ResourceStates::ShaderResource)
			.setKeepInitialState(true);

		m_Handle = GraphicsContext::GetDeviceHandle()->createTexture(textureDesc);

		//nvrhi::SamplerDesc samplerDesc = nvrhi::SamplerDesc()
		//	.setAllAddressModes(nvrhi::SamplerAddressMode::ClampToEdge)
		//	.setMinFilter(true)
		//	.setMagFilter(true)
		//	.setMipFilter(true);

		//m_SamplerHandle = GraphicsContext::GetDeviceHandle()->createSampler(samplerDesc);
	}

	void NvrhiTexture::Destroy()
	{
		if (m_SamplerHandle)
		{
			m_SamplerHandle->Release();
		}

		if (m_Handle)
		{
			m_Handle->Release();
		}
	}

	void NvrhiTexture::Upload(const void* data, uint64_t size)
	{
		DE_CORE_ASSERT(data);

		nvrhi::CommandListParameters commandListParameters = nvrhi::CommandListParameters()
			.setQueueType(nvrhi::CommandQueue::Graphics);

		nvrhi::CommandListHandle commandList = GraphicsContext::GetDeviceHandle()->createCommandList(commandListParameters);

		commandList->open();

		commandList->writeTexture(m_Handle, 0, 0, data, size);

		commandList->close();

		GraphicsContext::GetDeviceHandle()->executeCommandList(commandList);
	}

	void NvrhiTexture::Upload(const std::filesystem::path& filepath)
	{
		uint32_t width, height, channels;
		const uint8_t* data = FileSystem::ReadImage(filepath, &width, &height, &channels, true, true);
		DE_CORE_ASSERT(data, "Failed to load texture");

		nvrhi::CommandListParameters commandListParameters = nvrhi::CommandListParameters()
			.setQueueType(nvrhi::CommandQueue::Graphics);

		nvrhi::CommandListHandle commandList = GraphicsContext::GetDeviceHandle()->createCommandList(commandListParameters);

		commandList->open();

		channels = 4;

		commandList->writeTexture(m_Handle, 0, 0, data, width * channels, width * height * channels);

		commandList->close();

		GraphicsContext::GetDeviceHandle()->executeCommandList(commandList);
	}

}
