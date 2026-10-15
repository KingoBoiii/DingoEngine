#include <doctest/doctest.h>

#include "DingoEngine/Graphics/SkinnedVisibility.h"

#include <vector>

using Dingo::Internal::SkinnedViewStamp;
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

	TEST_CASE("a skipped frame leaves the scene animating as if seen")
	{
		std::vector<glm::mat4> palettes(2, glm::mat4(1.0f));
		SkinnedVisibility visibility;
		SkinnedViewStamp stamp;

		stamp.Stamp(20, false);
		visibility.BeginScene(20);
		visibility.Record(20, &palettes[0]);
		CHECK(stamp.RenderedLately(21));

		// Frame 21 acquired no swap-chain image: the scene stamps, the renderer records nothing.
		stamp.Stamp(21, true);
		CHECK_FALSE(visibility.WasDrawn(22, &palettes[0]));
		CHECK_FALSE(stamp.RenderedLately(22));

		stamp.Stamp(22, false);
		visibility.BeginScene(22);
		visibility.Record(22, &palettes[1]);
		CHECK(stamp.RenderedLately(23));
		CHECK(visibility.WasDrawn(23, &palettes[1]));
		CHECK_FALSE(visibility.WasDrawn(23, &palettes[0]));
	}
}
