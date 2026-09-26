#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace Dingo::Platform
{

	// Returns the per-user writable data directory for `appName`, creating it if missing.
	// Windows:	%LOCALAPPDATA%\<appName>
	// POSIX:	$XDG_DATA_HOME/<appName>, else $HOME/.local/share/<appName>, else ./<appName>
	std::filesystem::path GetUserDataDir(const std::string& appName);

	std::filesystem::path GetExecutablePath();
	std::filesystem::path GetExecutableDirectory();

	// Searches `start` and up to `maxLevels` of its parents for a directory named `name`.
	// Intended use: params.Assets.SetRootDirectory(Platform::FindDirectoryUpward("assets").value_or("assets")),
	// which resolves both a packaged build (assets beside the exe) and a dev build (exe under build/bin/<Config>/).
	std::optional<std::filesystem::path> FindDirectoryUpward(const std::filesystem::path& name, const std::filesystem::path& start = GetExecutableDirectory(), uint32_t maxLevels = 8);

}
