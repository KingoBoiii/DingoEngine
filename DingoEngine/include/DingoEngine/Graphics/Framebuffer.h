#pragma once

#include <nvrhi/nvrhi.h>

namespace DingoEngine
{

	struct FramebufferParams
	{
		nvrhi::ITexture* Texture = nullptr;

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

	public:
		Framebuffer(nvrhi::ITexture* texture);
		virtual ~Framebuffer() = default;

	public:
		virtual void Initialize();
		virtual void Destroy();

	protected:
		nvrhi::ITexture* m_Texture;
		nvrhi::FramebufferHandle m_FramebufferHandle;

		friend class Pipeline;
		friend class CommandList;
	};

}
