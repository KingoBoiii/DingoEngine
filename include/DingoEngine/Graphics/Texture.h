#pragma once
#include "DingoEngine/Graphics/Enums/TextureFormat.h"
#include "DingoEngine/Graphics/Enums/TextureDimension.h"
#include "DingoEngine/Graphics/IBindableShaderResource.h"

#include <filesystem>

namespace Dingo
{

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
		// of hot-reload. Binding caches keyed via NativeEquals re-bake naturally; the
		// old GPU texture is freed by the graphics backend once in-flight frames drop it.
		virtual void Reinitialize(const TextureParams& params) = 0;

		virtual bool NativeEquals(const Texture* other) const = 0;

		virtual uint32_t GetWidth() const { return m_Params.Width; }
		virtual uint32_t GetHeight() const { return m_Params.Height; }
		const TextureParams& GetParams() const { return m_Params; }
		virtual void* GetTextureHandle() const = 0;

	protected:
		TextureParams m_Params;
	};

}
