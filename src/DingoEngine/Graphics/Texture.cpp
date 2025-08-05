#include "depch.h"
#include "DingoEngine/Graphics/Texture.h"
#include "DingoEngine/Core/FileSystem.h"

#include "NVRHI/NvrhiTexture.h"

namespace Dingo
{
	
	Texture* Texture::CreateFromFile(const std::filesystem::path& filepath, const std::string& debugName)
	{
		uint32_t width, height, channels;
		const uint8_t* data = FileSystem::ReadImage("assets/textures/container.jpg", &width, &height, &channels, true, true);

		return Create(TextureParams()
			.SetDebugName(debugName)
			.SetWidth(width)
			.SetHeight(height)
			.SetDimension(TextureDimension::Texture2D)
			.SetFormat(channels == 4 ? Dingo::TextureFormat::RGBA : Dingo::TextureFormat::RGB)
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
