#pragma once

// Engine-internal: what a crash leaves behind. Linux: SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT and
// SIGTRAP write a backtrace into the log and stderr. Windows: an unhandled exception, abort(), a
// pure virtual call or an invalid CRT parameter writes a minidump beside the log and a line into
// it, and shows a message box in Distribution builds. Both: std::terminate logs the exception it
// ended on first. One thread reports; any other that crashes meanwhile waits for the exit. The
// handlers write with write(2)/WriteFile from buffers prepared at Install, and flush spdlog first
// only when the crashing thread isn't inside it (its mutex would deadlock).

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace Dingo::Internal
{

	// True while this thread is inside a Log call (Log.cpp keeps the count).
	bool IsLoggingOnThisThread();

	namespace CrashHandler
	{

		// Installs the handlers, writing into `logFile` (may be empty: stderr only) and, on Windows,
		// dumps into `dumpDirectory`, keeping the newest `keepDumps`. Again later only retargets them.
		void Install(const std::filesystem::path& logFile, const std::filesystem::path& dumpDirectory, std::string_view appName, uint32_t keepDumps = 5);

		// Every thread the engine starts calls this first: Windows keeps the terminate handler per
		// thread and needs a stack guarantee to report an overflow; Linux gives the thread its signal
		// stack. Threads a library starts for the engine (Jolt's workers) too.
		void PrepareThread();

		// A std::thread body for the engine's own threads: PrepareThread, then `body(context)` under
		// an SEH filter on Windows, where std::thread's noexcept entry would otherwise turn an escaping
		// exception into a terminate with no exception to log.
		void RunThread(void (*body)(void*), void* context);

		// The message box FatalError and a Windows crash show: Windows Distribution builds only, so
		// a CI run or a developer's console never blocks on one.
		void ShowMessage(std::string_view title, std::string_view message);

	}

}
