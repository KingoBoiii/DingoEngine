#pragma once
#include "DingoEngine/Graphics/Enums/TextureFormat.h"
#include "DingoEngine/Graphics/Enums/TextureDimension.h"

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
	};

	class Texture
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

		virtual void* GetTextureHandle() const = 0;

	protected:
		TextureParams m_Params;
	};

}
