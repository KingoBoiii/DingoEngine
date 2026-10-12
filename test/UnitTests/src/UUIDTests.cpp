#include <doctest/doctest.h>

#include <DingoEngine/Asset/AssetTypes.h>
#include <DingoEngine/Core/UUID.h>

#include <unordered_set>

using namespace Dingo;

TEST_SUITE("UUID")
{
	TEST_CASE("a default id is 0, the invalid asset")
	{
		CHECK(static_cast<uint64_t>(UUID()) == 0);
		CHECK(AssetHandle() == k_InvalidAsset);
	}

	TEST_CASE("generated ids are never 0 and don't repeat")
	{
		std::unordered_set<UUID> seen;
		for (int i = 0; i < 100000; ++i)
		{
			const UUID id = UUID::Generate();
			REQUIRE(static_cast<uint64_t>(id) != 0);
			REQUIRE(seen.insert(id).second);
		}
	}

	TEST_CASE("the hash is the value")
	{
		CHECK(std::hash<UUID>()(UUID(1234567890123ull)) == static_cast<size_t>(1234567890123ull));
	}
}
