#include "depch.h"
#include "DingoEngine/Graphics/Texture.h"
#include "DingoEngine/Core/FileSystem.h"

#include "NVRHI/NvrhiTexture.h"

namespace Dingo
{
	
	Texture* Texture::CreateFromFile(const std::filesystem::path& filepath, const std::string& debugName)
	{
		uint32_t width = 0, height = 0, channels = 0;
		const uint8_t* data = FileSystem::ReadImage(filepath, &width, &height, &channels, true, true);
		if (!data)
			return nullptr;

		// Upload() copies the pixels synchronously during Create(), so the CPU-side
		// buffer can (and must) be released here — it used to leak per load.
		Texture* texture = Create(TextureParams()
			.SetDebugName(debugName)
			.SetWidth(width)
			.SetHeight(height)
			.SetDimension(TextureDimension::Texture2D)
			.SetFormat(channels == 4 ? Dingo::TextureFormat::RGBA : Dingo::TextureFormat::RGB)
			.SetIsRenderTarget(false)
			.SetInitialData(data));
		FileSystem::FreeImage(data);

		return texture;
	}

	Texture* Texture::CreateFromData(uint32_t width, uint32_t height, const void* data, TextureFormat format, const std::string& debugName)
	{
		return Create(TextureParams()
			.SetDebugName(debugName)
			.SetWidth(width)
			.SetHeight(height)
			.SetDimension(TextureDimension::Texture2D)
			.SetFormat(format)
			.SetIsRenderTarget(false)
			.SetInitialData(data));
	}

	Texture* Texture::Create(const TextureParams& params)
	{
		Texture* texture = new NvrhiTexture(params);
		texture->Initialize();

		if (params.InitialData)
		{
			texture->Upload(params.InitialData, static_cast<uint64_t>(params.Width) * (params.Format == TextureFormat::RGB ? 3 : 4));
		}

		return texture;
	}

}
