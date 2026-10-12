#include <doctest/doctest.h>

#include <DingoEngine/Graphics/AnimationClip.h>
#include "DingoEngine/Graphics/EventsFile.h"

using namespace Dingo;
using Result = Internal::EventsLine::Result;

TEST_SUITE("EventsFile")
{
	TEST_CASE("an instant")
	{
		const Internal::EventsLine line = Internal::ParseEventsLine("Walk 0.25 step_l");
		REQUIRE(line.Status == Result::Event);
		CHECK(line.Clip == "Walk");
		CHECK(line.Name == "step_l");
		CHECK(line.Begin == doctest::Approx(0.25f));
		CHECK(line.End == line.Begin);
		CHECK_FALSE(line.Range);
		CHECK(line.Payload.empty());
	}

	TEST_CASE("a range, with whitespace and a trailing comment")
	{
		const Internal::EventsLine line = Internal::ParseEventsLine("  Attack\t0.1..0.4   hitbox  # the blade's sweep");
		REQUIRE(line.Status == Result::Event);
		CHECK(line.Clip == "Attack");
		CHECK(line.Name == "hitbox");
		CHECK(line.Range);
		CHECK(line.Begin == doctest::Approx(0.1f));
		CHECK(line.End == doctest::Approx(0.4f));
		CHECK(line.Time == "0.1..0.4");
	}

	TEST_CASE("blank lines and comments are empty")
	{
		CHECK(Internal::ParseEventsLine("").Status == Result::Empty);
		CHECK(Internal::ParseEventsLine("   \t ").Status == Result::Empty);
		CHECK(Internal::ParseEventsLine("# Walk 0.5 step").Status == Result::Empty);
	}

	TEST_CASE("quoted names hold spaces and #")
	{
		const Internal::EventsLine line = Internal::ParseEventsLine("\"Run Fast\" 1 \"foot # left\"");
		REQUIRE(line.Status == Result::Event);
		CHECK(line.Clip == "Run Fast");
		CHECK(line.Name == "foot # left");
	}

	TEST_CASE("the payload is rejoined so AnimationEventPayload reads it")
	{
		const Internal::EventsLine line = Internal::ParseEventsLine("Walk 0.5 step surface=\"wet stone\" volume=0.8 count=3 loud \"two words\"");
		REQUIRE(line.Status == Result::Event);
		CHECK(line.Payload == "surface=\"wet stone\" volume=0.8 count=3 loud \"two words\"");

		const AnimationEventPayload payload{ line.Payload };
		CHECK(payload.GetString("surface") == "wet stone");
		CHECK(payload.GetFloat("volume") == doctest::Approx(0.8f));
		CHECK(payload.GetInt("count") == 3);
		CHECK(payload.Has("loud"));
		CHECK_FALSE(payload.Has("quiet"));
		CHECK(payload.GetInt("volume", -1) == -1);
		CHECK(payload.GetString("missing", "none") == "none");
	}

	TEST_CASE("malformed lines")
	{
		CHECK(Internal::ParseEventsLine("Walk 0.5 \"step").Status == Result::UnclosedQuote);
		CHECK(Internal::ParseEventsLine("Walk 0.5 step key=\"open").Status == Result::UnclosedQuote);
		CHECK(Internal::ParseEventsLine("Walk 0.5").Status == Result::TooFewTokens);

		const Internal::EventsLine badTime = Internal::ParseEventsLine("Walk soon step");
		CHECK(badTime.Status == Result::BadTime);
		CHECK(badTime.Clip == "Walk");
		CHECK(Internal::ParseEventsLine("Walk 0.5s step").Status == Result::BadTime);
		CHECK(Internal::ParseEventsLine("Walk inf step").Status == Result::BadTime);
		CHECK(Internal::ParseEventsLine("Walk 0.1..x step").Status == Result::BadTime);
		CHECK(Internal::ParseEventsLine("Walk 0.4..0.1 step").Status == Result::ReversedRange);
	}

	TEST_CASE("a time outside the clip still parses; LoadEvents warns about it")
	{
		const Internal::EventsLine line = Internal::ParseEventsLine("Walk -0.5 step");
		CHECK(line.Status == Result::Event);
		CHECK(line.Begin == doctest::Approx(-0.5f));
	}
}
