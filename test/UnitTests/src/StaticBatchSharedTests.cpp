#include <doctest/doctest.h>

#include "DingoEngine/Graphics/StaticBatchData.h"

#include <algorithm>
#include <random>
#include <vector>

using Dingo::Internal::StaticBatchShared;

namespace
{
	using Range = std::pair<uint32_t, uint32_t>;

	bool Sorted(const StaticBatchShared& shared)
	{
		for (size_t i = 1; i < shared.FreeRanges.size(); ++i)
		{
			const Range& previous = shared.FreeRanges[i - 1];
			if (previous.first + previous.second >= shared.FreeRanges[i].first)
				return false;
		}
		return true;
	}
}

TEST_SUITE("StaticBatchShared")
{
	TEST_CASE("a hole left by a middle release is filled first fit, and everything given back leaves one record")
	{
		StaticBatchShared shared;
		const uint32_t a = shared.Allocate(3);
		const uint32_t b = shared.Allocate(2);
		const uint32_t c = shared.Allocate(4);
		CHECK(a == 1);
		CHECK(b == 4);
		CHECK(c == 6);
		CHECK(shared.Records.size() == 10);

		shared.Release(b, 2);
		REQUIRE(shared.FreeRanges.size() == 1);
		CHECK(shared.FreeRanges[0] == Range{ 4, 2 });
		CHECK(shared.Records.size() == 10);

		const uint32_t d = shared.Allocate(1);
		const uint32_t e = shared.Allocate(1);
		CHECK(d == 4);
		CHECK(e == 5);
		CHECK(shared.FreeRanges.empty());
		CHECK(shared.Records.size() == 10);

		shared.Release(c, 4);
		CHECK(shared.Records.size() == 6);
		shared.Release(a, 3);
		REQUIRE(shared.FreeRanges.size() == 1);
		CHECK(shared.FreeRanges[0] == Range{ 1, 3 });
		shared.Release(e, 1);
		CHECK(shared.Records.size() == 5);
		shared.Release(d, 1);
		CHECK(shared.FreeRanges.empty());
		CHECK(shared.Records.size() == 1);
	}

	TEST_CASE("a release between two free ranges merges all three")
	{
		StaticBatchShared shared;
		const uint32_t first = shared.Allocate(2);
		const uint32_t second = shared.Allocate(3);
		const uint32_t third = shared.Allocate(2);
		const uint32_t last = shared.Allocate(1);
		shared.Release(first, 2);
		shared.Release(third, 2);
		REQUIRE(shared.FreeRanges.size() == 2);

		shared.Release(second, 3);
		REQUIRE(shared.FreeRanges.size() == 1);
		CHECK(shared.FreeRanges[0] == Range{ first, 7 });
		CHECK(shared.Records.size() == last + 1);

		shared.Release(last, 1);
		CHECK(shared.FreeRanges.empty());
		CHECK(shared.Records.size() == 1);
	}

	TEST_CASE("the free tail is given back after a merge, not only after a plain release")
	{
		StaticBatchShared shared;
		const uint32_t a = shared.Allocate(2);
		const uint32_t b = shared.Allocate(2);
		const uint32_t c = shared.Allocate(2);
		shared.Release(a, 2);
		shared.Release(c, 2);
		CHECK(shared.Records.size() == 5);
		REQUIRE(shared.FreeRanges.size() == 1);
		CHECK(shared.FreeRanges[0] == Range{ a, 2 });

		shared.Release(b, 2);
		CHECK(shared.FreeRanges.empty());
		CHECK(shared.Records.size() == 1);
	}

	TEST_CASE("a request larger than every hole grows the records, and zero counts are ignored")
	{
		StaticBatchShared shared;
		const uint32_t a = shared.Allocate(1);
		shared.Allocate(3);
		shared.Release(a, 1);
		CHECK(shared.Allocate(2) == 5);
		CHECK(shared.Records.size() == 7);
		REQUIRE(shared.FreeRanges.size() == 1);

		const size_t size = shared.Records.size();
		CHECK(shared.Allocate(0) == 0);
		shared.Release(3, 0);
		CHECK(shared.Records.size() == size);
		CHECK(shared.FreeRanges.size() == 1);
	}

	TEST_CASE("random allocation and release never overlap, keep the free list sorted and unmerged, and end empty")
	{
		std::mt19937 random(7);
		StaticBatchShared shared;
		std::vector<Range> live;

		auto verify = [&]
		{
			REQUIRE(Sorted(shared));
			if (!shared.FreeRanges.empty())
				REQUIRE(shared.FreeRanges.back().first + shared.FreeRanges.back().second < shared.Records.size());
			std::vector<uint32_t> covered(shared.Records.size(), 0);
			for (const Range& range : live)
				for (uint32_t i = range.first; i < range.first + range.second; ++i)
					++covered[i];
			for (const Range& range : shared.FreeRanges)
				for (uint32_t i = range.first; i < range.first + range.second; ++i)
					++covered[i];
			for (uint32_t i = 1; i < covered.size(); ++i)
				REQUIRE(covered[i] == 1);
		};

		for (uint32_t step = 0; step < 2000; ++step)
		{
			if (live.empty() || random() % 5 < 3)
			{
				const uint32_t count = 1 + random() % 5;
				live.emplace_back(shared.Allocate(count), count);
			}
			else
			{
				const size_t pick = random() % live.size();
				shared.Release(live[pick].first, live[pick].second);
				live.erase(live.begin() + static_cast<std::ptrdiff_t>(pick));
			}
			verify();
		}

		std::shuffle(live.begin(), live.end(), random);
		for (const Range& range : live)
			shared.Release(range.first, range.second);
		live.clear();
		verify();
		CHECK(shared.FreeRanges.empty());
		CHECK(shared.Records.size() == 1);
	}
}
