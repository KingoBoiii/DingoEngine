#pragma once
#include "DingoEngine/Core/UUID.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Dingo
{

	// A mounted .dpak, from AssetManager::Unpackage. k_InvalidPackage when the package didn't open.
	using PackageHandle = UUID;
	inline const PackageHandle k_InvalidPackage = PackageHandle(0);

	// The 256-bit key a package is encrypted with. All zero means not encrypted.
	struct PackageKey
	{
		std::array<uint8_t, 32> Bytes{};

		bool IsNull() const;
		std::string ToHex() const;
		// 64 hex digits; whitespace is skipped. nullopt for anything else.
		static std::optional<PackageKey> FromHex(std::string_view hex);
		// A key file holds the 64 hex digits (lines starting with # are comments).
		static std::optional<PackageKey> FromFile(const std::filesystem::path& path);
		static PackageKey Generate();

		bool operator==(const PackageKey&) const = default;
	};

	struct PackageParams
	{
		// Empty means the running AssetManager's root.
		std::filesystem::path SourceDirectory;
		std::filesystem::path OutputPath;
		PackageKey Key;
		// Glob patterns, matched as the source root's .dpakignore lines are: "*" stays within one
		// path segment, "**" crosses them, "?" is one character; a pattern without a '/' matches a
		// file name in any directory, and a trailing '/' matches a directory.
		std::vector<std::string> Exclude;

		PackageParams& SetSourceDirectory(const std::filesystem::path& directory)
		{
			SourceDirectory = directory;
			return *this;
		}

		PackageParams& SetOutputPath(const std::filesystem::path& path)
		{
			OutputPath = path;
			return *this;
		}

		PackageParams& SetKey(const PackageKey& key)
		{
			Key = key;
			return *this;
		}

		PackageParams& AddExclude(const std::string& pattern)
		{
			Exclude.push_back(pattern);
			return *this;
		}
	};

	struct PackageResult
	{
		bool Success = false;
		std::string Error;
		uint32_t FileCount = 0;
		uint64_t RawBytes = 0;
		uint64_t PackedBytes = 0;
		std::vector<std::string> Warnings;
	};

}
