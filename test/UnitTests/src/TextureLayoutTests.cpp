#include <doctest/doctest.h>

#include "DingoEngine/Graphics/HalfFloat.h"
#include "DingoEngine/Graphics/TextureLayout.h"

#include <cmath>
#include <limits>

using namespace Dingo;
using namespace Dingo::Internal;

TEST_SUITE("TextureLayout")
{
	TEST_CASE("the mip chain runs to 1x1 along the largest side")
	{
		CHECK(GetMipLevelCount(16, 16) == 5);
		CHECK(GetMipLevelCount(32, 16) == 6);
		CHECK(GetMipLevelCount(1, 1) == 1);
		CHECK(GetMipLevelCount(16, 16, 8) == 5);
		CHECK(GetMipLevelCount(4, 4, 64) == 7);
	}

	TEST_CASE("a mip extent halves each side down to 1")
	{
		const TextureExtent e = GetMipExtent(32, 16, 8, 2);
		CHECK(e.Width == 8);
		CHECK(e.Height == 4);
		CHECK(e.Depth == 2);
		const TextureExtent last = GetMipExtent(32, 16, 1, 5);
		CHECK(last.Width == 1);
		CHECK(last.Height == 1);
		CHECK(last.Depth == 1);
	}

	TEST_CASE("a subresource's bytes cover its whole volume")
	{
		TextureParams params = TextureParams().SetWidth(16).SetHeight(16).SetDepth(8).SetFormat(TextureFormat::RGBA16F).SetDimension(TextureDimension::Texture3D);
		CHECK(GetSubresourceBytes(params, 0) == 16u * 16u * 8u * 8u);
		CHECK(GetSubresourceBytes(params, 1) == 8u * 8u * 4u * 8u);
	}

	TEST_CASE("normalising fills defaults")
	{
		std::string error;
		TextureParams cube = TextureParams().SetWidth(16).SetHeight(16).SetFormat(TextureFormat::RGBA8_UNORM).SetDimension(TextureDimension::TextureCube).SetMipLevels(0);
		CHECK(NormalizeTextureParams(cube, error));
		CHECK(cube.ArrayLayers == 6);
		CHECK(cube.MipLevels == 5);

		TextureParams plain = TextureParams().SetWidth(8).SetHeight(8).SetFormat(TextureFormat::RGBA8_UNORM).SetDimension(TextureDimension::Texture2D);
		plain.Depth = 0;
		plain.ArrayLayers = 0;
		CHECK(NormalizeTextureParams(plain, error));
		CHECK(plain.Depth == 1);
		CHECK(plain.ArrayLayers == 1);
		CHECK(plain.MipLevels == 1);

		TextureParams array = TextureParams().SetWidth(8).SetHeight(8).SetArrayLayers(3).SetFormat(TextureFormat::RGBA8_UNORM).SetDimension(TextureDimension::Texture2D);
		CHECK(NormalizeTextureParams(array, error));
	}

	TEST_CASE("normalising rejects combinations the dimension can't hold")
	{
		std::string error;
		auto base = [] { return TextureParams().SetWidth(16).SetHeight(16).SetFormat(TextureFormat::RGBA8_UNORM).SetDimension(TextureDimension::Texture2D); };

		TextureParams p = base().SetDimension(TextureDimension::Unknown);
		CHECK_FALSE(NormalizeTextureParams(p, error));
		p = base().SetWidth(0);
		CHECK_FALSE(NormalizeTextureParams(p, error));
		p = base().SetDepth(4);
		CHECK_FALSE(NormalizeTextureParams(p, error));
		p = base().SetDimension(TextureDimension::TextureCube).SetHeight(8);
		CHECK_FALSE(NormalizeTextureParams(p, error));
		p = base().SetDimension(TextureDimension::TextureCube).SetArrayLayers(4);
		CHECK_FALSE(NormalizeTextureParams(p, error));
		p = base().SetDimension(TextureDimension::Texture3D).SetArrayLayers(2);
		CHECK_FALSE(NormalizeTextureParams(p, error));
		p = base().SetMipLevels(6);
		CHECK_FALSE(NormalizeTextureParams(p, error));
		p = base().SetIsRenderTarget(true).SetArrayLayers(2);
		CHECK_FALSE(NormalizeTextureParams(p, error));
		p = base().SetFormat(TextureFormat::D32).SetMipLevels(2);
		CHECK_FALSE(NormalizeTextureParams(p, error));
		p = base().SetFormat(TextureFormat::D32).SetDimension(TextureDimension::TextureCube);
		CHECK_FALSE(NormalizeTextureParams(p, error));
		CHECK_FALSE(error.empty());
	}
}

TEST_SUITE("HalfFloat")
{
	TEST_CASE("values a half holds round trip exactly")
	{
		for (float v : { 0.0f, 1.0f, 0.5f, 2.0f, 1024.0f, 65504.0f, 0.000061035156f, 5.9604645e-8f, 0.333251953125f })
			CHECK(HalfToFloat(FloatToHalf(v)) == v);
		for (uint32_t h = 0; h < 0x7C00u; ++h)
			CHECK(FloatToHalf(HalfToFloat(static_cast<uint16_t>(h))) == h);
	}

	TEST_CASE("rounding is to nearest even")
	{
		CHECK(FloatToHalf(1.0f + 1.0f / 2048.0f) == 0x3C00u);
		CHECK(FloatToHalf(1.0f + 3.0f / 2048.0f) == 0x3C02u);
		CHECK(FloatToHalf(1.0f + 1.0f / 2048.0f + 1.0f / 65536.0f) == 0x3C01u);
		CHECK(FloatToHalf(std::ldexp(1.0f, -25)) == 0u);
		CHECK(FloatToHalf(std::ldexp(1.5f, -25)) == 1u);
	}

	TEST_CASE("NaN and negatives become 0 and large values clamp to 65504")
	{
		CHECK(FloatToHalf(std::numeric_limits<float>::quiet_NaN()) == 0u);
		CHECK(FloatToHalf(-3.0f) == 0u);
		CHECK(FloatToHalf(-std::numeric_limits<float>::infinity()) == 0u);
		CHECK(FloatToHalf(std::numeric_limits<float>::infinity()) == 0x7BFFu);
		CHECK(FloatToHalf(1.0e9f) == 0x7BFFu);
		CHECK(FloatToHalf(65519.0f) == 0x7BFFu);
	}
}
