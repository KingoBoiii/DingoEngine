#include "depch.h"
#include "NvrhiTexture.h"

#include "DingoEngine/Graphics/GraphicsContext.h"

//#define STB_IMAGE_IMPLEMENTATION
//#include <stb_image.h>

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
			.setInitialState(nvrhi::ResourceStates::ShaderResource)
			.setKeepInitialState(true);

		m_Handle = GraphicsContext::GetDeviceHandle()->createTexture(textureDesc);
	}

	void NvrhiTexture::Destroy()
	{
		if (m_Handle)
		{
			m_Handle->Release();
		}
	}

	void NvrhiTexture::Upload(const void* data, uint64_t size)
	{
		//int32_t width, height, channels;
		//uint8_t* imageData = stbi_load("assets/textures/dickbutt.png", &width, &height, &channels, STBI_rgb_alpha);
		//DE_CORE_ASSERT(imageData, "Failed to load texture");

		//nvrhi::CommandListParameters commandListParameters = nvrhi::CommandListParameters()
		//	.setQueueType(nvrhi::CommandQueue::Graphics);

		//nvrhi::CommandListHandle commandList = GraphicsContext::GetDeviceHandle()->createCommandList(commandListParameters);

		//commandList->open();

		//commandList->writeTexture(m_Handle, 0, 0, imageData, width * channels, height * (width * channels));

		//commandList->close();

		//GraphicsContext::GetDeviceHandle()->executeCommandList(commandList);
	}

}
