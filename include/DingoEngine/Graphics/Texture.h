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
	};

	class Texture : public IBindableShaderResource
	{
	public:
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
		virtual void Upload(const std::filesystem::path& filepath) = 0;

		virtual bool NativeEquals(const Texture* other) const = 0;

		virtual void* GetTextureHandle() const = 0;

	protected:
		TextureParams m_Params;
	};

}
