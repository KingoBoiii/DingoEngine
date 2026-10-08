#include "depch.h"
#include "NvrhiFramebuffer.h"
#include "DingoEngine/Graphics/FramebufferFormat.h"

#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/NVRHI/NvrhiGraphicsContext.h"

namespace Dingo
{

	void NvrhiFramebuffer::Initialize()
	{
		for (uint32_t index = 0; index < m_Params.Attachments.size(); ++index)
			m_Attachments.push_back(Texture::Create(MakeColorParams(index)));

		if (m_Params.EnableDepth)
		{
			m_DepthAttachment = Texture::Create(MakeDepthParams());
			DE_CORE_ASSERT(m_DepthAttachment->GetTextureHandle(), "Framebuffer depth texture creation failed; depth test/write would be silently disabled.");
		}

		CreateHandle();
	}

	void NvrhiFramebuffer::Destroy()
	{
		for (Texture*& attachment : m_Attachments)
			DestroyAndDelete(attachment);
		m_Attachments.clear();
		DestroyAndDelete(m_DepthAttachment);

		m_DepthTextureHandle = nullptr;
		m_FramebufferHandle = nullptr;
	}

	void NvrhiFramebuffer::Resize(uint32_t width, uint32_t height)
	{
		m_Width = m_Params.Width = width;
		m_Height = m_Params.Height = height;

		for (uint32_t index = 0; index < m_Attachments.size(); ++index)
			m_Attachments[index]->Reinitialize(MakeColorParams(index));
		if (m_DepthAttachment)
			m_DepthAttachment->Reinitialize(MakeDepthParams());

		CreateHandle();
	}

	TextureParams NvrhiFramebuffer::MakeColorParams(uint32_t index) const
	{
		return TextureParams()
			.SetDebugName(std::format("{} ({})", m_Params.DebugName, index))
			.SetWidth(m_Params.Width)
			.SetHeight(m_Params.Height)
			.SetFormat(m_Params.Attachments[index].Format)
			.SetDimension(TextureDimension::Texture2D)
			.SetIsRenderTarget(true);
	}

	TextureParams NvrhiFramebuffer::MakeDepthParams() const
	{
		return TextureParams()
			.SetDebugName(m_Params.DebugName + " (Depth)")
			.SetWidth(m_Params.Width)
			.SetHeight(m_Params.Height)
			.SetFormat(TextureFormat::D32)
			.SetDimension(TextureDimension::Texture2D)
			.SetIsRenderTarget(true)
			.SetIsShaderResource(m_Params.DepthSampleable);
	}

	void NvrhiFramebuffer::CreateHandle()
	{
		nvrhi::FramebufferDesc framebufferDesc = nvrhi::FramebufferDesc();
		for (Texture* attachment : m_Attachments)
			framebufferDesc.addColorAttachment(static_cast<nvrhi::ITexture*>(attachment->GetTextureHandle()));

		m_DepthTextureHandle = m_DepthAttachment ? static_cast<nvrhi::ITexture*>(m_DepthAttachment->GetTextureHandle()) : nullptr;
		if (m_DepthTextureHandle)
			framebufferDesc.setDepthAttachment(m_DepthTextureHandle);

		m_FramebufferHandle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createFramebuffer(framebufferDesc);

		m_Viewport = nvrhi::Viewport(static_cast<float>(m_Params.Width), static_cast<float>(m_Params.Height));
	}

	namespace Internal
	{

		uint64_t GetFramebufferFormatKey(const Framebuffer* framebuffer)
		{
			const NvrhiFramebuffer* nvrhiFramebuffer = static_cast<const NvrhiFramebuffer*>(framebuffer);
			if (!nvrhiFramebuffer || !nvrhiFramebuffer->m_FramebufferHandle)
				return 0;

			const nvrhi::FramebufferInfo& info = nvrhiFramebuffer->m_FramebufferHandle->getFramebufferInfo();

			uint64_t key = 0xcbf29ce484222325ull;
			auto mix = [&key](uint64_t value)
			{
				key ^= value;
				key *= 0x100000001b3ull;
			};

			mix(info.colorFormats.size());
			for (const nvrhi::Format format : info.colorFormats)
				mix(static_cast<uint64_t>(format));
			mix(static_cast<uint64_t>(info.depthFormat));
			mix(info.sampleCount);
			mix(info.sampleQuality);
			return key;
		}

	}

}
