#pragma once
#include "DingoEngine/Graphics/Texture.h"

#include <string>

namespace Dingo::Internal
{

	struct TextureExtent
	{
		uint32_t Width = 1;
		uint32_t Height = 1;
		uint32_t Depth = 1;
	};

	inline uint32_t GetTextureArrayLayers(const TextureParams& params)
	{
		return params.Dimension == TextureDimension::TextureCube ? 6u : (params.ArrayLayers > 0 ? params.ArrayLayers : 1u);
	}

	inline TextureExtent GetMipExtent(uint32_t width, uint32_t height, uint32_t depth, uint32_t mip)
	{
		auto shrink = [mip](uint32_t size) { return mip >= 32 ? 1u : ((size >> mip) > 0 ? (size >> mip) : 1u); };
		return { shrink(width), shrink(height), shrink(depth) };
	}

	inline TextureExtent GetMipExtent(const TextureParams& params, uint32_t mip)
	{
		return GetMipExtent(params.Width, params.Height, params.Depth, mip);
	}

	// Packed bytes of one mip of one layer; for a 3D texture, of the whole volume.
	inline uint64_t GetSubresourceBytes(const TextureParams& params, uint32_t mip)
	{
		const TextureExtent extent = GetMipExtent(params, mip);
		return static_cast<uint64_t>(extent.Width) * extent.Height * extent.Depth * GetBytesPerPixel(params.Format);
	}

	// Checks params against the dimension rules and fills in the defaults (Depth/ArrayLayers 0 -> 1,
	// cube -> 6 layers, MipLevels 0 -> the full chain). On failure params is left unspecified and
	// error says why.
	inline bool NormalizeTextureParams(TextureParams& params, std::string& error)
	{
		const bool depthTarget = params.Format == TextureFormat::D32;

		if (params.Dimension == TextureDimension::Unknown || params.Dimension >= TextureDimension::Count)
		{
			error = "the dimension isn't set";
			return false;
		}
		if (params.Width == 0 || params.Height == 0)
		{
			error = "Width and Height must be at least 1";
			return false;
		}

		if (params.Depth == 0)
			params.Depth = 1;
		if (params.ArrayLayers == 0)
			params.ArrayLayers = 1;

		if (params.Depth > 1 && params.Dimension != TextureDimension::Texture3D)
		{
			error = "Depth above 1 needs Texture3D";
			return false;
		}
		if (params.Dimension == TextureDimension::TextureCube)
		{
			if (params.Width != params.Height)
			{
				error = "a cube texture must be square";
				return false;
			}
			if (params.ArrayLayers != 1 && params.ArrayLayers != 6)
			{
				error = "a cube texture has 6 layers";
				return false;
			}
			params.ArrayLayers = 6;
		}
		else if (params.ArrayLayers > 1 && params.Dimension != TextureDimension::Texture2D)
		{
			error = "ArrayLayers above 1 needs Texture2D or TextureCube";
			return false;
		}

		const uint32_t chain = GetMipLevelCount(params.Width, params.Height, params.Dimension == TextureDimension::Texture3D ? params.Depth : 1u);
		if (params.MipLevels == 0)
			params.MipLevels = chain;
		else if (params.MipLevels > chain)
		{
			error = "MipLevels is longer than the mip chain";
			return false;
		}

		if (depthTarget || params.IsRenderTarget)
		{
			if (params.Dimension != TextureDimension::Texture2D || params.ArrayLayers != 1)
			{
				error = "a render target or depth texture must be a single-layer Texture2D";
				return false;
			}
			if (depthTarget && params.MipLevels != 1)
			{
				error = "a depth texture has one mip level";
				return false;
			}
		}
		return true;
	}

}
