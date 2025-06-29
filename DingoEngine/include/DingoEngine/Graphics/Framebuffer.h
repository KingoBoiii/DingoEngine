#pragma once

#include <nvrhi/nvrhi.h>

namespace DingoEngine
{

	struct FramebufferParams
	{
		int32_t Width;
		int32_t Height;
		nvrhi::ITexture* Texture = nullptr;

		FramebufferParams& SetWidth(int32_t width)
		{
			Width = width;
			return *this;
		}

		FramebufferParams& SetHeight(int32_t height)
		{
			Height = height;
			return *this;
		}

		FramebufferParams& SetTexture(nvrhi::ITexture* texture)
		{
			Texture = texture;
			return *this;
		}
	};

	class Framebuffer
	{
	public:
		static Framebuffer* Create(nvrhi::ITexture* texture);
		static Framebuffer* Create(const FramebufferParams& params);

	public:
		Framebuffer(const FramebufferParams& params);
		virtual ~Framebuffer() = default;

	public:
		virtual void Initialize();
		virtual void Destroy();

	protected:
		FramebufferParams m_Params;
		nvrhi::FramebufferHandle m_FramebufferHandle;
		nvrhi::Viewport m_Viewport;

		friend class Pipeline;
		friend class CommandList;
	};

}
