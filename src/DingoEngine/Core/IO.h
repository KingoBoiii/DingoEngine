#pragma once
#include <filesystem>
#include <string_view>

namespace Dingo::IO
{

	// Writes `contents` to `path` atomically: the data is written to a temporary file in the
	// same directory as `path`, flushed and closed, then renamed over `path`. A crash or power
	// loss mid-write leaves the original file untouched instead of truncated/corrupted.
	// Creates parent directories if needed. Returns true on success.
	bool WriteFileAtomic(const std::filesystem::path& path, std::string_view contents);

}
