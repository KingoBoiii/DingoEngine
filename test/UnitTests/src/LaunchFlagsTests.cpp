#include <doctest/doctest.h>

#include "DingoEngine/Core/LaunchFlags.h"

using namespace Dingo;
using namespace Dingo::Internal;

TEST_SUITE("LaunchFlags")
{
	TEST_CASE("--graphics names every backend")
	{
		CHECK(ParseGraphicsFlag("vulkan") == GraphicsAPI::Vulkan);
		CHECK(ParseGraphicsFlag("dx11") == GraphicsAPI::DirectX11);
		CHECK(ParseGraphicsFlag("directx11") == GraphicsAPI::DirectX11);
		CHECK(ParseGraphicsFlag("dx12") == GraphicsAPI::DirectX12);
		CHECK(ParseGraphicsFlag("directx12") == GraphicsAPI::DirectX12);
		CHECK(ParseGraphicsFlag("headless") == GraphicsAPI::Headless);
	}

	TEST_CASE("--graphics rejects anything else")
	{
		CHECK_FALSE(ParseGraphicsFlag("").has_value());
		CHECK_FALSE(ParseGraphicsFlag("Vulkan").has_value());
		CHECK_FALSE(ParseGraphicsFlag("dx9").has_value());
		CHECK_FALSE(ParseGraphicsFlag("vulkan ").has_value());
	}

	TEST_CASE("a bare flag is on, and off, false, 0 and no turn it off")
	{
		CHECK(IsLaunchFlagOn(std::string_view{}));
		CHECK(IsLaunchFlagOn(std::string_view("1")));
		CHECK_FALSE(IsLaunchFlagOn(std::nullopt));
		CHECK_FALSE(IsLaunchFlagOn(std::string_view("off")));
		CHECK_FALSE(IsLaunchFlagOn(std::string_view("false")));
		CHECK_FALSE(IsLaunchFlagOn(std::string_view("0")));
		CHECK_FALSE(IsLaunchFlagOn(std::string_view("no")));
	}

	TEST_CASE("--window reads WxH")
	{
		CHECK(ParseWindowFlag("1280x720") == glm::ivec2(1280, 720));
		CHECK(ParseWindowFlag("1920X1080") == glm::ivec2(1920, 1080));
		CHECK(ParseWindowFlag("640x360") == glm::ivec2(640, 360));
		CHECK(ParseWindowFlag("16384x16384") == glm::ivec2(16384, 16384));
	}

	TEST_CASE("--window ignores sizes outside 640x360 to 16384x16384 and malformed text")
	{
		CHECK_FALSE(ParseWindowFlag("16385x720").has_value());
		CHECK_FALSE(ParseWindowFlag("1280x16385").has_value());
		CHECK_FALSE(ParseWindowFlag("639x720").has_value());
		CHECK_FALSE(ParseWindowFlag("1280x359").has_value());
		CHECK_FALSE(ParseWindowFlag("-1280x720").has_value());
		CHECK_FALSE(ParseWindowFlag("").has_value());
		CHECK_FALSE(ParseWindowFlag("1280").has_value());
		CHECK_FALSE(ParseWindowFlag("1280x").has_value());
		CHECK_FALSE(ParseWindowFlag("x720").has_value());
		CHECK_FALSE(ParseWindowFlag("1280x720x").has_value());
		CHECK_FALSE(ParseWindowFlag("1280*720").has_value());
		CHECK_FALSE(ParseWindowFlag("99999999999x720").has_value());
	}
}
