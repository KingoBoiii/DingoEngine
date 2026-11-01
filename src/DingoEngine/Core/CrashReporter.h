#pragma once

// Engine-internal: opt-in crash and error reports through sentry-native (premake --crash-reporting,
// DE_CRASH_REPORTING), with sentry's backend "none": the engine's CrashHandler stays the only crash
// handler, and a crash it logged is sent on the next launch. A sink on the loggers queues every
// line; one worker thread, the only code that calls sentry, turns Error/Fatal into events and the
// rest into breadcrumbs. The sink never blocks on sentry and never logs, so it is safe under the
// sink lock, in TryLogFatal and on a crashing thread. Without DE_CRASH_REPORTING every call is a
// no-op and nothing of sentry is compiled in.

#include <filesystem>
#include <string>

namespace Dingo::Internal::CrashReporter
{

	struct Config
	{
		std::string Dsn;
		std::string Release;
		std::string Environment;
		bool SendAttachments = false;
		std::filesystem::path LogDirectory;	// empty: reporting can't start (sentry's database lives here)
		std::filesystem::path LogFile;			// this run's log, never reported as a crashed run
	};

#ifdef DE_CRASH_REPORTING

	// The first call wins: a RequestRestart's new Application keeps the running configuration and
	// whatever SetEnabled chose.
	bool IsConfigured();
	void Configure(const Config& config);

	// Main thread. True when the requested state is in effect afterwards.
	bool SetEnabled(bool enabled);
	bool IsEnabled();

	// Crash handler, once its report starts (signal-safe): later lines aren't sent from the dying
	// process, since the next launch sends the report.
	void MuteForCrash();

	// Sends what is queued (up to about 2 s) and closes sentry; Internal::Main, before ShutdownLog.
	void Shutdown();

	// FatalError, after its line is logged: has the worker send the queue and wait for the upload,
	// for at most about 2 s. The calling thread never enters sentry or waits on a lock for long.
	void FlushBeforeFatal();

#else

	inline bool IsConfigured() { return true; }
	inline void Configure(const Config&) {}
	inline bool SetEnabled(bool enabled) { return !enabled; }
	inline bool IsEnabled() { return false; }
	inline void MuteForCrash() {}
	inline void Shutdown() {}
	inline void FlushBeforeFatal() {}

#endif

}
