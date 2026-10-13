#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace Dingo::Internal
{

	// Paths become text as UTF-8 on every platform. On Windows path::string() and generic_string()
	// convert through the ANSI code page and throw on a character it can't map (Cyrillic under
	// code page 1252, say), and path(std::string) decodes through it too.

	inline std::string PathToUtf8(const std::filesystem::path& path)
	{
		const std::u8string text = path.u8string();
		return std::string(text.begin(), text.end());
	}

	inline std::string PathToGenericUtf8(const std::filesystem::path& path)
	{
		const std::u8string text = path.generic_u8string();
		return std::string(text.begin(), text.end());
	}

	inline std::filesystem::path PathFromUtf8(std::string_view text)
	{
		return std::filesystem::path(std::u8string(text.begin(), text.end()));
	}

}
