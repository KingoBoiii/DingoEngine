#include <doctest/doctest.h>

#include "DingoEngine/UI/DingoUI/DingoUITextCache.h"

#include <string>

using namespace Dingo::Internal;

TEST_CASE("UITextMetricsCache: a hit returns the stored size without measuring again")
{
	UITextMetricsCache cache;
	int measured = 0;
	const auto measure = [&] { ++measured; return glm::vec2(40.0f, 18.0f); };

	const glm::vec2 first = cache.Get(1, 18.0f, 0.0f, "Play", measure);
	const glm::vec2 second = cache.Get(1, 18.0f, 0.0f, "Play", measure);
	CHECK(measured == 1);
	CHECK(first == second);
	CHECK(cache.Hits() == 1);
	CHECK(cache.Misses() == 1);
}

TEST_CASE("UITextMetricsCache: font, size, wrap width and text each make a different entry")
{
	UITextMetricsCache cache;
	int measured = 0;
	const auto measure = [&] { ++measured; return glm::vec2(1.0f); };

	cache.Get(1, 18.0f, 0.0f, "Play", measure);
	cache.Get(2, 18.0f, 0.0f, "Play", measure);
	cache.Get(1, 20.0f, 0.0f, "Play", measure);
	cache.Get(1, 18.0f, 100.0f, "Play", measure);
	cache.Get(1, 18.0f, 0.0f, "Quit", measure);
	CHECK(measured == 5);
	CHECK(cache.Size() == 5);
}

TEST_CASE("UITextMetricsCache: a substring view of a longer buffer hits the same entry as an equal string")
{
	UITextMetricsCache cache;
	int measured = 0;
	const auto measure = [&] { ++measured; return glm::vec2(7.0f, 3.0f); };

	const std::string buffer = "xxPlayyy";
	cache.Get(1, 18.0f, 0.0f, std::string_view(buffer).substr(2, 4), measure);
	cache.Get(1, 18.0f, 0.0f, "Play", measure);
	CHECK(measured == 1);
}

TEST_CASE("UITextMetricsCache: an entry survives one idle frame and is dropped after two")
{
	UITextMetricsCache cache;
	int measured = 0;
	const auto measure = [&] { ++measured; return glm::vec2(5.0f); };

	cache.Get(1, 18.0f, 0.0f, "Play", measure);
	cache.Advance();
	cache.Advance();
	CHECK(cache.Size() == 0);
	cache.Get(1, 18.0f, 0.0f, "Play", measure);
	CHECK(measured == 2);

	cache.Advance();
	cache.Get(1, 18.0f, 0.0f, "Play", measure);
	cache.Advance();
	cache.Get(1, 18.0f, 0.0f, "Play", measure);
	CHECK(measured == 2);
}
