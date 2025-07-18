#include "depch.h"
#include "DingoEngine/Graphics/Texture.h"

#include "NVRHI/NvrhiTexture.h"

namespace Dingo
{

	Texture* Texture::Create(const TextureParams& params)
	{
		return new NvrhiTexture(params);
	}

}
