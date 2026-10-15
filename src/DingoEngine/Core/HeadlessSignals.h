#pragma once

// Engine-internal: how a headless Application hears Ctrl+C, SIGTERM and a console close. The guard
// installs handlers for SIGINT and SIGTERM (Linux) or SetConsoleCtrlHandler (Windows) while it
// lives and puts the earlier ones back. A handler only sets an atomic; the run loop polls
// StopRequest. A second signal while the first is still being handled falls back to the default
// action and ends the process at once. A signal that was ignored when the guard started stays
// ignored, as for a job started in the background of a non-interactive shell.

namespace Dingo::Internal
{

	class HeadlessSignalGuard
	{
	public:
		HeadlessSignalGuard();
		~HeadlessSignalGuard();
		HeadlessSignalGuard(const HeadlessSignalGuard&) = delete;
		HeadlessSignalGuard& operator=(const HeadlessSignalGuard&) = delete;

		// What was caught ("SIGINT", "Ctrl+C", ...), nullptr while nothing has been.
		const char* StopRequest() const;
	};

}
