#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

/// Operating-system queries: the per-user data folder and where the executable lives.
namespace Dingo::Platform
{

	/// Returns the per-user writable data directory for `appName`, creating it if missing.
	/// appName must not be empty and is read as UTF-8. When the OS variables below are unset it
	/// logs an error and uses `./<appName>` instead.
	/// Windows:	`%LOCALAPPDATA%\<appName>`
	/// POSIX:	`$XDG_DATA_HOME/<appName>`, else `$HOME/.local/share/<appName>`, else `./<appName>`
	std::filesystem::path GetUserDataDir(const std::string& appName);

	/// Absolute path of the running executable; the current directory (logged) if the OS can't say.
	std::filesystem::path GetExecutablePath();
	/// The folder holding the running executable.
	std::filesystem::path GetExecutableDirectory();

	/// Searches `start` and up to `maxLevels` of its parents for a directory named `name`.
	/// Returns the nearest match as an absolute path, or nullopt when none is found.
	/// Intended use:
	/// params.Assets.SetRootDirectory(Platform::FindDirectoryUpward("assets").value_or("assets")),
	/// which resolves both a packaged build (assets beside the exe) and a dev build (exe under
	/// `build/bin/<Config>/`).
	std::optional<std::filesystem::path> FindDirectoryUpward(const std::filesystem::path& name, const std::filesystem::path& start = GetExecutableDirectory(), uint32_t maxLevels = 8);

}
