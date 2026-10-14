#include <doctest/doctest.h>

#include "DingoEngine/Graphics/ShadowAtlasAllocator.h"

#include <random>
#include <vector>

using Dingo::Internal::ShadowAtlasAllocator;

namespace
{
	struct Tile
	{
		glm::uvec2 Corner;
		uint32_t Size;
	};

	bool Overlap(const Tile& a, const Tile& b)
	{
		return a.Corner.x < b.Corner.x + b.Size && b.Corner.x < a.Corner.x + a.Size &&
			a.Corner.y < b.Corner.y + b.Size && b.Corner.y < a.Corner.y + a.Size;
	}
}

TEST_SUITE("ShadowAtlasAllocator")
{
	TEST_CASE("four quarters fill the atlas, a fifth fails")
	{
		ShadowAtlasAllocator allocator(4096);
		glm::uvec2 corner;
		std::vector<Tile> tiles;
		for (int i = 0; i < 4; ++i)
		{
			REQUIRE(allocator.Allocate(2048, corner));
			tiles.push_back({ corner, 2048 });
		}
		CHECK_FALSE(allocator.Allocate(2048, corner));
		CHECK_FALSE(allocator.Allocate(1, corner));
		for (size_t a = 0; a < tiles.size(); ++a)
			for (size_t b = a + 1; b < tiles.size(); ++b)
				CHECK_FALSE(Overlap(tiles[a], tiles[b]));
	}

	TEST_CASE("the first tile takes the corner and the smallest fitting square goes next")
	{
		ShadowAtlasAllocator allocator(4096);
		glm::uvec2 corner;
		REQUIRE(allocator.Allocate(1024, corner));
		CHECK(corner == glm::uvec2(0, 0));

		REQUIRE(allocator.Allocate(1024, corner));
		CHECK(corner.x < 2048);
		CHECK(corner.y < 2048);
	}

	TEST_CASE("a tile as large as the atlas, and one larger")
	{
		ShadowAtlasAllocator allocator(2048);
		glm::uvec2 corner;
		CHECK_FALSE(allocator.Allocate(4096, corner));
		REQUIRE(allocator.Allocate(2048, corner));
		CHECK(corner == glm::uvec2(0, 0));
		CHECK_FALSE(allocator.Allocate(64, corner));
	}

	TEST_CASE("random sizes never overlap, stay inside and are aligned to their size")
	{
		std::mt19937 random(7);
		for (int round = 0; round < 50; ++round)
		{
			ShadowAtlasAllocator allocator(4096);
			std::vector<Tile> tiles;
			uint64_t area = 0;
			for (int i = 0; i < 200; ++i)
			{
				const uint32_t size = 64u << (random() % 6);
				glm::uvec2 corner;
				if (!allocator.Allocate(size, corner))
					continue;
				const Tile tile{ corner, size };
				REQUIRE(corner.x + size <= 4096);
				REQUIRE(corner.y + size <= 4096);
				REQUIRE(corner.x % size == 0);
				REQUIRE(corner.y % size == 0);
				for (const Tile& other : tiles)
					REQUIRE_FALSE(Overlap(tile, other));
				tiles.push_back(tile);
				area += uint64_t(size) * size;
			}
			CHECK(area <= 4096ull * 4096ull);
		}
	}
}
