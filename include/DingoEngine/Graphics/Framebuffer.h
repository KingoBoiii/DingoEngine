#pragma once
#include "Enums/TextureFormat.h"
#include "DingoEngine/Graphics/Texture.h"

namespace Dingo
{

	struct FramebufferAttachment
	{
		TextureFormat Format = TextureFormat::Unknown;
	};

	struct FramebufferParams
	{
		std::string DebugName;
		int32_t Width;
		int32_t Height;
		std::vector<FramebufferAttachment> Attachments;

		FramebufferParams& SetDebugName(const std::string& name)
		{
			DebugName = name;
			return *this;
		}

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

		FramebufferParams& AddAttachment(const FramebufferAttachment& attachment)
		{
			Attachments.push_back(attachment);
			return *this;
		}
	};

	class Framebuffer
	{
	public:
		static Framebuffer* Create(const FramebufferParams& params);

	public:
		Framebuffer(const FramebufferParams& params);
		virtual ~Framebuffer() = default;

	public:
		virtual void Initialize() = 0;
		virtual void Destroy() = 0;

		virtual void Resize(uint32_t width, uint32_t height) = 0;

		virtual Texture* GetAttachment(uint32_t index) const = 0;

		const FramebufferParams& GetParams() const { return m_Params; }

	protected:
		FramebufferParams m_Params;

		friend class NvrhiPipeline;
		friend class CommandList;
		friend class ImGuiRenderer;
	};

}
