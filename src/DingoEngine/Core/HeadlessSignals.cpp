#include "depch.h"
#include "DingoEngine/Core/HeadlessSignals.h"

#include <atomic>

#ifndef DE_PLATFORM_WINDOWS
#include <signal.h>
#endif

namespace Dingo::Internal
{

	namespace
	{
		static_assert(std::atomic<int>::is_always_lock_free);

		// 0 until a handler ran, then the signal or control event it caught.
		std::atomic<int> s_Caught{ 0 };

#ifdef DE_PLATFORM_WINDOWS

		// The OS ends the process when a CTRL_CLOSE_EVENT handler returns, or after about five
		// seconds of it running. Teardown ends the process by returning from main first.
		constexpr DWORD k_CloseGraceMs = 4500;
		bool s_Installed = false;

		BOOL WINAPI OnConsoleControl(DWORD event)
		{
			switch (event)
			{
				case CTRL_C_EVENT:
				case CTRL_BREAK_EVENT:
					return s_Caught.exchange(static_cast<int>(event) + 1) == 0 ? TRUE : FALSE;
				case CTRL_CLOSE_EVENT:
				case CTRL_LOGOFF_EVENT:
				case CTRL_SHUTDOWN_EVENT:
					s_Caught.store(static_cast<int>(event) + 1);
					Sleep(k_CloseGraceMs);
					return TRUE;
				default:
					return FALSE;
			}
		}

		const char* Describe(int caught)
		{
			switch (caught - 1)
			{
				case CTRL_C_EVENT:		return "Ctrl+C";
				case CTRL_BREAK_EVENT:	return "Ctrl+Break";
				case CTRL_CLOSE_EVENT:	return "the console closing";
				case CTRL_LOGOFF_EVENT:	return "a logoff";
				case CTRL_SHUTDOWN_EVENT: return "a shutdown";
				default:				return "a console event";
			}
		}

#else

		constexpr int k_Signals[] = { SIGINT, SIGTERM };
		struct sigaction s_Previous[2];
		bool s_Installed[2] = {};

		void OnSignal(int number)
		{
			if (s_Caught.exchange(number) == 0)
				return;
			signal(number, SIG_DFL);
			raise(number);
		}

		const char* Describe(int caught)
		{
			return caught == SIGINT ? "SIGINT" : caught == SIGTERM ? "SIGTERM" : "a signal";
		}

#endif
	}

	HeadlessSignalGuard::HeadlessSignalGuard()
	{
		s_Caught.store(0);
#ifdef DE_PLATFORM_WINDOWS
		s_Installed = SetConsoleCtrlHandler(OnConsoleControl, TRUE) != 0;
#else
		for (size_t i = 0; i < 2; ++i)
		{
			struct sigaction action = {};
			action.sa_handler = OnSignal;
			sigemptyset(&action.sa_mask);
			sigaction(k_Signals[i], nullptr, &s_Previous[i]);
			if (s_Previous[i].sa_handler == SIG_IGN)
				continue;
			s_Installed[i] = sigaction(k_Signals[i], &action, nullptr) == 0;
		}
#endif
	}

	HeadlessSignalGuard::~HeadlessSignalGuard()
	{
#ifdef DE_PLATFORM_WINDOWS
		if (s_Installed)
			SetConsoleCtrlHandler(OnConsoleControl, FALSE);
		s_Installed = false;
#else
		for (size_t i = 0; i < 2; ++i)
		{
			if (s_Installed[i])
				sigaction(k_Signals[i], &s_Previous[i], nullptr);
			s_Installed[i] = false;
		}
#endif
	}

	const char* HeadlessSignalGuard::StopRequest() const
	{
		const int caught = s_Caught.load();
		return caught == 0 ? nullptr : Describe(caught);
	}

}
