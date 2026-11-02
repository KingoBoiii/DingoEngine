#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>

namespace spdlog::sinks { class sink; }

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

	// Adds a sink beside the console and the file, under the loggers' shared sink lock: it sees
	// every line logged from then on, TryLogFatal's included, and must never log itself.
	void AddLogSink(const std::shared_ptr<spdlog::sinks::sink>& sink);
	void RemoveLogSink(const std::shared_ptr<spdlog::sinks::sink>& sink);

}
