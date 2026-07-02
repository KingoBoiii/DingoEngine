#include "depch.h"
#include "DingoEngine/Core/IO.h"

namespace Dingo::IO
{

	bool WriteFileAtomic(const std::filesystem::path& path, std::string_view contents)
	{
		DE_CORE_ASSERT(!path.empty(), "Path cannot be empty.");

		std::filesystem::path directory = path.parent_path();
		if (!directory.empty() && !std::filesystem::exists(directory))
		{
			std::error_code errorCode;
			std::filesystem::create_directories(directory, errorCode);
			if (errorCode)
			{
				DE_CORE_ERROR("Failed to create parent directory '{}': {}", directory.string(), errorCode.message());
				return false;
			}
		}

		// Temp file lives next to the destination so the final rename stays on the same
		// filesystem/volume, which is required for it to be atomic.
		std::filesystem::path tempPath = path;
		tempPath += ".tmp";

		{
			std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
			if (!file.is_open())
			{
				DE_CORE_ERROR("Failed to open temp file for atomic write: {}", tempPath.string());
				return false;
			}

			file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
			file.flush();

			if (!file.good())
			{
				DE_CORE_ERROR("Failed to write contents to temp file: {}", tempPath.string());
				file.close();
				std::error_code removeError;
				std::filesystem::remove(tempPath, removeError);
				return false;
			}
			// File closes here (end of scope) before the rename below.
		}

		std::error_code errorCode;
		std::filesystem::rename(tempPath, path, errorCode);
		if (errorCode)
		{
			DE_CORE_ERROR("Failed to atomically rename '{}' to '{}': {}", tempPath.string(), path.string(), errorCode.message());
			std::error_code removeError;
			std::filesystem::remove(tempPath, removeError);
			return false;
		}

		return true;
	}

} // namespace Dingo::IO
