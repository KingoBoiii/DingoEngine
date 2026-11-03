#pragma once

// Engine-internal: the text side of crash reporting, kept free of sentry and spdlog so the unit
// tests cover it in a build without --crash-reporting.

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace Dingo::Internal::CrashReportText
{

	enum class ReportLevel : uint8_t { Debug, Info, Warning, Error, Fatal };

	// From spdlog::level::level_enum's values (trace 0, debug 1, info 2, warn 3, err 4, critical 5);
	// CrashReporter.cpp static_asserts they still hold.
	constexpr ReportLevel LevelFromSpdlog(int level)
	{
		switch (level)
		{
			case 0:
			case 1: return ReportLevel::Debug;
			case 2: return ReportLevel::Info;
			case 3: return ReportLevel::Warning;
			case 4: return ReportLevel::Error;
			default: return ReportLevel::Fatal;
		}
	}

	// Error and Fatal lines become events; the rest are breadcrumbs on the next event.
	constexpr bool IsEvent(ReportLevel level)
	{
		return level == ReportLevel::Error || level == ReportLevel::Fatal;
	}

	constexpr const char* LevelName(ReportLevel level)
	{
		switch (level)
		{
			case ReportLevel::Debug: return "debug";
			case ReportLevel::Info: return "info";
			case ReportLevel::Warning: return "warning";
			case ReportLevel::Error: return "error";
			case ReportLevel::Fatal: return "fatal";
		}
		return "info";
	}

	// `<name>@<major.minor.patch>[+build]`, with the characters a Sentry release can't hold made '-'.
	inline std::string MakeRelease(std::string_view name, uint32_t major, uint32_t minor, uint32_t patch, uint32_t build)
	{
		std::string release(name.empty() ? std::string_view("Dingo") : name);
		for (char& c : release)
		{
			if (c == '/' || c == '\\' || c == '\n' || c == '\r' || c == '\t')
				c = '-';
		}
		release += "@" + std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
		if (build != 0)
			release += "+" + std::to_string(build);
		return release;
	}

	// Replaces the player's home folder with "~" in text, and masks its last part (the user name)
	// with '_' in binary data such as a minidump, where the length must not change.
	class HomeScrubber
	{
	public:
		HomeScrubber() = default;
		HomeScrubber(std::string_view homeUtf8, std::u16string_view homeUtf16, bool ignoreCase)
			: m_IgnoreCase(ignoreCase)
		{
			while (!homeUtf8.empty() && (homeUtf8.back() == '/' || homeUtf8.back() == '\\'))
				homeUtf8.remove_suffix(1);
			while (!homeUtf16.empty() && (homeUtf16.back() == u'/' || homeUtf16.back() == u'\\'))
				homeUtf16.remove_suffix(1);
			// "/" or "C:" alone would scrub every path.
			if (homeUtf8.size() < 4)
				return;

			m_Home.emplace_back(homeUtf8);
			std::string swapped(homeUtf8);
			for (char& c : swapped)
				c = c == '\\' ? '/' : c == '/' ? '\\' : c;
			if (swapped != m_Home.front())
				m_Home.push_back(std::move(swapped));
			m_HomeUtf16 = homeUtf16;
		}

		bool IsActive() const { return !m_Home.empty(); }

		std::string Apply(std::string_view text) const
		{
			std::string result(text);
			for (const std::string& home : m_Home)
			{
				size_t at = 0;
				while ((at = Find(result, home, at)) != std::string::npos)
				{
					const size_t end = at + home.size();
					if (end < result.size() && result[end] != '/' && result[end] != '\\' && IsNameChar(result[end]))
					{
						at += 1;
						continue;
					}
					result.replace(at, home.size(), "~");
					at += 1;
				}
			}
			return result;
		}

		void MaskBinary(std::vector<char>& bytes) const
		{
			for (const std::string& home : m_Home)
				MaskUnits<char>(bytes, home);
			if (m_HomeUtf16.size() >= 4)
				MaskUnits<char16_t>(bytes, m_HomeUtf16);
		}

	private:
		static char Lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

		// "C:\Users\Anna" must not match the start of "C:\Users\Annabel".
		static bool IsNameChar(char c)
		{
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'
				|| static_cast<unsigned char>(c) >= 0x80;
		}

		bool Same(char a, char b) const { return m_IgnoreCase ? Lower(a) == Lower(b) : a == b; }

		size_t Find(std::string_view haystack, std::string_view needle, size_t from) const
		{
			if (needle.empty() || haystack.size() < needle.size())
				return std::string::npos;
			for (size_t i = from; i + needle.size() <= haystack.size(); ++i)
			{
				size_t j = 0;
				while (j < needle.size() && Same(haystack[i + j], needle[j]))
					++j;
				if (j == needle.size())
					return i;
			}
			return std::string::npos;
		}

		template<typename Unit>
		void MaskUnits(std::vector<char>& bytes, std::basic_string_view<Unit> home) const
		{
			size_t nameStart = home.size();
			while (nameStart > 0 && home[nameStart - 1] != Unit('/') && home[nameStart - 1] != Unit('\\'))
				--nameStart;
			const size_t unitSize = sizeof(Unit);
			const size_t needle = home.size() * unitSize;
			if (nameStart == home.size() || bytes.size() < needle)
				return;

			for (size_t i = 0; i + needle <= bytes.size(); ++i)
			{
				bool match = true;
				for (size_t u = 0; u < home.size() && match; ++u)
				{
					uint32_t unit = 0;
					for (size_t b = 0; b < unitSize; ++b)
						unit |= static_cast<uint32_t>(static_cast<uint8_t>(bytes[i + u * unitSize + b])) << (8 * b);
					const uint32_t expected = static_cast<uint32_t>(home[u]);
					const bool separator = (expected == '/' || expected == '\\') && (unit == '/' || unit == '\\');
					match = separator || unit == expected
						|| (m_IgnoreCase && expected < 128 && unit < 128 && Lower(static_cast<char>(unit)) == Lower(static_cast<char>(expected)));
				}
				if (!match)
					continue;
				for (size_t u = nameStart; u < home.size(); ++u)
				{
					bytes[i + u * unitSize] = '_';
					for (size_t b = 1; b < unitSize; ++b)
						bytes[i + u * unitSize + b] = 0;
				}
				i += needle - 1;
			}
		}

		std::vector<std::string> m_Home;
		std::u16string m_HomeUtf16;
		bool m_IgnoreCase = false;
	};

	// What the engine's crash handler left in a log: its "*** <app> crashed: ..." report, the lines
	// before it and, on Windows, the minidump it names.
	struct CrashLog
	{
		std::string Header;
		std::string Report;
		std::string Tail;
		std::string MinidumpPath;
		bool Complete = false;		// the handler's last line is there: no process still writes it
		bool ReportingOn = false;	// the crashed run had reports on, so its player opted in
	};

	// Cuts at most maxBytes without splitting a UTF-8 sequence, which would make the event invalid JSON.
	inline std::string_view TruncateUtf8(std::string_view text, size_t maxBytes)
	{
		if (text.size() <= maxBytes)
			return text;
		size_t end = maxBytes;
		while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80)
			--end;
		return text.substr(0, end);
	}

	// FNV-1a 64 of the report and the lines before it, as hex: stable across builds and platforms,
	// and unlike a size or write time it doesn't change if the crashed process writes on.
	inline std::string CrashKey(const CrashLog& crash)
	{
		uint64_t hash = 14695981039346656037ull;
		for (const std::string* part : { &crash.Header, &crash.Report, &crash.Tail })
		{
			for (const char c : *part)
			{
				hash ^= static_cast<uint8_t>(c);
				hash *= 1099511628211ull;
			}
			hash ^= 0xFF;
			hash *= 1099511628211ull;
		}
		static constexpr char k_Hex[] = "0123456789abcdef";
		std::string key(16, '0');
		for (size_t i = 0; i < 16; ++i)
			key[15 - i] = k_Hex[(hash >> (4 * i)) & 0xF];
		return key;
	}

	inline constexpr size_t k_MaxReportBytes = 16 * 1024;
	inline constexpr size_t k_MaxTailBytes = 24 * 1024;

	inline std::optional<CrashLog> FindCrash(std::string_view log, size_t tailLines = 100)
	{
		std::vector<std::string_view> lines;
		size_t start = 0;
		while (start < log.size())
		{
			size_t end = log.find('\n', start);
			if (end == std::string_view::npos)
				end = log.size();
			std::string_view line = log.substr(start, end - start);
			if (!line.empty() && line.back() == '\r')
				line.remove_suffix(1);
			lines.push_back(line);
			start = end + 1;
		}

		constexpr std::string_view k_Marker = "*** ";
		constexpr std::string_view k_Crashed = " crashed: ";
		constexpr std::string_view k_Minidump = "*** Minidump: ";
		constexpr std::string_view k_ReportingOn = "*** Crash reporting: on";
		constexpr std::string_view k_Footers[] = { k_Minidump, "*** No minidump", "*** End of backtrace" };
		size_t header = lines.size();
		while (header > 0)
		{
			--header;
			if (lines[header].starts_with(k_Marker) && lines[header].find(k_Crashed) != std::string_view::npos)
				break;
			if (header == 0)
				return std::nullopt;
		}
		if (header == lines.size())
			return std::nullopt;

		CrashLog crash;
		crash.Header = std::string(lines[header].substr(k_Marker.size()));
		for (size_t i = header; i < lines.size(); ++i)
		{
			if (lines[i].starts_with(k_Minidump) && crash.MinidumpPath.empty())
				crash.MinidumpPath = std::string(lines[i].substr(k_Minidump.size()));
			if (lines[i] == k_ReportingOn)
				crash.ReportingOn = true;
			if (crash.Report.size() + lines[i].size() + 1 <= k_MaxReportBytes)
				crash.Report.append(lines[i]).push_back('\n');
			// The handler's last line: anything after it doesn't belong to the report or its key.
			for (const std::string_view footer : k_Footers)
				crash.Complete = crash.Complete || lines[i].starts_with(footer);
			if (crash.Complete)
				break;
		}

		size_t first = header > tailLines ? header - tailLines : 0;
		while (first < header && lines[first].empty())
			++first;
		size_t bytes = 0;
		for (size_t i = header; i > first; --i)
		{
			bytes += lines[i - 1].size() + 1;
			if (bytes > k_MaxTailBytes)
			{
				first = i;
				break;
			}
		}
		for (size_t i = first; i < header; ++i)
			crash.Tail.append(lines[i]).push_back('\n');
		while (!crash.Tail.empty() && (crash.Tail.back() == '\n'))
			crash.Tail.pop_back();
		return crash;
	}

	// Each distinct message is sent once per run, and no more than k_MaxEvents in all, so an error
	// logged every frame costs one event.
	class EventLimiter
	{
	public:
		static constexpr uint32_t k_MaxEvents = 25;

		bool Allow(std::string_view message)
		{
			if (m_Sent >= k_MaxEvents)
				return false;
			if (!m_Seen.insert(std::hash<std::string_view>{}(message)).second)
				return false;
			++m_Sent;
			return true;
		}

	private:
		std::unordered_set<size_t> m_Seen;
		uint32_t m_Sent = 0;
	};

}
