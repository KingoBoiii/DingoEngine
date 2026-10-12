#include "CrashTrigger.h"

#include <DingoEngine/Assertion.h>
#include <DingoEngine/Core/FatalError.h>

#include <cstdlib>
#include <thread>
#include <vector>

namespace Dingo
{

	namespace
	{
		void WriteThroughNull()
		{
			volatile int* volatile pointer = nullptr;
			*pointer = 1;
		}

		void ThrowOutOfNoexcept() noexcept
		{
			const std::vector<int> empty;
			(void)empty.at(0);
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
			ThrowOutOfNoexcept();
		else if (kind == "verify")
		{
			DE_VERIFY(false, "the test app's --crash=verify");
		}
		else if (kind == "fatal")
			FatalError("The test app's --crash=fatal.");
		else
			DE_WARN("[Crash] unknown --crash={}; expected segv, thread, abort, terminate, verify or fatal", kind);
	}

}
