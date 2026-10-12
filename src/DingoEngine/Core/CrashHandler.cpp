#include "depch.h"
#include "DingoEngine/Core/CrashHandler.h"
#include "DingoEngine/Core/FatalError.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <exception>

#if defined(DE_PLATFORM_WINDOWS)
#include <DbgHelp.h>
#include <csignal>
#pragma comment(lib, "Dbghelp.lib")
#elif defined(DE_PLATFORM_LINUX)
#include <csignal>
#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#if defined(__SANITIZE_ADDRESS__)
#define DE_CRASH_HANDLER_SANITIZED 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define DE_CRASH_HANDLER_SANITIZED 1
#endif
#endif

namespace Dingo
{

	namespace
	{

		std::atomic<bool> s_Crashing = false;
		bool s_Installed = false;
		char s_AppName[128] = "The application";

		// Appends to a fixed buffer without allocating, for the handlers.
		struct LineBuilder
		{
			char Text[512];
			size_t Length = 0;

			LineBuilder& operator<<(const char* text)
			{
				while (*text && Length + 1 < sizeof(Text))
					Text[Length++] = *text++;
				Text[Length] = '\0';
				return *this;
			}

			LineBuilder& Hex(uintptr_t value)
			{
				char digits[2 + sizeof(uintptr_t) * 2 + 1] = "0x";
				for (size_t i = 0; i < sizeof(uintptr_t) * 2; ++i)
					digits[2 + i] = "0123456789abcdef"[(value >> ((sizeof(uintptr_t) * 2 - 1 - i) * 4)) & 0xF];
				digits[sizeof(digits) - 1] = '\0';
				return *this << digits;
			}
		};

		void CopyName(std::string_view name, char* out, size_t capacity)
		{
			const size_t length = (std::min)(name.size(), capacity - 1);
			std::memcpy(out, name.data(), length);
			out[length] = '\0';
		}

		std::string Timestamp()
		{
			const std::time_t now = std::time(nullptr);
			std::tm local{};
#ifdef DE_PLATFORM_WINDOWS
			localtime_s(&local, &now);
#else
			localtime_r(&now, &local);
#endif
			char text[32];
			std::strftime(text, sizeof(text), "%Y%m%d-%H%M%S", &local);
			return text;
		}

		void WriteStdErr(const char* text, size_t length);
		void WriteLog(const char* text, size_t length);

		void WriteBoth(const LineBuilder& line)
		{
			WriteStdErr(line.Text, line.Length);
			WriteLog(line.Text, line.Length);
		}

		void FlushLogIfSafe()
		{
			if (!Internal::IsLoggingOnThisThread())
				Log::Flush();
		}

#if defined(DE_PLATFORM_WINDOWS)

		HANDLE s_LogFile = INVALID_HANDLE_VALUE;
		wchar_t s_DumpPath[1024] = L"";
		char s_DumpPathUtf8[1024] = "";
		wchar_t s_CrashMessage[2048] = L"";

		std::wstring Widen(std::string_view text)
		{
			if (text.empty())
				return {};
			const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
			std::wstring wide(static_cast<size_t>(length), L'\0');
			MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
			return wide;
		}

		void CopyWide(const std::wstring& text, wchar_t* out, size_t capacity)
		{
			const size_t length = (std::min)(text.size(), capacity - 1);
			std::wmemcpy(out, text.data(), length);
			out[length] = L'\0';
		}

		void WriteHandle(HANDLE handle, const char* text, size_t length)
		{
			if (handle == INVALID_HANDLE_VALUE || handle == nullptr)
				return;
			DWORD written = 0;
			WriteFile(handle, text, static_cast<DWORD>(length), &written, nullptr);
		}

		void WriteStdErr(const char* text, size_t length)
		{
			WriteHandle(GetStdHandle(STD_ERROR_HANDLE), text, length);
		}

		void WriteLog(const char* text, size_t length)
		{
			WriteHandle(s_LogFile, text, length);
		}

		const char* ExceptionName(DWORD code)
		{
			switch (code)
			{
				case EXCEPTION_ACCESS_VIOLATION:		return "access violation";
				case EXCEPTION_STACK_OVERFLOW:			return "stack overflow";
				case EXCEPTION_ILLEGAL_INSTRUCTION:		return "illegal instruction";
				case EXCEPTION_INT_DIVIDE_BY_ZERO:		return "integer division by zero";
				case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:	return "array bounds exceeded";
				case EXCEPTION_IN_PAGE_ERROR:			return "in-page error";
				case EXCEPTION_BREAKPOINT:				return "breakpoint (a failed assert or verify)";
				case EXCEPTION_PRIV_INSTRUCTION:		return "privileged instruction";
				case EXCEPTION_DATATYPE_MISALIGNMENT:	return "misaligned data";
				case 0xE06D7363:						return "C++ exception";
				default:								return "exception";
			}
		}

		void WriteCrashReport(EXCEPTION_POINTERS* exception, const LineBuilder& what)
		{
			if (s_Crashing.exchange(true))
				return;

			FlushLogIfSafe();

			LineBuilder header;
			header << "\n*** " << s_AppName << " crashed: " << what.Text << "\n";
			WriteBoth(header);

			bool written = false;
			if (s_DumpPath[0] != L'\0')
			{
				const HANDLE file = CreateFileW(s_DumpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
				if (file != INVALID_HANDLE_VALUE)
				{
					MINIDUMP_EXCEPTION_INFORMATION information{};
					information.ThreadId = GetCurrentThreadId();
					information.ExceptionPointers = exception;
					information.ClientPointers = FALSE;
					const auto type = static_cast<MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithDataSegs | MiniDumpWithThreadInfo);
					written = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type, exception ? &information : nullptr, nullptr, nullptr);
					CloseHandle(file);
				}
			}

			LineBuilder footer;
			if (written)
				footer << "*** Minidump: " << s_DumpPathUtf8 << "\n";
			else
				footer << "*** No minidump could be written\n";
			WriteBoth(footer);

#ifdef DE_DISTRIBUTION
			MessageBoxW(nullptr, s_CrashMessage, L"Crash", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
#endif
		}

		[[noreturn]] void ReportAndExit(const char* what)
		{
			LineBuilder line;
			line << what;
			WriteCrashReport(nullptr, line);
			TerminateProcess(GetCurrentProcess(), 3);
			std::_Exit(3);
		}

		LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* exception)
		{
			LineBuilder what;
			const EXCEPTION_RECORD* record = exception ? exception->ExceptionRecord : nullptr;
			if (record)
			{
				what << ExceptionName(record->ExceptionCode) << " (code ";
				what.Hex(record->ExceptionCode) << ") at ";
				what.Hex(reinterpret_cast<uintptr_t>(record->ExceptionAddress));
				if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2)
				{
					what << (record->ExceptionInformation[0] == 1 ? ", writing " : ", reading ");
					what.Hex(static_cast<uintptr_t>(record->ExceptionInformation[1]));
				}
			}
			else
			{
				what << "an unhandled exception";
			}
			WriteCrashReport(exception, what);
			return EXCEPTION_EXECUTE_HANDLER;
		}

		void OnAbortSignal(int)
		{
			ReportAndExit("abort()");
		}

		void OnPureCall()
		{
			ReportAndExit("a pure virtual function call");
		}

		void OnInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
		{
			ReportAndExit("an invalid parameter passed to a C runtime function");
		}

		void InstallPlatform(const std::filesystem::path& logFile, const std::filesystem::path& dumpDirectory, uint32_t keepDumps)
		{
			if (s_LogFile != INVALID_HANDLE_VALUE)
			{
				CloseHandle(s_LogFile);
				s_LogFile = INVALID_HANDLE_VALUE;
			}
			if (!logFile.empty())
				s_LogFile = CreateFileW(logFile.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

			s_DumpPath[0] = L'\0';
			s_DumpPathUtf8[0] = '\0';
			if (!dumpDirectory.empty())
			{
				std::error_code error;
				std::filesystem::create_directories(dumpDirectory, error);

				std::vector<std::filesystem::path> dumps;
				for (const auto& entry : std::filesystem::directory_iterator(dumpDirectory, error))
				{
					const std::string name = entry.path().filename().string();
					if (entry.is_regular_file(error) && name.starts_with("Dingo-") && entry.path().extension() == ".dmp")
						dumps.push_back(entry.path());
				}
				std::sort(dumps.begin(), dumps.end());
				const size_t keepOld = keepDumps > 0 ? keepDumps - 1 : 0;
				for (size_t i = 0; i + keepOld < dumps.size(); ++i)
					std::filesystem::remove(dumps[i], error);

				const std::filesystem::path dump = dumpDirectory / std::format("Dingo-{}-{}.dmp", Timestamp(), GetCurrentProcessId());
				CopyWide(dump.wstring(), s_DumpPath, std::size(s_DumpPath));
				CopyName(dump.string(), s_DumpPathUtf8, sizeof(s_DumpPathUtf8));
			}

			const std::string message = std::format("{} crashed.\n\nA crash report was written to:\n{}", s_AppName, dumpDirectory.string());
			CopyWide(Widen(message), s_CrashMessage, std::size(s_CrashMessage));

			if (s_Installed)
				return;

			// The filter needs stack left to run on after an overflow.
			ULONG guarantee = 64 * 1024;
			SetThreadStackGuarantee(&guarantee);

			SetUnhandledExceptionFilter(OnUnhandledException);
			std::signal(SIGABRT, OnAbortSignal);
			_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
			_set_purecall_handler(OnPureCall);
			_set_invalid_parameter_handler(OnInvalidParameter);
		}

		[[noreturn]] void CrashOnTerminate()
		{
			ReportAndExit("std::terminate");
		}

#elif defined(DE_PLATFORM_LINUX)

		int s_LogFd = -1;

		void WriteFd(int fd, const char* text, size_t length)
		{
			while (fd >= 0 && length > 0)
			{
				const ssize_t written = write(fd, text, length);
				if (written < 0 && errno == EINTR)
					continue;
				if (written <= 0)
					return;
				text += written;
				length -= static_cast<size_t>(written);
			}
		}

		void WriteStdErr(const char* text, size_t length)
		{
			WriteFd(STDERR_FILENO, text, length);
		}

		void WriteLog(const char* text, size_t length)
		{
			WriteFd(s_LogFd, text, length);
		}

#ifndef DE_CRASH_HANDLER_SANITIZED
		constexpr int k_Signals[] = { SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT, SIGTRAP };

		// For a stack overflow, which leaves no stack to run the handler on.
		alignas(16) char s_AlternateStack[64 * 1024];

		const char* SignalName(int signal)
		{
			switch (signal)
			{
				case SIGSEGV:	return "SIGSEGV (segmentation fault)";
				case SIGBUS:	return "SIGBUS (bus error)";
				case SIGFPE:	return "SIGFPE (arithmetic error)";
				case SIGILL:	return "SIGILL (illegal instruction)";
				case SIGABRT:	return "SIGABRT (abort)";
				case SIGTRAP:	return "SIGTRAP (a failed assert or verify)";
				default:		return "a fatal signal";
			}
		}

		void OnSignal(int signal, siginfo_t* info, void*)
		{
			if (!s_Crashing.exchange(true))
			{
				FlushLogIfSafe();

				LineBuilder header;
				header << "\n*** " << s_AppName << " crashed: " << SignalName(signal);
				if (info && signal != SIGABRT && signal != SIGTRAP)
				{
					header << " at ";
					header.Hex(reinterpret_cast<uintptr_t>(info->si_addr));
				}
				header << "\n*** Backtrace (c++filt demangles; addr2line -C -f -e <executable> <offset> names an offset):\n";
				WriteBoth(header);

				void* frames[64];
				const int count = backtrace(frames, 64);
				backtrace_symbols_fd(frames, count, STDERR_FILENO);
				if (s_LogFd >= 0)
					backtrace_symbols_fd(frames, count, s_LogFd);

				LineBuilder footer;
				footer << "*** End of backtrace\n";
				WriteBoth(footer);
			}

			// SA_RESETHAND restored the default action: the process ends as it would have, core dump
			// and exit status included.
			raise(signal);
		}
#endif

		void InstallPlatform(const std::filesystem::path& logFile, const std::filesystem::path&, uint32_t)
		{
			if (s_LogFd >= 0)
			{
				close(s_LogFd);
				s_LogFd = -1;
			}
			if (!logFile.empty())
				s_LogFd = open(logFile.c_str(), O_WRONLY | O_APPEND | O_CLOEXEC);

			if (s_Installed)
				return;

#ifndef DE_CRASH_HANDLER_SANITIZED
			// The first backtrace loads libgcc, which allocates: do it now rather than in a handler.
			void* frame = nullptr;
			backtrace(&frame, 1);

			stack_t stack{};
			stack.ss_sp = s_AlternateStack;
			stack.ss_size = sizeof(s_AlternateStack);
			sigaltstack(&stack, nullptr);

			struct sigaction action{};
			action.sa_sigaction = OnSignal;
			action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
			sigemptyset(&action.sa_mask);
			for (int signal : k_Signals)
				sigaction(signal, &action, nullptr);
#endif
		}

		[[noreturn]] void CrashOnTerminate()
		{
			std::abort();
		}

#else

		void WriteStdErr(const char* text, size_t length) { std::fwrite(text, 1, length, stderr); }
		void WriteLog(const char*, size_t) {}
		void InstallPlatform(const std::filesystem::path&, const std::filesystem::path&, uint32_t) {}
		[[noreturn]] void CrashOnTerminate() { std::abort(); }

#endif

		[[noreturn]] void OnTerminate()
		{
			std::string reason = "with no exception";
			if (const std::exception_ptr exception = std::current_exception())
			{
				try
				{
					std::rethrow_exception(exception);
				}
				catch (const std::exception& caught)
				{
					reason = std::format("on an uncaught exception: {}", caught.what());
				}
				catch (...)
				{
					reason = "on an uncaught exception of an unknown type";
				}
			}

			if (Internal::IsLoggingOnThisThread())
			{
				const std::string line = std::format("\n*** std::terminate called {}\n", reason);
				WriteStdErr(line.data(), line.size());
				WriteLog(line.data(), line.size());
			}
			else
			{
				DE_CORE_FATAL("std::terminate called {}", reason);
			}
			CrashOnTerminate();
		}

	}

	void Internal::CrashHandler::Install(const std::filesystem::path& logFile, const std::filesystem::path& dumpDirectory, std::string_view appName, uint32_t keepDumps)
	{
		CopyName(appName.empty() ? std::string_view("The application") : appName, s_AppName, sizeof(s_AppName));
		InstallPlatform(logFile, dumpDirectory, keepDumps);
		if (!s_Installed)
			std::set_terminate(OnTerminate);
		s_Installed = true;
	}

	void Internal::CrashHandler::ShowMessage(std::string_view title, std::string_view message)
	{
#if defined(DE_PLATFORM_WINDOWS) && defined(DE_DISTRIBUTION)
		MessageBoxW(nullptr, Widen(message).c_str(), Widen(title).c_str(), MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
#else
		(void)title;
		(void)message;
#endif
	}

	void FatalError(std::string_view message, const std::source_location& location)
	{
		const std::string where = std::format("{}:{}", std::filesystem::path(location.file_name()).filename().string(), location.line());

		if (Internal::IsLoggingOnThisThread())
		{
			const std::string line = std::format("\n*** Fatal error: {} ({})\n", message, where);
			WriteStdErr(line.data(), line.size());
			WriteLog(line.data(), line.size());
		}
		else
		{
			DE_CORE_FATAL("Fatal error: {} ({})", message, where);
			Log::Flush();
		}

		const std::filesystem::path& logFile = Log::GetFilePath();
		const std::string text = logFile.empty()
			? std::string(message)
			: std::format("{}\n\nThe log is in:\n{}", message, logFile.string());
		Internal::CrashHandler::ShowMessage(s_AppName, text);

		std::fflush(stdout);
		std::fflush(stderr);
		std::_Exit(1);
	}

}
