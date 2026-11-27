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

	/// The number of mip levels in a full chain for a texture of this size: down to 1x1x1 along
	/// the largest side. Pass depth only for a 3D texture.
	constexpr uint32_t GetMipLevelCount(uint32_t width, uint32_t height, uint32_t depth = 1)
	{
		uint32_t largest = width > height ? width : height;
		if (depth > largest)
			largest = depth;
		uint32_t levels = 1;
		while (largest > 1)
		{
			largest >>= 1;
			++levels;
		}
		return levels;
	}

	/// Describes a Texture for Texture::Create.
	struct TextureParams
	{
		std::string DebugName; ///< Name shown in graphics debuggers and logs.
		uint32_t Width = 0; ///< Width in texels; required.
		uint32_t Height = 0; ///< Height in texels; required.
		uint32_t Depth = 1; ///< Depth in texels; above 1 only for Texture3D.
		/// Mip levels; 0 means the full chain down to 1x1x1 (GetMipLevelCount). InitialData and
		/// Upload fill mip 0 only; the other mips are undefined until uploaded or written.
		uint32_t MipLevels = 1;
		/// Array layers; above 1 only for Texture2D (a 2D array). A TextureCube always has 6.
		uint32_t ArrayLayers = 1;
		TextureFormat Format = TextureFormat::Unknown; ///< Pixel format; required.
		/// Required; Texture2D for ordinary images.
		TextureDimension Dimension = TextureDimension::Unknown;
		/// Lets the texture be drawn into as a framebuffer attachment (colour, or depth for D32);
		/// only a single-layer Texture2D can be one.
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

		TextureParams& SetDepth(uint32_t depth)
		{
			Depth = depth;
			return *this;
		}

		TextureParams& SetMipLevels(uint32_t mipLevels)
		{
			MipLevels = mipLevels;
			return *this;
		}

		TextureParams& SetArrayLayers(uint32_t arrayLayers)
		{
			ArrayLayers = arrayLayers;
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

	/// A GPU texture (one or more mip levels and layers): an image, a render target or a storage image. The caller
	/// owns it; delete frees the GPU texture. Wrapping and filtering belong to a Sampler.
	class Texture : public IBindableShaderResource
	{
	public:
		/// Returns nullptr on failure (error is logged), and when there is no graphics device, as
		/// in a headless app. Caller owns the returned Texture.
		/// A relative filepath is looked up under the asset root first, then the working
		/// directory.
		[[nodiscard]] static Texture* CreateFromFile(const std::filesystem::path& filepath, const std::string& debugName = "Texture (File)");
		/// Loads a Radiance .hdr image (looked up like CreateFromFile, and flipped on load the same
		/// way) as a sampled-only 2D texture of `format`, which must be RGBA16F or RGBA32F; any
		/// other format, a file that isn't a .hdr, or a load failure logs an error and returns
		/// nullptr. RGBA16F clamps to the half range (65504), so a sun brighter than that is cut off.
		[[nodiscard]] static Texture* CreateFromHdrFile(const std::filesystem::path& filepath, TextureFormat format = TextureFormat::RGBA16F, const std::string& debugName = "Texture (HDR)");
		/// Creates a width x height texture of format from packed pixels, copied before it returns;
		/// the texture is sampled-only, not a render target. Returns nullptr (with an error) when there
		/// is no graphics device, as in a headless app.
		[[nodiscard]] static Texture* CreateFromData(uint32_t width, uint32_t height, const void* data, TextureFormat format = TextureFormat::RGBA8_UNORM, const std::string& debugName = "Texture (Data)");
		/// Creates the texture from params, ready to use; uploads InitialData when set. The caller
		/// owns it. Returns nullptr (with an error) when there is no graphics device, as in a headless
		/// app, or when the params are invalid (a missing size, format or dimension, or a combination
		/// the dimension doesn't allow).
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
		/// Mip 0 of every layer, in layer order (the six faces of a cube): each Width x Height
		/// (x Depth for a 3D texture) pixels of the format, packed, first row first. Joins the
		/// frame's command list when one is recording, else uploads at once.
		virtual void Upload(const void* data) = 0;
		/// One mip of one array layer or cube face: the mip's extent in packed pixels (a 3D
		/// texture takes every slice of the mip, with layer 0). Timing as Upload. Out-of-range
		/// mip or layer logs an error and uploads nothing.
		virtual void UploadSubresource(const void* data, uint32_t mip, uint32_t layer = 0) = 0;

		/// Recreates the GPU texture in place from new params (uploading InitialData if
		/// set), so existing Texture* references survive a content change - the backbone
		/// of hot-reload. Changes GetGeneration(); the old GPU texture is freed by the
		/// graphics backend once in-flight frames drop it. Invalid params log an error and leave the
		/// old texture and its generation as they were.
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
		/// (Renderer::IsFrameSkipped) reads what the texture last held. Any colour format
		/// (not D32): the pixels come back in the texture's format, and TexturePixels::GetPixel
		/// decodes them. Row 0 is the texture's first row: the top of a render target's picture,
		/// the bottom of an image CreateFromFile loaded (it flips on load).
		virtual void ReadPixels(std::function<void(const TexturePixels&)> done) = 0;
		/// ReadPixels for one mip of one array layer or cube face (for a 3D texture, layer is the z
		/// slice); the pixels are the mip's extent wide and high. ReadPixels is ReadSubresource(0, 0)
		/// and works on any dimension but 1D. Out-of-range mip or layer reports empty pixels.
		virtual void ReadSubresource(uint32_t mip, uint32_t layer, std::function<void(const TexturePixels&)> done) = 0;
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
		/// Never reused, unlike the address, and kept across Reinitialize, unlike the generation.
		uint64_t GetId() const { return m_Id; }

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

	private:
		static uint64_t AllocateId();

	protected:
		TextureParams m_Params; ///< The params the texture was created or last Reinitialized with.
		uint32_t m_Generation = NextGeneration(); ///< See GetGeneration.

	private:
		uint64_t m_Id = AllocateId();
	};

}
