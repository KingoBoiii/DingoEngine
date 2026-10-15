#pragma once

#include <string_view>

namespace Dingo
{

	// The test app's --crash=<kind>: segv, thread (a segfault on another thread), abort,
	// terminate (an exception out of a noexcept function), verify or fatal. Unknown kinds warn.
	// engine-thread, engine-throw and engine-stack run on a thread started through
	// CrashHandler::RunThread, as the render and asset threads are; prepared-throw and
	// prepared-stack on one that called CrashHandler::PrepareThread, as Jolt's workers do.
	void TriggerCrash(std::string_view kind);

}
