#include "CrashTrigger.h"

#include <DingoEngine/Assertion.h>
#include <DingoEngine/Core/FatalError.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <stdexcept>
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

		// Bounded so no compiler sees it as infinite, but far deeper than any stack.
		uint64_t Recurse(uint64_t depth)
		{
			volatile char frame[512];
			frame[0] = static_cast<char>(depth);
			const uint64_t below = depth < (uint64_t(1) << 40) ? Recurse(depth + 1) : depth;
			return below + static_cast<uint64_t>(frame[0]);
		}

		void CrashThreadsTogether()
		{
			constexpr int k_Threads = 8;
			std::atomic<int> ready = 0;
			std::atomic<bool> go = false;
			std::vector<std::thread> threads;
			for (int i = 0; i < k_Threads; i++)
			{
				threads.emplace_back([&]
				{
					ready++;
					while (!go)
					{
					}
					WriteThroughNull();
				});
			}
			while (ready < k_Threads)
				std::this_thread::yield();
			go = true;
			for (std::thread& thread : threads)
				thread.join();
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
		else if (kind == "threads")
			CrashThreadsTogether();
		else if (kind == "segv-after-info")
		{
			DE_INFO("[Crash] last info line");
			WriteThroughNull();
		}
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
			DE_WARN("[Crash] unknown --crash={}; expected segv, thread, threads, segv-after-info, abort, terminate, throw, stack, verify or fatal", kind);
	}

}
