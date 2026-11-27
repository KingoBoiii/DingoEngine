#include "depch.h"
#include "DingoEngine/Graphics/RendererBackend.h"
#include "NvrhiTexture.h"

#include "DingoEngine/Core/FileSystem.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "NvrhiCommandList.h"
#include "NvrhiGraphicsContext.h"
#include "DingoEngine/Graphics/TextureLayout.h"
#include "DingoEngine/Graphics/TextureReadback.h"
#include "DingoEngine/UI/DingoUI/DingoUITextureHook.h"

#include <cstring>

namespace Dingo
{

	namespace Utils
	{

		static nvrhi::Format GetTextureFormat(const TextureFormat format)
		{
			switch (format)
			{
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
				case TextureDimension::TextureCube: return nvrhi::TextureDimension::TextureCube;
				default: break;
			}

			return nvrhi::TextureDimension::Unknown;
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

	namespace
	{

		struct SubresourceView
		{
			uint32_t Width = 0;
			uint32_t Height = 0;
			bool Volume = false;
			TextureFormat Format = TextureFormat::Unknown;
		};

		// False when the texture is not a colour texture of a readable format and dimension, or the
		// mip or layer is out of range; error says which.
		bool DescribeSubresource(const nvrhi::TextureDesc& desc, uint32_t mip, uint32_t layer, SubresourceView& view, const char*& error)
		{
			view.Format = Utils::GetTextureFormat(desc.format);
			view.Volume = desc.dimension == nvrhi::TextureDimension::Texture3D;
			const bool readable = desc.dimension == nvrhi::TextureDimension::Texture2D || desc.dimension == nvrhi::TextureDimension::Texture2DArray
				|| desc.dimension == nvrhi::TextureDimension::TextureCube || view.Volume;
			if (view.Format == TextureFormat::Unknown || !readable)
			{
				error = "is not a colour texture of a format and dimension it reads (RGBA8, RGBA16F, RGBA32F, R11G11B10F, R8, R16F, R32F; not 1D)";
				return false;
			}
			const Internal::TextureExtent extent = Internal::GetMipExtent(desc.width, desc.height, desc.depth, mip);
			view.Width = extent.Width;
			view.Height = extent.Height;
			const uint32_t layers = view.Volume ? extent.Depth : desc.arraySize;
			if (mip >= desc.mipLevels || layer >= layers)
			{
				error = "has no such mip level or layer";
				return false;
			}
			return true;
		}

		nvrhi::StagingTextureHandle CreateReadbackStaging(nvrhi::IDevice* device, const nvrhi::TextureDesc& desc, const SubresourceView& view)
		{
			return device->createStagingTexture(nvrhi::TextureDesc()
				.setDebugName(desc.debugName + " (readback)")
				.setWidth(view.Width)
				.setHeight(view.Height)
				.setDepth(1)
				.setFormat(desc.format)
				.setDimension(view.Volume ? nvrhi::TextureDimension::Texture3D : nvrhi::TextureDimension::Texture2D), nvrhi::CpuAccessMode::Read);
		}

		// D3D12 caches the state a framebuffer or binding set left the subresource in, so a draw or
		// sample after the copy would find it a copy source: it gets back the state it had.
		void RecordSubresourceCopy(nvrhi::ICommandList* list, nvrhi::IStagingTexture* staging, nvrhi::ITexture* source, uint32_t mip, uint32_t layer, const SubresourceView& view)
		{
			const nvrhi::TextureDesc& desc = source->getDesc();
			const uint32_t arraySlice = view.Volume ? 0 : layer;
			nvrhi::ResourceStates state = list->getTextureSubresourceState(source, arraySlice, mip);
			if (state == nvrhi::ResourceStates::Unknown)
				state = desc.initialState;

			nvrhi::TextureSlice slice = nvrhi::TextureSlice()
				.setMipLevel(mip)
				.setArraySlice(arraySlice)
				.setWidth(view.Width)
				.setHeight(view.Height)
				.setDepth(1);
			if (view.Volume)
				slice.setOrigin(0, 0, layer);
			list->copyTexture(staging, nvrhi::TextureSlice(), source, slice);
			if (state != nvrhi::ResourceStates::Unknown)
			{
				list->setTextureState(source, nvrhi::TextureSubresourceSet(mip, 1, arraySlice, 1), state);
				list->commitBarriers();
			}
		}

		TexturePixels CopyOutStaging(nvrhi::IDevice* device, nvrhi::IStagingTexture* staging, const SubresourceView& view)
		{
			TexturePixels pixels;
			pixels.Format = view.Format;
			size_t rowPitch = 0;
			if (const uint8_t* mapped = static_cast<const uint8_t*>(device->mapStagingTexture(staging, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &rowPitch)))
			{
				const size_t rowBytes = static_cast<size_t>(view.Width) * GetBytesPerPixel(view.Format);
				pixels.Width = view.Width;
				pixels.Height = view.Height;
				pixels.Data.resize(rowBytes * view.Height);
				for (uint32_t row = 0; row < view.Height; ++row)
					std::memcpy(pixels.Data.data() + row * rowBytes, mapped + row * rowPitch, rowBytes);
				device->unmapStagingTexture(staging);
			}
			return pixels;
		}

	}

	void NvrhiTexture::Initialize()
	{
		const uint32_t layers = Internal::GetTextureArrayLayers(m_Params);
		const bool depth = m_Params.Format == TextureFormat::D32;
		nvrhi::TextureDesc textureDesc = nvrhi::TextureDesc()
			.setDebugName(m_Params.DebugName)
			.setWidth(m_Params.Width)
			.setHeight(m_Params.Height)
			.setFormat(Utils::GetTextureFormat(m_Params.Format))
			.setDimension(m_Params.Dimension == TextureDimension::Texture2D && layers > 1 ? nvrhi::TextureDimension::Texture2DArray : Utils::GetTextureDimension(m_Params.Dimension))
			.setDepth(m_Params.Depth > 0 ? m_Params.Depth : 1)
			.setMipLevels(m_Params.MipLevels > 0 ? m_Params.MipLevels : 1)
			.setArraySize(layers)
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
			textureDesc.setClearValue(nvrhi::Color(k_DepthClearValue, 0.0f, 0.0f, 0.0f));
		}

		m_Handle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createTexture(textureDesc);
	}

	NvrhiTexture::~NvrhiTexture()
	{
		Internal::DingoUIOnTextureDestroyed(this);
	}

	void NvrhiTexture::Destroy()
	{
		Internal::DingoUIOnTextureDestroyed(this);
		m_Handle = nullptr;
	}

	void NvrhiTexture::Reinitialize(const TextureParams& params)
	{
		TextureParams normalized = params;
		std::string error;
		if (!Internal::NormalizeTextureParams(normalized, error))
		{
			DE_CORE_ERROR("Texture::Reinitialize: '{}': {}.", params.DebugName, error);
			return;
		}

		m_Params = normalized;
		m_Handle = nullptr; // NVRHI frees the old texture once in-flight frames release it
		Initialize();

		// Initialize() produced a different nvrhi::ITexture — tell cached binding sets.
		m_Generation = NextGeneration();

		if (m_Params.InitialData)
		{
			Upload(m_Params.InitialData);
			m_Params.InitialData = nullptr; // the caller's buffer is not retained
		}
	}

	void NvrhiTexture::Upload(const void* data)
	{
		DE_CORE_ASSERT(data, "Texture::Upload needs pixels");
		UploadLayers(static_cast<const uint8_t*>(data), 0, 0, Internal::GetTextureArrayLayers(m_Params), Internal::GetSubresourceBytes(m_Params, 0));
	}

	void NvrhiTexture::UploadSubresource(const void* data, uint32_t mip, uint32_t layer)
	{
		DE_CORE_ASSERT(data, "Texture::UploadSubresource needs pixels");
		const bool volume = m_Params.Dimension == TextureDimension::Texture3D;
		if (mip >= m_Params.MipLevels || layer >= (volume ? 1u : Internal::GetTextureArrayLayers(m_Params)))
		{
			DE_CORE_ERROR("Texture::UploadSubresource: '{}' has no mip {} layer {}.", m_Params.DebugName, mip, layer);
			return;
		}
		UploadLayers(static_cast<const uint8_t*>(data), mip, layer, 1, Internal::GetSubresourceBytes(m_Params, mip));
	}

	void NvrhiTexture::UploadLayers(const uint8_t* data, uint32_t mip, uint32_t firstLayer, uint32_t layerCount, uint64_t layerBytes)
	{
		const Internal::TextureExtent extent = Internal::GetMipExtent(m_Params, mip);
		const uint64_t rowPitch = Utils::GetImageMemoryRowPitch(m_Params.Format, extent.Width);
		const uint64_t depthPitch = m_Params.Dimension == TextureDimension::Texture3D ? rowPitch * extent.Height : 0;

		// Join the frame's command list whenever one is open. A list of our own would be
		// wrong mid-frame on D3D11: NVRHI's open() and close() each call ClearState() on the
		// immediate context, unbinding the render target that the frame's list still believes
		// is set, and it does not re-bind because its own graphics state looks unchanged.
		// The async asset pump runs before any layer draws so it never noticed, but the
		// Assets panel's Reload button runs after every layer has recorded its draws.
		if (Internal::CommandList* frameList = Internal::RendererBackend::TryGetRecordingCommandList())
		{
			for (uint32_t i = 0; i < layerCount; ++i)
				frameList->UploadTexture(this, data + i * layerBytes, mip, firstLayer + i, rowPitch, depthPitch);
			return;
		}

		// Outside a frame (asset loads during OnAttach, the white texture during
		// Renderer::Initialize) there is no list to join, so open a short-lived one.
		nvrhi::CommandListParameters commandListParameters = nvrhi::CommandListParameters()
			.setQueueType(nvrhi::CommandQueue::Graphics);

		nvrhi::CommandListHandle commandList = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createCommandList(commandListParameters);

		commandList->open();

		for (uint32_t i = 0; i < layerCount; ++i)
			commandList->writeTexture(m_Handle, firstLayer + i, mip, data + i * layerBytes, rowPitch, depthPitch);

		commandList->close();

		GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->executeCommandList(commandList);
	}

	void NvrhiTexture::ReadPixels(std::function<void(const TexturePixels&)> done)
	{
		ReadSubresource(0, 0, std::move(done));
	}

	void NvrhiTexture::ReadSubresource(uint32_t mip, uint32_t layer, std::function<void(const TexturePixels&)> done)
	{
		SubresourceView view;
		const char* error = nullptr;
		if (!m_Handle || !DescribeSubresource(m_Handle->getDesc(), mip, layer, view, error))
		{
			DE_CORE_ERROR("Texture::ReadSubresource: '{}' {}.", m_Params.DebugName, error ? error : "has no GPU texture");
			done(TexturePixels{});
			return;
		}

		nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();
		nvrhi::StagingTextureHandle staging = CreateReadbackStaging(device, m_Handle->getDesc(), view);

		// Mapping waits for the GPU to finish the copy, on every backend.
		auto resolve = [staging, view, done = std::move(done)]()
		{
			nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();
			done(CopyOutStaging(device, staging, view));
		};

		// As for Upload: inside a frame the copy joins its list, which keeps it after the frame's
		// earlier draws and never opens a list of its own mid-frame (D3D11's ClearState).
		if (Internal::CommandList* frameList = Internal::RendererBackend::TryGetRecordingCommandList())
		{
			RecordSubresourceCopy(static_cast<NvrhiCommandList*>(frameList)->GetNvrhiHandle(), staging, m_Handle, mip, layer, view);
			Renderer::RunAfterFrame(std::move(resolve));
			return;
		}

		auto copyAndResolve = [source = m_Handle, staging, mip, layer, view, resolve = std::move(resolve)]()
		{
			nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();
			nvrhi::CommandListHandle commandList = device->createCommandList(nvrhi::CommandListParameters().setQueueType(nvrhi::CommandQueue::Graphics));
			commandList->open();
			RecordSubresourceCopy(commandList, staging, source, mip, layer, view);
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

	namespace Internal
	{

		class NvrhiTextureReadback : public TextureReadback, public std::enable_shared_from_this<NvrhiTextureReadback>
		{
		public:
			bool IsBusy() const override { return m_Busy; }

			bool Read(Texture* source, std::function<void(const TexturePixels&)> done) override
			{
				nvrhi::ITexture* handle = source ? static_cast<nvrhi::ITexture*>(source->GetTextureHandle()) : nullptr;
				Internal::CommandList* frameList = Internal::RendererBackend::TryGetRecordingCommandList();
				if (m_Busy || !handle || !frameList)
					return false;
				const nvrhi::TextureDesc& desc = handle->getDesc();
				SubresourceView view;
				const char* error = nullptr;
				if (!DescribeSubresource(desc, 0, 0, view, error))
					return false;

				nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();
				if (!m_Staging || m_View.Width != view.Width || m_View.Height != view.Height || m_View.Format != view.Format || m_View.Volume != view.Volume)
				{
					m_Staging = CreateReadbackStaging(device, desc, view);
					m_View = view;
				}
				// As in NvrhiTexture::ReadSubresource: the copy joins the frame, and the subresource gets
				// back the state it had.
				RecordSubresourceCopy(static_cast<NvrhiCommandList*>(frameList)->GetNvrhiHandle(), m_Staging, handle, 0, 0, view);

				m_Busy = true;
				// The frame is submitted by the next frame's start, so the event set then follows the copy.
				Renderer::RunAfterFrame([self = shared_from_this(), done = std::move(done)]() mutable
				{
					// A fresh query each time: NVRHI's D3D11 poll takes S_FALSE for done, so a reused query
					// could be ended again with its result never fetched (#410 QUERY_END_ABANDONING_PREVIOUS_RESULTS).
					nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();
					self->m_Event = device->createEventQuery();
					device->setEventQuery(self->m_Event, nvrhi::CommandQueue::Graphics);
					self->Poll(std::move(done));
				});
				return true;
			}

		private:
			void Poll(std::function<void(const TexturePixels&)> done)
			{
				nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();
				if (!device->pollEventQuery(m_Event) && !Renderer::IsShuttingDown())
				{
					Renderer::RunAfterFrame([self = shared_from_this(), done = std::move(done)]() mutable { self->Poll(std::move(done)); });
					return;
				}

				TexturePixels pixels = CopyOutStaging(device, m_Staging, m_View);
				m_Busy = false;
				done(pixels);
			}

			nvrhi::StagingTextureHandle m_Staging;
			nvrhi::EventQueryHandle m_Event;
			SubresourceView m_View;
			bool m_Busy = false;
		};

		std::shared_ptr<TextureReadback> TextureReadback::Create()
		{
			return std::make_shared<NvrhiTextureReadback>();
		}

	}

}
