#include "depch.h"
#include "NvrhiTexture.h"

#include "DingoEngine/Core/FileSystem.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "NvrhiGraphicsContext.h"

namespace Dingo
{

	namespace Utils
	{

		static nvrhi::Format GetTextureFormat(const TextureFormat format)
		{
			switch (format)
			{
				case TextureFormat::RGBA: return nvrhi::Format::RGBA8_UNORM;
				case TextureFormat::RGB: return nvrhi::Format::RGBA8_UNORM;

				case TextureFormat::RGBA8_UNORM: return nvrhi::Format::RGBA8_UNORM;
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

		static nvrhi::SamplerAddressMode GetSamplerAddressMode(const TextureWrapMode wrapMode)
		{
			switch (wrapMode)
			{
				case TextureWrapMode::Repeat: return nvrhi::SamplerAddressMode::Repeat;
				case TextureWrapMode::MirroredRepeat: return nvrhi::SamplerAddressMode::MirroredRepeat;
				case TextureWrapMode::ClampToEdge: return nvrhi::SamplerAddressMode::ClampToEdge;
				case TextureWrapMode::ClampToBorder: return nvrhi::SamplerAddressMode::ClampToBorder;
				case TextureWrapMode::MirrorClampToEdge: return nvrhi::SamplerAddressMode::MirrorClampToEdge;
				default: break;
			}
			return nvrhi::SamplerAddressMode::ClampToEdge; // Default to ClampToEdge if unknown
		}

		static uint32_t GetImageFormatBPP(TextureFormat format)
		{
			switch (format)
			{
				case TextureFormat::RGB: return 3;
				case TextureFormat::RGBA: 
				case TextureFormat::RGBA8_UNORM: 
					return 4;
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
			.setIsRenderTarget(m_Params.IsRenderTarget)
			.setKeepInitialState(true);

		m_Handle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createTexture(textureDesc);

#ifdef ENABLE_TEXTURE_SAMPLER
		nvrhi::SamplerDesc samplerDesc = nvrhi::SamplerDesc()
			.setAllAddressModes(Utils::GetSamplerAddressMode(m_Params.WrapMode))
			.setMinFilter(true)
			.setMagFilter(true)
			.setMipFilter(true);

		m_SamplerHandle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createSampler(samplerDesc);
#endif
	}

	void NvrhiTexture::Destroy()
	{
#ifdef ENABLE_TEXTURE_SAMPLER
		if (m_SamplerHandle)
		{
			m_SamplerHandle->Release();
		}
#endif

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

		nvrhi::CommandListHandle commandList = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createCommandList(commandListParameters);

		commandList->open();

		commandList->writeTexture(m_Handle, 0, 0, data, size);

		commandList->close();

		GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->executeCommandList(commandList);
	}

	void NvrhiTexture::Upload(const std::filesystem::path& filepath)
	{
		uint32_t width, height, channels;
		const uint8_t* data = FileSystem::ReadImage(filepath, &width, &height, &channels, true, true);
		DE_CORE_ASSERT(data, "Failed to load texture");

		nvrhi::CommandListParameters commandListParameters = nvrhi::CommandListParameters()
			.setQueueType(nvrhi::CommandQueue::Graphics);

		nvrhi::CommandListHandle commandList = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createCommandList(commandListParameters);

		commandList->open();

		channels = 4;

		commandList->writeTexture(m_Handle, 0, 0, data, width * channels, width * height * channels);

		commandList->close();

		GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->executeCommandList(commandList);
	}

}
