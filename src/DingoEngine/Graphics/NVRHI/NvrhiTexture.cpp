#include "depch.h"
#include "NvrhiTexture.h"

#include "DingoEngine/Core/FileSystem.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "NvrhiCommandList.h"
#include "NvrhiGraphicsContext.h"

#include <cstring>

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

				case TextureFormat::RGBA32F: return nvrhi::Format::RGBA32_FLOAT;

				case TextureFormat::RGBA16F: return nvrhi::Format::RGBA16_FLOAT;
				case TextureFormat::R11G11B10F: return nvrhi::Format::R11G11B10_FLOAT;
				case TextureFormat::R8: return nvrhi::Format::R8_UNORM;
				case TextureFormat::R16F: return nvrhi::Format::R16_FLOAT;
				case TextureFormat::R32F: return nvrhi::Format::R32_FLOAT;
				case TextureFormat::D32: return nvrhi::Format::D32;

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

		static uint32_t GetImageMemoryRowPitch(TextureFormat format, uint32_t width)
		{
			return width * GetBytesPerPixel(format);
		}

		static TextureFormat GetTextureFormat(nvrhi::Format format)
		{
			switch (format)
			{
				case nvrhi::Format::RGBA8_UNORM: return TextureFormat::RGBA8_UNORM;
				case nvrhi::Format::RGBA32_FLOAT: return TextureFormat::RGBA32F;
				case nvrhi::Format::RGBA16_FLOAT: return TextureFormat::RGBA16F;
				case nvrhi::Format::R11G11B10_FLOAT: return TextureFormat::R11G11B10F;
				case nvrhi::Format::R8_UNORM: return TextureFormat::R8;
				case nvrhi::Format::R16_FLOAT: return TextureFormat::R16F;
				case nvrhi::Format::R32_FLOAT: return TextureFormat::R32F;
				default: return TextureFormat::Unknown;
			}
		}

	}

	void NvrhiTexture::Initialize()
	{
		const bool depth = m_Params.Format == TextureFormat::D32;
		nvrhi::TextureDesc textureDesc = nvrhi::TextureDesc()
			.setDebugName(m_Params.DebugName)
			.setWidth(m_Params.Width)
			.setHeight(m_Params.Height)
			.setFormat(Utils::GetTextureFormat(m_Params.Format))
			.setDimension(Utils::GetTextureDimension(m_Params.Dimension))
			.setDepth(1)
			.setMipLevels(1)
			.setArraySize(1)
			.setInitialState(depth ? nvrhi::ResourceStates::DepthWrite : nvrhi::ResourceStates::ShaderResource)
			.setIsRenderTarget(m_Params.IsRenderTarget || depth)
			.setIsUAV(m_Params.IsStorage && !depth)
			.setKeepInitialState(true);

		// D3D can't put a shader-resource view on a D32 resource: a sampled depth is created
		// R32_TYPELESS (NVRHI derives the D32 depth view and the R32 SRV), and one that isn't sampled
		// must say so or creating it fails.
		if (depth)
		{
			textureDesc.isShaderResource = m_Params.IsShaderResource;
			textureDesc.isTypeless = m_Params.IsShaderResource;
		}

		m_Handle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createTexture(textureDesc);
	}

	void NvrhiTexture::Destroy()
	{
		m_Handle = nullptr;
	}

	void NvrhiTexture::Reinitialize(const TextureParams& params)
	{
		m_Params = params;
		m_Handle = nullptr; // NVRHI frees the old texture once in-flight frames release it
		Initialize();

		// Initialize() produced a different nvrhi::ITexture — tell cached binding sets.
		m_Generation = NextGeneration();

		if (m_Params.InitialData)
		{
			Upload(m_Params.InitialData, Utils::GetImageMemoryRowPitch(m_Params.Format, m_Params.Width));
			m_Params.InitialData = nullptr; // the caller's buffer is not retained
		}
	}

	void NvrhiTexture::Upload(const void* data, uint64_t size)
	{
		DE_CORE_ASSERT(data);

		// Join the frame's command list whenever one is open. A list of our own would be
		// wrong mid-frame on D3D11: NVRHI's open() and close() each call ClearState() on the
		// immediate context, unbinding the render target that the frame's list still believes
		// is set, and it does not re-bind because its own graphics state looks unchanged.
		// The async asset pump runs before any layer draws so it never noticed, but the
		// Assets panel's Reload button runs after every layer has recorded its draws.
		if (CommandList* frameList = Renderer::TryGetRecordingCommandList())
		{
			frameList->UploadTexture(this, data, size);
			return;
		}

		// Outside a frame (asset loads during OnAttach, the white texture during
		// Renderer::Initialize) there is no list to join, so open a short-lived one.
		nvrhi::CommandListParameters commandListParameters = nvrhi::CommandListParameters()
			.setQueueType(nvrhi::CommandQueue::Graphics);

		nvrhi::CommandListHandle commandList = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createCommandList(commandListParameters);

		commandList->open();

		commandList->writeTexture(m_Handle, 0, 0, data, size);

		commandList->close();

		GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->executeCommandList(commandList);
	}

	void NvrhiTexture::ReadPixels(std::function<void(const TexturePixels&)> done)
	{
		const TextureFormat format = m_Handle ? Utils::GetTextureFormat(m_Handle->getDesc().format) : TextureFormat::Unknown;
		if (format == TextureFormat::Unknown || m_Handle->getDesc().dimension != nvrhi::TextureDimension::Texture2D)
		{
			DE_CORE_ERROR("Texture::ReadPixels: '{}' isn't a 2D colour texture of a format it reads (RGBA8, RGBA16F, RGBA32F, R11G11B10F, R8, R16F, R32F).", m_Params.DebugName);
			done(TexturePixels{});
			return;
		}

		nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();
		const uint32_t width = m_Handle->getDesc().width;
		const uint32_t height = m_Handle->getDesc().height;

		nvrhi::StagingTextureHandle staging = device->createStagingTexture(nvrhi::TextureDesc()
			.setDebugName(m_Params.DebugName + " (readback)")
			.setWidth(width)
			.setHeight(height)
			.setFormat(m_Handle->getDesc().format)
			.setDimension(nvrhi::TextureDimension::Texture2D), nvrhi::CpuAccessMode::Read);

		// Mapping waits for the GPU to finish the copy, on every backend.
		auto resolve = [staging, width, height, format, done = std::move(done)]()
		{
			nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();
			TexturePixels pixels;
			pixels.Format = format;
			size_t rowPitch = 0;
			if (const uint8_t* mapped = static_cast<const uint8_t*>(device->mapStagingTexture(staging, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &rowPitch)))
			{
				const size_t rowBytes = static_cast<size_t>(width) * GetBytesPerPixel(format);
				pixels.Width = width;
				pixels.Height = height;
				pixels.Data.resize(rowBytes * height);
				for (uint32_t row = 0; row < height; ++row)
					std::memcpy(pixels.Data.data() + row * rowBytes, mapped + row * rowPitch, rowBytes);
				device->unmapStagingTexture(staging);
			}
			done(pixels);
		};

		// As for Upload: inside a frame the copy joins its list, which keeps it after the frame's
		// earlier draws and never opens a list of its own mid-frame (D3D11's ClearState).
		if (CommandList* frameList = Renderer::TryGetRecordingCommandList())
		{
			// D3D12 caches the state a framebuffer or binding set left the texture in, so a draw or
			// sample after the copy would find it a copy source: put back the state it had.
			nvrhi::ICommandList* list = static_cast<NvrhiCommandList*>(frameList)->GetNvrhiHandle();
			const nvrhi::ResourceStates state = list->getTextureSubresourceState(m_Handle, 0, 0);
			list->copyTexture(staging, nvrhi::TextureSlice(), m_Handle, nvrhi::TextureSlice());
			if (state != nvrhi::ResourceStates::Unknown)
			{
				list->setTextureState(m_Handle, nvrhi::AllSubresources, state);
				list->commitBarriers();
			}
			Renderer::RunAfterFrame(std::move(resolve));
			return;
		}

		auto copyAndResolve = [source = m_Handle, staging, resolve = std::move(resolve)]()
		{
			nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();
			nvrhi::CommandListHandle commandList = device->createCommandList(nvrhi::CommandListParameters().setQueueType(nvrhi::CommandQueue::Graphics));
			commandList->open();
			commandList->copyTexture(staging, nvrhi::TextureSlice(), source, nvrhi::TextureSlice());
			commandList->close();
			device->executeCommandList(commandList);
			resolve();
		};

		// Between frames (an event handler, a post-execution callback) the render thread may be
		// submitting, so a list of our own waits for the next frame's start, when it is parked.
		if (Renderer::IsRenderThreadParked())
			copyAndResolve();
		else
			Renderer::RunAfterFrame(std::move(copyAndResolve));
	}

}
