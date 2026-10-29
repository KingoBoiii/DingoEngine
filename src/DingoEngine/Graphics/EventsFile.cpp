#include "depch.h"
#include "DingoEngine/Graphics/EventsFile.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <span>
#include <vector>

namespace Dingo
{

	bool Internal::TokenizeSidecarLine(std::string_view line, std::vector<std::string>& tokens)
	{
		tokens.clear();
		size_t i = 0;
		while (i < line.size())
		{
			if (std::isspace(static_cast<unsigned char>(line[i])))
			{
				i++;
				continue;
			}
			if (line[i] == '#')
				break;

			if (line[i] == '"')
			{
				const size_t close = line.find('"', i + 1);
				if (close == std::string_view::npos)
					return false;
				tokens.emplace_back(line.substr(i + 1, close - i - 1));
				i = close + 1;
				continue;
			}

			// A quote inside a token quotes that part of it: a payload's key="value with spaces".
			std::string& token = tokens.emplace_back();
			while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i])) && line[i] != '#')
			{
				if (line[i] != '"')
				{
					token += line[i++];
					continue;
				}

				const size_t close = line.find('"', i + 1);
				if (close == std::string_view::npos)
					return false;
				token += line.substr(i + 1, close - i - 1);
				i = close + 1;
			}
		}
		return true;
	}

	namespace
	{

		// Payload tokens back into AnimationEventPayload's text, a value with a space quoted again.
		std::string JoinPayload(std::span<const std::string> tokens)
		{
			std::string payload;
			for (const std::string& token : tokens)
			{
				if (!payload.empty())
					payload += ' ';

				const size_t equals = token.find('=');
				const bool spaced = std::any_of(token.begin(), token.end(), [](char c) { return std::isspace(static_cast<unsigned char>(c)); });
				if (!spaced)
				{
					payload += token;
					continue;
				}
				if (equals == std::string::npos)
				{
					payload += '"';
					payload += token;
					payload += '"';
					continue;
				}
				payload += token.substr(0, equals + 1);
				payload += '"';
				payload += token.substr(equals + 1);
				payload += '"';
			}
			return payload;
		}

		bool ParseSeconds(std::string_view text, float& value)
		{
			const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
			return error == std::errc() && end == text.data() + text.size() && std::isfinite(value);
		}

	}

	Internal::EventsLine Internal::ParseEventsLine(std::string_view line)
	{
		EventsLine result;
		std::vector<std::string> tokens;
		if (!TokenizeSidecarLine(line, tokens))
		{
			result.Status = EventsLine::Result::UnclosedQuote;
			return result;
		}
		if (tokens.empty())
			return result;
		if (tokens.size() < 3)
		{
			result.Status = EventsLine::Result::TooFewTokens;
			return result;
		}

		result.Clip = std::move(tokens[0]);
		result.Time = tokens[1];

		const std::string_view time = tokens[1];
		const size_t dots = time.find("..");
		result.Range = dots != std::string_view::npos;
		const bool parsed = result.Range
			? ParseSeconds(time.substr(0, dots), result.Begin) && ParseSeconds(time.substr(dots + 2), result.End)
			: ParseSeconds(time, result.Begin);
		if (!parsed)
		{
			result.Status = EventsLine::Result::BadTime;
			return result;
		}
		if (result.Range && result.End < result.Begin)
		{
			result.Status = EventsLine::Result::ReversedRange;
			return result;
		}
		if (!result.Range)
			result.End = result.Begin;

		result.Status = EventsLine::Result::Event;
		result.Name = std::move(tokens[2]);
		result.Payload = JoinPayload(std::span<const std::string>(tokens).subspan(3));
		return result;
	}

}
