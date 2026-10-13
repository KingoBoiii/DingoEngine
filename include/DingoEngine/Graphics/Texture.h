#pragma once
#include "DingoEngine/Graphics/Enums/TextureFormat.h"
#include "DingoEngine/Graphics/Enums/TextureDimension.h"
#include "DingoEngine/Graphics/IBindableShaderResource.h"

#include <glm/glm.hpp>

#include <filesystem>
#include <functional>
#include <vector>

namespace Dingo
{

	/// A texture's pixels back on the CPU in the texture's own format, Width *
	/// GetBytesPerPixel(Format) bytes a row, rows in the texture's own order (Texture::ReadPixels).
	/// An RGBA8 texture reads back as 8-bit RGBA.
	struct TexturePixels
	{
		uint32_t Width = 0; ///< Width in pixels.
		uint32_t Height = 0; ///< Height in pixels.
		TextureFormat Format = TextureFormat::RGBA8_UNORM; ///< Format of Data, the texture's own.
		std::vector<uint8_t> Data; ///< empty when the texture couldn't be read

		/// One pixel decoded to floats: unorm formats in 0..1, float formats as stored, and
		/// channels the format lacks as 0 (alpha 1). Zero outside the image or when Data is empty.
		glm::vec4 GetPixel(uint32_t x, uint32_t y) const;
	};

	/// Describes a Texture for Texture::Create.
	struct TextureParams
	{
		std::string DebugName; ///< Name shown in graphics debuggers and logs.
		uint32_t Width = 0; ///< Width in texels; required.
		uint32_t Height = 0; ///< Height in texels; required.
		TextureFormat Format = TextureFormat::Unknown; ///< Pixel format; required.
		/// Required; Texture2D for ordinary images.
		TextureDimension Dimension = TextureDimension::Unknown;
		/// Lets the texture be drawn into as a framebuffer attachment (colour, or depth for D32).
		bool IsRenderTarget = false;
		/// A D32 texture can be sampled only with this set, which makes it typeless on D3D (R32
		/// behind a D32 depth view); without it it is a depth target alone, as the swap chain's is.
		bool IsShaderResource = true;
		/// A compute shader can write it as a storage image (ComputePass::SetStorageTexture).
		bool IsStorage = false;

		/// Pixels to upload at Create (see Texture::Upload for the layout); copied, never kept.
		const void* InitialData = nullptr;

		/// Each setter sets the field of the same name and returns *this, for chaining.
		///@{
		TextureParams& SetDebugName(const std::string& name)
		{
			DebugName = name;
			return *this;
		}

		TextureParams& SetWidth(uint32_t width)
		{
			Width = width;
			return *this;
		}

		TextureParams& SetHeight(uint32_t height)
		{
			Height = height;
			return *this;
		}

		TextureParams& SetFormat(TextureFormat format)
		{
			Format = format;
			return *this;
		}

		TextureParams& SetDimension(TextureDimension dimension)
		{
			Dimension = dimension;
			return *this;
		}

		TextureParams& SetIsRenderTarget(bool isRenderTarget)
		{
			IsRenderTarget = isRenderTarget;
			return *this;
		}

		TextureParams& SetIsShaderResource(bool isShaderResource)
		{
			IsShaderResource = isShaderResource;
			return *this;
		}

		TextureParams& SetIsStorage(bool isStorage)
		{
			IsStorage = isStorage;
			return *this;
		}

		TextureParams& SetInitialData(const void* data)
		{
			InitialData = data;
			return *this;
		}
		///@}
	};

	/// A GPU texture with one mip level: an image, a render target or a storage image. The caller
	/// owns it; delete frees the GPU texture. Wrapping and filtering belong to a Sampler.
	class Texture : public IBindableShaderResource
	{
	public:
		/// Returns nullptr on failure (error is logged). Caller owns the returned Texture.
		/// A relative filepath is looked up under the asset root first, then the working
		/// directory.
		[[nodiscard]] static Texture* CreateFromFile(const std::filesystem::path& filepath, const std::string& debugName = "Texture (File)");
		/// Creates a width x height texture of format from packed pixels, copied before it returns;
		/// the texture is sampled-only, not a render target.
		[[nodiscard]] static Texture* CreateFromData(uint32_t width, uint32_t height, const void* data, TextureFormat format = TextureFormat::RGBA8_UNORM, const std::string& debugName = "Texture (Data)");
		/// Creates the texture from params, ready to use; uploads InitialData when set. The caller
		/// owns it.
		[[nodiscard]] static Texture* Create(const TextureParams& params);

	public:
		/// For subclasses; make textures with Create.
		Texture(const TextureParams& params)
			: m_Params(params)
		{}
		/// Frees the GPU texture.
		virtual ~Texture() = default;

	protected:
		/// Create() calls it, so a resource is ready when the factory returns it.
		virtual void Initialize() = 0;

	public:
		/// Releases the GPU object early; the destructor releases everything anyway.
		virtual void Destroy() = 0;
		/// The whole image: Width x Height pixels of the format, packed, first row first. Joins the
		/// frame's command list when one is recording, else uploads at once.
		virtual void Upload(const void* data) = 0;

		/// Recreates the GPU texture in place from new params (uploading InitialData if
		/// set), so existing Texture* references survive a content change - the backbone
		/// of hot-reload. Changes GetGeneration(); the old GPU texture is freed by the
		/// graphics backend once in-flight frames drop it.
		virtual void Reinitialize(const TextureParams& params) = 0;

		/// Copies the texture back to the CPU and hands its pixels to `done`, on the main thread.
		/// - In OnUpdate or OnImGuiRender the copy follows the draws recorded so far (Renderer2D
		///   and Renderer3D record theirs at EndScene, so call it after), and `done` runs at the
		///   start of the next frame, before its command list opens: it may read back or upload,
		///   not draw.
		/// - In OnAttach, or a frame Application skips while minimized, `done` runs before
		///   ReadPixels returns.
		/// - Between frames (an event handler, a post-execution callback), at the next frame's
		///   start.
		///
		/// A paused app's next frame waits for it to resume, and a frame that renders nothing
		/// (Renderer::IsFrameSkipped) reads what the texture last held. Any 2D colour format
		/// (not D32): the pixels come back in the texture's format, and TexturePixels::GetPixel
		/// decodes them. Row 0 is the texture's first row: the top of a render target's picture,
		/// the bottom of an image CreateFromFile loaded (it flips on load).
		virtual void ReadPixels(std::function<void(const TexturePixels&)> done) = 0;
		/// Writes the texture to an image file through ReadPixels and FileSystem::WriteImage (PNG,
		/// or BMP, TGA, JPEG by extension), with ReadPixels' timing. A texture that isn't a render
		/// target is written last row first, so an image CreateFromFile loaded comes out as its
		/// file was. RGBA8 textures only; any other format reports failure.
		/// `done`, if given, reports whether the file was written.
		void SaveToFile(const std::filesystem::path& path, std::function<void(bool)> done = {});

		/// Changed by every Reinitialize. Owners that cache a binding set built from this
		/// texture's native handle (Material's per-framebuffer render passes) compare it at
		/// bind time and re-bake on a mismatch: the handle is a *new* object after a
		/// reload, so a cache that only re-bakes on demand would sample - and keep alive -
		/// the original forever. Renderer2D needs no such check because it re-sets every
		/// slot each flush. Every texture draws it from one counter, so a texture created
		/// at a freed texture's address never matches what was cached for the old one.
		uint32_t GetGeneration() const { return m_Generation; }

		/// Width in texels.
		virtual uint32_t GetWidth() const { return m_Params.Width; }
		/// Height in texels.
		virtual uint32_t GetHeight() const { return m_Params.Height; }
		/// The params the texture was created or last Reinitialized with.
		const TextureParams& GetParams() const { return m_Params; }
		/// The backend's native texture object, for engine internals; game code has no use for it.
		virtual void* GetTextureHandle() const = 0;

	private:
		virtual bool NativeEquals(const Texture* other) const = 0;
		friend class Renderer2D;

	protected:
		/// The next value of the counter every texture's generation is drawn from.
		static uint32_t NextGeneration();

	protected:
		TextureParams m_Params; ///< The params the texture was created or last Reinitialized with.
		uint32_t m_Generation = NextGeneration(); ///< See GetGeneration.
	};

}
