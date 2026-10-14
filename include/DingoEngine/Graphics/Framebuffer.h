#pragma once
#include "Enums/TextureFormat.h"
#include "DingoEngine/Graphics/Texture.h"

namespace Dingo
{

	/// One colour attachment of a framebuffer.
	struct FramebufferAttachment
	{
		/// Format of the attachment's texture: a colour format such as RGBA8_UNORM or RGBA16F.
		TextureFormat Format = TextureFormat::Unknown;
	};

	/// Describes a framebuffer for Framebuffer::Create.
	struct FramebufferParams
	{
		std::string DebugName; ///< Name shown in graphics debuggers and the engine's debug panels.
		int32_t Width = 0;  ///< Width in pixels; required.
		int32_t Height = 0; ///< Height in pixels; required.
		/// Colour attachments, in render-target order. None and EnableDepth: a depth-only target (a
		/// shadow map).
		std::vector<FramebufferAttachment> Attachments;
		bool EnableDepth = false; ///< Adds a depth attachment, cleared to 1 by Renderer::Clear.
		/// The depth can be sampled through GetDepthAttachment() (a D32 texture, typeless on D3D).
		/// Off, it is a depth target alone, as before v0.9.
		bool DepthSampleable = false;

		/// Each setter sets the field of the same name and returns *this, for chaining.
		///@{
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
		///@}

		/// Appends one colour attachment to Attachments and returns *this, for chaining.
		FramebufferParams& AddAttachment(const FramebufferAttachment& attachment)
		{
			Attachments.push_back(attachment);
			return *this;
		}

		/// Sets EnableDepth and returns *this, for chaining.
		FramebufferParams& SetEnableDepth(bool enable)
		{
			EnableDepth = enable;
			return *this;
		}

		/// Implies SetEnableDepth(true).
		FramebufferParams& SetDepthSampleable(bool sampleable)
		{
			DepthSampleable = sampleable;
			EnableDepth = EnableDepth || sampleable;
			return *this;
		}
	};

	/// A set of colour attachments and an optional depth attachment that draws render into. The
	/// caller owns a framebuffer made with Create(); delete it (the swap chain's is the engine's).
	class Framebuffer
	{
	public:
		/// Creates the framebuffer and its attachment textures; the caller deletes it. Not for the
		/// swap chain: that framebuffer comes from Renderer::GetSwapChainFramebuffer.
		[[nodiscard]] static Framebuffer* Create(const FramebufferParams& params);

	public:
		/// Stores the params; use Create(), which also makes the GPU objects.
		Framebuffer(const FramebufferParams& params);
		/// Frees the attachment textures and GPU objects; not while a frame that draws into it is
		/// still being recorded.
		virtual ~Framebuffer() = default;

	protected:
		/// Create() calls it, so a resource is ready when the factory returns it.
		virtual void Initialize() = 0;

	public:
		/// Releases the GPU object early; the destructor releases everything anyway.
		virtual void Destroy() = 0;

		/// Resizes every attachment in place (Texture::Reinitialize): the Texture objects
		/// GetAttachment and GetDepthAttachment return stay the same, with a new GetGeneration(),
		/// so materials and passes that bound them rebind at their next draw. Their contents are
		/// undefined until drawn. Sizes are in pixels.
		virtual void Resize(uint32_t width, uint32_t height) = 0;

		/// Width in pixels.
		virtual uint32_t GetWidth() const = 0;
		/// Height in pixels.
		virtual uint32_t GetHeight() const = 0;
		/// nullptr when there is no such colour attachment - always for the swap-chain
		/// framebuffer, which draws straight into the swap-chain image and owns no Texture.
		/// The framebuffer owns the texture; never destroy it.
		virtual Texture* GetAttachment(uint32_t index) const = 0;
		/// The depth as a D32 texture, for sampling; nullptr unless
		/// FramebufferParams::DepthSampleable.
		virtual Texture* GetDepthAttachment() const = 0;

		/// The params it was created with; Width and Height follow Resize.
		const FramebufferParams& GetParams() const { return m_Params; }
		/// Never reused, unlike the address, which a freed framebuffer can hand to one of other
		/// formats.
		uint64_t GetId() const { return m_Id; }

	private:
		static uint64_t AllocateId();

	protected:
		FramebufferParams m_Params; ///< The params, with Width and Height following Resize.

	private:
		uint64_t m_Id = AllocateId();
	};

}
