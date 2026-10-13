#pragma once
#include <cstdint>
#include <filesystem>

namespace Dingo::Internal
{

	// Logs to the console, keeping what is logged until OpenLogFile in memory.
	void InitializeLog();
	void ShutdownLog();

	// Opens `directory`/Dingo.log for this run, first moving the earlier runs' logs up one name
	// (Dingo.log -> Dingo.1.log, ... up to Dingo.<keepRuns - 1>.log, the oldest dropped), and
	// writes everything logged since InitializeLog into it. Application's constructor calls it;
	// later calls keep the file already open. False when the directory can't be written: the
	// log then goes to the console only.
	bool OpenLogFile(const std::filesystem::path& directory, uint32_t keepRuns = 5);

}
