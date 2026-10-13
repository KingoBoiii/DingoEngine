#pragma once

#include <source_location>
#include <string_view>

namespace Dingo
{

	/// Ends the program over an error it can't go on from: logs the message with where it was
	/// raised, flushes the log, shows it in a message box (Windows Distribution builds) and exits
	/// with code 1, without running static destructors. DE_VERIFY ends here in Distribution builds.
	[[noreturn]] void FatalError(std::string_view message, const std::source_location& location = std::source_location::current());

}
