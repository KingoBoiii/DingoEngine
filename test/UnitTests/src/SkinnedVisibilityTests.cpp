#include <doctest/doctest.h>

#include "DingoEngine/Graphics/SkinnedVisibility.h"

#include <vector>

using Dingo::Internal::SkinnedVisibility;

TEST_SUITE("SkinnedVisibility")
{
	TEST_CASE("a palette counts as drawn in its frame and the next")
	{
		std::vector<glm::mat4> palettes(4, glm::mat4(1.0f));
		SkinnedVisibility visibility;
		CHECK_FALSE(visibility.WasDrawn(0, &palettes[0]));

		visibility.BeginScene(10);
		visibility.Record(10, &palettes[2]);
		visibility.Record(10, &palettes[0]);
		visibility.BeginScene(10);
		visibility.Record(10, &palettes[3]);
		CHECK(visibility.WasDrawn(10, &palettes[0]));
		CHECK(visibility.WasDrawn(10, &palettes[2]));
		CHECK(visibility.WasDrawn(10, &palettes[3]));
		CHECK_FALSE(visibility.WasDrawn(10, &palettes[1]));
		CHECK(visibility.WasDrawn(11, &palettes[3]));
		CHECK_FALSE(visibility.WasDrawn(12, &palettes[3]));

		visibility.Record(10, &palettes[1]);
		CHECK(visibility.WasDrawn(10, &palettes[1]));

		visibility.BeginScene(11);
		visibility.Record(11, &palettes[1]);
		visibility.BeginScene(12);
		CHECK(visibility.WasDrawn(12, &palettes[1]));
		CHECK_FALSE(visibility.WasDrawn(12, &palettes[0]));
	}
}
