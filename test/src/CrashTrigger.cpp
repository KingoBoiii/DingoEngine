#include "CrashTrigger.h"

#include <DingoEngine/Assertion.h>
#include <DingoEngine/Core/FatalError.h>

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <thread>

namespace Dingo
{

	namespace
	{
		void WriteThroughNull()
		{
			volatile int* volatile pointer = nullptr;
			*pointer = 1;
		}

		// Bounded so no compiler sees it as infinite, but far deeper than any stack.
		uint64_t Recurse(uint64_t depth)
		{
			volatile char frame[512];
			frame[0] = static_cast<char>(depth);
			const uint64_t below = depth < (uint64_t(1) << 40) ? Recurse(depth + 1) : depth;
			return below + static_cast<uint64_t>(frame[0]);
		}

		[[noreturn]] void TerminateWithException()
		{
			try
			{
				throw std::runtime_error("the test app's --crash=terminate");
			}
			catch (...)
			{
				std::terminate();
			}
		}
	}

	void TriggerCrash(std::string_view kind)
	{
		DE_WARN("[Crash] --crash={}: crashing on purpose", kind);
		if (kind == "segv")
			WriteThroughNull();
		else if (kind == "thread")
			std::thread(WriteThroughNull).join();
		else if (kind == "abort")
			std::abort();
		else if (kind == "terminate")
			TerminateWithException();
		else if (kind == "throw")
			throw std::runtime_error("the test app's --crash=throw");
		else if (kind == "stack")
			DE_WARN("[Crash] {}", Recurse(0));
		else if (kind == "verify")
		{
			DE_VERIFY(false, "the test app's --crash=verify");
		}
		else if (kind == "fatal")
			FatalError("The test app's --crash=fatal.");
		else
			DE_WARN("[Crash] unknown --crash={}; expected segv, thread, abort, terminate, throw, stack, verify or fatal", kind);
	}

}
