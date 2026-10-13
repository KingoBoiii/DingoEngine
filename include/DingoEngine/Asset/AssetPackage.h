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

	/// A mounted .dpak, from AssetManager::Unpackage. k_InvalidPackage when the package didn't
	/// open.
	using PackageHandle = UUID;
	/// The null package handle: Unpackage's result when the package didn't open.
	inline const PackageHandle k_InvalidPackage = PackageHandle(0);

	/// The 256-bit key a package is encrypted with. All zero means not encrypted.
	struct PackageKey
	{
		std::array<uint8_t, 32> Bytes{};	///< The raw key bytes.

		/// True when every byte is zero, which means no encryption.
		bool IsNull() const;
		/// The key as 64 lowercase hex digits.
		std::string ToHex() const;
		/// 64 hex digits; whitespace is skipped. nullopt for anything else.
		static std::optional<PackageKey> FromHex(std::string_view hex);
		/// A key file holds the 64 hex digits (lines starting with # are comments).
		/// nullopt when the file can't be read or doesn't hold a valid key.
		static std::optional<PackageKey> FromFile(const std::filesystem::path& path);
		/// A new random key. Keep it secret: it is what protects the package.
		static PackageKey Generate();

		/// Compares the key bytes.
		bool operator==(const PackageKey&) const = default;
	};

	/// What AssetManager::Package packs, and how.
	struct PackageParams
	{
		/// Empty means the running AssetManager's root.
		std::filesystem::path SourceDirectory;
		std::filesystem::path OutputPath;	///< The .dpak file to write; required.
		PackageKey Key;	///< Encrypts the package; a null key writes it unencrypted.
		/// Glob patterns, matched as the source root's .dpakignore lines are: "*" stays within one
		/// path segment, "**" crosses them, "?" is one character; a pattern without a '/' matches a
		/// file name in any directory, and a trailing '/' matches a directory.
		std::vector<std::string> Exclude;

		/// Sets SourceDirectory and returns *this, for chaining.
		PackageParams& SetSourceDirectory(const std::filesystem::path& directory)
		{
			SourceDirectory = directory;
			return *this;
		}

		/// Sets OutputPath and returns *this, for chaining.
		PackageParams& SetOutputPath(const std::filesystem::path& path)
		{
			OutputPath = path;
			return *this;
		}

		/// Sets Key and returns *this, for chaining.
		PackageParams& SetKey(const PackageKey& key)
		{
			Key = key;
			return *this;
		}

		/// Appends a glob pattern to Exclude and returns *this, for chaining.
		PackageParams& AddExclude(const std::string& pattern)
		{
			Exclude.push_back(pattern);
			return *this;
		}
	};

	/// The outcome of AssetManager::Package.
	struct PackageResult
	{
		bool Success = false;	///< True when the package was written.
		std::string Error;	///< Why it failed; empty on success.
		uint32_t FileCount = 0;	///< Files packed.
		uint64_t RawBytes = 0;	///< Total size of the packed files before compression, in bytes.
		uint64_t PackedBytes = 0;	///< Size of the written .dpak file, in bytes.
		std::vector<std::string> Warnings;	///< Notes that didn't stop packing, e.g. a skipped output file.
	};

}
