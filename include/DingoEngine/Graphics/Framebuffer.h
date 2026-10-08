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
		int32_t Width = 0;
		int32_t Height = 0;
		std::vector<FramebufferAttachment> Attachments; // none and EnableDepth: a depth-only target (a shadow map)
		bool EnableDepth = false;
		// The depth can be sampled through GetDepthAttachment() (a D32 texture, typeless on D3D). Off,
		// it is a depth target alone, as before v0.9.
		bool DepthSampleable = false;

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

		FramebufferParams& SetEnableDepth(bool enable)
		{
			EnableDepth = enable;
			return *this;
		}

		// Implies SetEnableDepth(true).
		FramebufferParams& SetDepthSampleable(bool sampleable)
		{
			DepthSampleable = sampleable;
			EnableDepth = EnableDepth || sampleable;
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

		// Resizes every attachment in place (Texture::Reinitialize): the Texture objects GetAttachment
		// and GetDepthAttachment return stay the same, with a new GetGeneration(), so materials and
		// passes that bound them rebind at their next draw. Their contents are undefined until drawn.
		virtual void Resize(uint32_t width, uint32_t height) = 0;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		// nullptr when there is no such colour attachment — always for the swap-chain
		// framebuffer, which draws straight into the swap-chain image and owns no Texture.
		virtual Texture* GetAttachment(uint32_t index) const = 0;
		// The depth as a D32 texture, for sampling; nullptr unless FramebufferParams::DepthSampleable.
		virtual Texture* GetDepthAttachment() const = 0;

		const FramebufferParams& GetParams() const { return m_Params; }
		// Never reused, unlike the address, which a freed framebuffer can hand to one of other formats.
		uint64_t GetId() const { return m_Id; }

	private:
		static uint64_t AllocateId();

	protected:
		FramebufferParams m_Params;

	private:
		uint64_t m_Id = AllocateId();

		friend class NvrhiPipeline;
		friend class CommandList;
		friend class ImGuiRenderer;
	};

}
