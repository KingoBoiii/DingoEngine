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

	// A texture's pixels back on the CPU in the texture's own format, Width * GetBytesPerPixel(Format)
	// bytes a row, rows in the texture's own order (Texture::ReadPixels). An RGBA8 texture reads back
	// as 8-bit RGBA.
	struct TexturePixels
	{
		uint32_t Width = 0;
		uint32_t Height = 0;
		TextureFormat Format = TextureFormat::RGBA8_UNORM;
		std::vector<uint8_t> Data; // empty when the texture couldn't be read

		// One pixel decoded to floats: unorm formats in 0..1, float formats as stored, and channels the
		// format lacks as 0 (alpha 1). Zero outside the image or when Data is empty.
		glm::vec4 GetPixel(uint32_t x, uint32_t y) const;
	};

	enum class TextureWrapMode
	{
		Repeat,
		MirroredRepeat,
		ClampToEdge,
		ClampToBorder,
		MirrorClampToEdge
	};

	struct TextureParams
	{
		std::string DebugName;
		uint32_t Width;
		uint32_t Height;
		TextureFormat Format = TextureFormat::Unknown;
		TextureDimension Dimension = TextureDimension::Unknown;
		TextureWrapMode WrapMode = TextureWrapMode::Repeat;
		bool IsRenderTarget = false;
		// A D32 texture can be sampled only with this set, which makes it typeless on D3D (R32 behind a
		// D32 depth view); without it it is a depth target alone, as the swap chain's is.
		bool IsShaderResource = true;
		// A compute shader can write it as a storage image (ComputePass::SetStorageTexture).
		bool IsStorage = false;

		const void* InitialData = nullptr;

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

		TextureParams& SetWrapMode(TextureWrapMode wrapMode)
		{
			WrapMode = wrapMode;
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
	};

	class Texture : public IBindableShaderResource
	{
	public:
		// Returns nullptr on failure (error is logged). Caller owns the returned Texture.
		// A relative filepath is looked up under the asset root first, then the working
		// directory.
		static Texture* CreateFromFile(const std::filesystem::path& filepath, const std::string& debugName = "Texture (File)");
		static Texture* CreateFromData(uint32_t width, uint32_t height, const void* data, TextureFormat format = TextureFormat::RGBA, const std::string& debugName = "Texture (Data)");
		static Texture* Create(const TextureParams& params);

	public:
		Texture(const TextureParams& params)
			: m_Params(params)
		{}
		virtual ~Texture() = default;

	public:
		virtual void Initialize() = 0;
		virtual void Destroy() = 0;
		virtual void Upload(const void* data, uint64_t size) = 0;

		// Recreates the GPU texture in place from new params (uploading InitialData if
		// set), so existing Texture* references survive a content change - the backbone
		// of hot-reload. Changes GetGeneration(); the old GPU texture is freed by the
		// graphics backend once in-flight frames drop it.
		virtual void Reinitialize(const TextureParams& params) = 0;

		virtual bool NativeEquals(const Texture* other) const = 0;

		// Copies the texture back to the CPU and hands its pixels to `done`, on the main thread.
		// - In OnUpdate or OnUIRender the copy follows the draws recorded so far (Renderer2D and
		//   Renderer3D record theirs at EndScene, so call it after), and `done` runs at the start of
		//   the next frame, before its command list opens: it may read back or upload, not draw.
		// - In OnAttach, or a frame Application skips while minimized, `done` runs before
		//   ReadPixels returns.
		// - Between frames (an event handler, a post-execution callback), at the next frame's start.
		// A paused app's next frame waits for it to resume, and a frame that renders nothing
		// (Renderer::IsFrameSkipped) reads what the texture last held. Any 2D colour format (not D32):
		// the pixels come back in the texture's format, and TexturePixels::GetPixel decodes them.
		// Row 0 is the texture's first row: the top of a render target's picture, the bottom of
		// an image CreateFromFile loaded (it flips on load).
		virtual void ReadPixels(std::function<void(const TexturePixels&)> done) = 0;
		// Writes the texture to an image file through ReadPixels and FileSystem::WriteImage (PNG, or
		// BMP, TGA, JPEG by extension), with ReadPixels' timing. A texture that isn't a render target
		// is written last row first, so an image CreateFromFile loaded comes out as its file was.
		// RGBA8 textures only; any other format reports failure.
		// `done`, if given, reports whether the file was written.
		void SaveToFile(const std::filesystem::path& path, std::function<void(bool)> done = {});

		// Changed by every Reinitialize. Owners that cache a binding set built from this
		// texture's native handle (Material's per-framebuffer render passes) compare it at
		// bind time and re-bake on a mismatch: the handle is a *new* object after a
		// reload, so a cache that only re-bakes on demand would sample - and keep alive -
		// the original forever. Renderer2D needs no such check because it re-sets every
		// slot each flush. Every texture draws it from one counter, so a texture created
		// at a freed texture's address never matches what was cached for the old one.
		uint32_t GetGeneration() const { return m_Generation; }

		virtual uint32_t GetWidth() const { return m_Params.Width; }
		virtual uint32_t GetHeight() const { return m_Params.Height; }
		const TextureParams& GetParams() const { return m_Params; }
		virtual void* GetTextureHandle() const = 0;

	protected:
		static uint32_t NextGeneration();

	protected:
		TextureParams m_Params;
		uint32_t m_Generation = NextGeneration();
	};

}
