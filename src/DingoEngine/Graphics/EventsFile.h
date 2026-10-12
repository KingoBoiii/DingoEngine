#pragma once

// Engine-internal: one line of a model's `.events` sidecar (Model::LoadEvents), parsed without the
// model, so the format is testable on its own.

#include <string>
#include <string_view>

namespace Dingo::Internal
{

	struct EventsLine
	{
		enum class Result
		{
			Empty,          // blank or a comment
			Event,
			UnclosedQuote,
			TooFewTokens,
			BadTime,        // Clip is set: an unknown clip is reported first
			ReversedRange,  // Clip is set
		};

		Result Status = Result::Empty;
		std::string Clip;
		std::string Time;       // the time token as written, for warnings
		std::string Name;
		std::string Payload;    // AnimationEventPayload's text
		float Begin = 0.0f;
		float End = 0.0f;       // Begin for an instant
		bool Range = false;
	};

	// `<clip> <seconds> <event> [payload...]` or `<clip> <a>..<b> <event> [payload...]`, `#` starting
	// a comment; a double-quoted token (or part of one: key="a b") may hold spaces.
	EventsLine ParseEventsLine(std::string_view line);

}
