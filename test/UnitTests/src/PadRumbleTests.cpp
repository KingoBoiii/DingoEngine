#include <doctest/doctest.h>

#include "DingoEngine/Core/PadRumble.h"

using Dingo::Internal::PadRumble::DeviceIds;
using Dingo::Internal::PadRumble::ParseGlfwGuid;

namespace
{
	bool Same(const DeviceIds& a, const DeviceIds& b)
	{
		return a.Bus == b.Bus && a.Vendor == b.Vendor && a.Product == b.Product && a.Version == b.Version;
	}
}

TEST_SUITE("PadRumble")
{
	TEST_CASE("a Linux evdev GUID gives bus, vendor, product and version")
	{
		CHECK(Same(ParseGlfwGuid("030000004c050000e60c000011810000"), DeviceIds{ 0x0003, 0x054c, 0x0ce6, 0x8111 }));
		CHECK(Same(ParseGlfwGuid("050000004C050000CC09000000810000"), DeviceIds{ 0x0005, 0x054c, 0x09cc, 0x8100 }));
	}

	TEST_CASE("a Windows DirectInput GUID gives vendor and product with version 0")
	{
		CHECK(Same(ParseGlfwGuid("030000004c050000e60c000000000000"), DeviceIds{ 0x0003, 0x054c, 0x0ce6, 0x0000 }));
	}

	TEST_CASE("GUIDs without ids read as zero")
	{
		const DeviceIds none{};
		CHECK(Same(ParseGlfwGuid("78696e70757401000000000000000000"), none));  // XInput
		CHECK(Same(ParseGlfwGuid("05000000476f6f676c652050616400"), none));     // too short
		CHECK(Same(ParseGlfwGuid("050000004753205061642056320000"), none));
		CHECK(Same(ParseGlfwGuid("0500000047532050616420563200000z"), none));  // not hex
		CHECK(Same(ParseGlfwGuid("05000000476f6f676c65205061640000"), none));  // built from the name
		CHECK(Same(ParseGlfwGuid(nullptr), none));
	}
}
