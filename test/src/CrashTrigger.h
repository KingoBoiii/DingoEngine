#pragma once

#include <string_view>

namespace Dingo
{

	// The test app's --crash=<kind>: segv, thread (a segfault on another thread), abort,
	// terminate (an exception out of a noexcept function), verify or fatal. Unknown kinds warn.
	void TriggerCrash(std::string_view kind);

}
