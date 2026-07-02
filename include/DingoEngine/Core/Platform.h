#pragma once
#include <filesystem>
#include <string>

namespace Dingo::Platform
{

	// Returns the per-user writable data directory for `appName`, creating it if missing.
	// Windows:	%LOCALAPPDATA%\<appName>
	// POSIX:	$XDG_DATA_HOME/<appName>, else $HOME/.local/share/<appName>, else ./<appName>
	std::filesystem::path GetUserDataDir(const std::string& appName);

}
