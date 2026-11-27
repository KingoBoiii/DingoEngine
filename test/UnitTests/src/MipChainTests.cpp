#include <doctest/doctest.h>

#include "DingoEngine/Graphics/MipChain.h"

#include <cmath>
#include <vector>

using namespace Dingo::Internal;

namespace
{
	std::vector<uint8_t> Solid(uint32_t width, uint32_t height, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
	{
		std::vector<uint8_t> image;
		for (uint32_t i = 0; i < width * height; ++i)
			image.insert(image.end(), { r, g, b, a });
		return image;
	}
}

TEST_SUITE("MipChain")
{
	TEST_CASE("a 2x2 box average gives the 1x1 mean, rounded to nearest")
	{
		const std::vector<uint8_t> image = {
			0, 0, 0, 255,   10, 20, 30, 255,
			20, 40, 60, 255,   30, 61, 90, 0,
		};
		const std::vector<uint8_t> mip = DownsampleRgba8(image.data(), 2, 2, MipFilter::Color);
		REQUIRE(mip.size() == 4);
		CHECK(mip[0] == 15);
		CHECK(mip[1] == 30);
		CHECK(mip[2] == 45);
		CHECK(mip[3] == 191);
	}

	TEST_CASE("the chain runs to 1x1 with the layout's mip sizes")
	{
		const std::vector<uint8_t> image = Solid(8, 4, 100, 150, 200, 255);
		const auto mips = BuildMipChain(image.data(), 8, 4, MipFilter::Color);
		REQUIRE(mips.size() == Dingo::GetMipLevelCount(8, 4) - 1);
		for (size_t i = 0; i < mips.size(); ++i)
		{
			const TextureExtent extent = GetMipExtent(8, 4, 1, static_cast<uint32_t>(i + 1));
			CHECK(mips[i].size() == static_cast<size_t>(extent.Width) * extent.Height * 4);
			CHECK(mips[i][0] == 100);
			CHECK(mips[i][2] == 200);
		}
		CHECK(mips.back().size() == 4);
	}

	TEST_CASE("odd sizes lose no texel and match the layout's extent")
	{
		std::vector<uint8_t> image;
		for (uint32_t x = 0; x < 5; ++x)
			image.insert(image.end(), { static_cast<uint8_t>(x * 10), 0, 0, 255 });

		const std::vector<uint8_t> mip = DownsampleRgba8(image.data(), 5, 1, MipFilter::Color);
		REQUIRE(mip.size() == 2 * 4);
		CHECK(mip[0] == 5);
		CHECK(mip[4] == 30);

		const std::vector<uint8_t> three = DownsampleRgba8(image.data(), 3, 1, MipFilter::Color);
		REQUIRE(three.size() == 4);
		CHECK(three[0] == 10);

		const auto chain = BuildMipChain(Solid(5, 3, 1, 2, 3, 4).data(), 5, 3, MipFilter::Color);
		REQUIRE(chain.size() == 2);
		CHECK(chain[0].size() == 2 * 1 * 4);
		CHECK(chain[1].size() == 4);
	}

	TEST_CASE("a 1x1 image has no further mips")
	{
		const std::vector<uint8_t> image = Solid(1, 1, 9, 9, 9, 9);
		CHECK(BuildMipChain(image.data(), 1, 1, MipFilter::Color).empty());
	}

	TEST_CASE("normals are averaged as vectors and renormalised")
	{
		const std::vector<uint8_t> image = {
			204, 128, 255, 255,   51, 128, 255, 255,
		};
		const std::vector<uint8_t> mip = DownsampleRgba8(image.data(), 2, 1, MipFilter::Normal);
		REQUIRE(mip.size() == 4);
		CHECK(std::abs(static_cast<int>(mip[0]) - 128) <= 1);
		CHECK(std::abs(static_cast<int>(mip[1]) - 128) <= 1);
		CHECK(mip[2] == 255);

		const std::vector<uint8_t> tilted = {
			204, 128, 230, 255,   204, 128, 230, 255,
		};
		const std::vector<uint8_t> same = DownsampleRgba8(tilted.data(), 2, 1, MipFilter::Normal);
		const float x = same[0] / 255.0f * 2.0f - 1.0f;
		const float y = same[1] / 255.0f * 2.0f - 1.0f;
		const float z = same[2] / 255.0f * 2.0f - 1.0f;
		CHECK(std::sqrt(x * x + y * y + z * z) == doctest::Approx(1.0f).epsilon(0.02f));
	}

	TEST_CASE("opposite normals fall back to a flat normal")
	{
		const std::vector<uint8_t> image = {
			255, 255, 255, 255,   0, 0, 0, 255,
		};
		const std::vector<uint8_t> mip = DownsampleRgba8(image.data(), 2, 1, MipFilter::Normal);
		CHECK(std::abs(static_cast<int>(mip[0]) - 128) <= 1);
		CHECK(std::abs(static_cast<int>(mip[1]) - 128) <= 1);
		CHECK(mip[2] == 255);
	}

	TEST_CASE("sRGB colours average in linear light: 0 and 255 give about 188, not 128")
	{
		const std::vector<uint8_t> image = {
			0, 0, 0, 255,   255, 255, 255, 255,
		};
		const std::vector<uint8_t> mip = DownsampleRgba8(image.data(), 2, 1, MipFilter::Srgb);
		REQUIRE(mip.size() == 4);
		for (int c = 0; c < 3; ++c)
			CHECK(std::abs(static_cast<int>(mip[c]) - 188) <= 1);
		CHECK(mip[3] == 255);
	}

	TEST_CASE("sRGB filtering keeps a solid colour and averages alpha linearly")
	{
		const std::vector<uint8_t> solid = Solid(2, 2, 40, 128, 250, 255);
		const std::vector<uint8_t> same = DownsampleRgba8(solid.data(), 2, 2, MipFilter::Srgb);
		CHECK(same[0] == 40);
		CHECK(same[1] == 128);
		CHECK(same[2] == 250);

		const std::vector<uint8_t> image = {
			10, 10, 10, 0,   10, 10, 10, 255,
		};
		const std::vector<uint8_t> mip = DownsampleRgba8(image.data(), 2, 1, MipFilter::Srgb);
		CHECK(mip[0] == 10);
		CHECK(mip[3] == 128);
	}
}
