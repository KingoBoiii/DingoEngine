#include <doctest/doctest.h>

#include <DingoEngine/Core/FileSystem.h>
#include <DingoEngine/Core/Platform.h>
#include <DingoEngine/Log.h>
#include "DingoEngine/Core/LogLifecycle.h"

#include "LogWriter.h"

#include <charconv>
#include <chrono>
#include <cstdlib>
#include <format>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <thread>
#include <vector>

#if defined(DE_PLATFORM_WINDOWS)
#include <Windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace Dingo;

namespace
{
	uint64_t ProcessId()
	{
#if defined(DE_PLATFORM_WINDOWS)
		return GetCurrentProcessId();
#else
		return static_cast<uint64_t>(getpid());
#endif
	}

	std::string ReadText(const std::filesystem::path& path)
	{
		std::ifstream stream(path, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(stream), {});
	}

	struct TempDirectory
	{
		std::filesystem::path Path;

		TempDirectory()
		{
			std::random_device random;
			Path = std::filesystem::temp_directory_path() / ("DingoUnitTests-logs-" + std::to_string(random()));
			std::filesystem::create_directories(Path);
		}

		~TempDirectory()
		{
			std::error_code ignored;
			std::filesystem::remove_all(Path, ignored);
		}
	};

#if defined(DE_PLATFORM_WINDOWS)
	struct Child
	{
		PROCESS_INFORMATION Info = {};

		bool Start()
		{
			std::wstring commandLine = L"\"" + Platform::GetExecutablePath().wstring() + L"\" --log-writer";
			STARTUPINFOW startup = { sizeof(startup) };
			return CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &Info) != 0;
		}

		int Wait()
		{
			WaitForSingleObject(Info.hProcess, INFINITE);
			DWORD code = 0;
			GetExitCodeProcess(Info.hProcess, &code);
			CloseHandle(Info.hThread);
			CloseHandle(Info.hProcess);
			return static_cast<int>(code);
		}

		uint64_t Id() const { return Info.dwProcessId; }
	};

	void SetVariables(const std::filesystem::path& directory, int64_t startMs)
	{
		_wputenv_s(Test::k_LogWriterVariableW, directory.c_str());
		_wputenv_s(Test::k_LogWriterStartVariableW, std::to_wstring(startMs).c_str());
	}
#else
	struct Child
	{
		pid_t Pid = -1;

		bool Start()
		{
			const std::filesystem::path executable = Platform::GetExecutablePath();
			std::string argument = "--log-writer";
			char* argv[] = { const_cast<char*>(executable.c_str()), argument.data(), nullptr };
			return posix_spawn(&Pid, executable.c_str(), nullptr, nullptr, argv, environ) == 0;
		}

		int Wait()
		{
			int status = 0;
			waitpid(Pid, &status, 0);
			return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
		}

		uint64_t Id() const { return static_cast<uint64_t>(Pid); }
	};

	void SetVariables(const std::filesystem::path& directory, int64_t startMs)
	{
		setenv(Test::k_LogWriterVariable, directory.c_str(), 1);
		setenv(Test::k_LogWriterStartVariable, std::to_string(startMs).c_str(), 1);
	}
#endif

	struct Writers
	{
		std::set<uint64_t> Started;
		std::set<uint64_t> Ended;
	};

	std::map<std::filesystem::path, Writers> ReadWriters(const std::filesystem::path& directory)
	{
		std::map<std::filesystem::path, Writers> files;
		for (const auto& entry : std::filesystem::directory_iterator(directory))
		{
			if (entry.path().extension() != ".log")
				continue;
			Writers& writers = files[entry.path()];
			const std::string text = ReadText(entry.path());
			constexpr std::string_view k_Prefix = "writer ";
			for (size_t at = text.find(k_Prefix); at != std::string::npos; at = text.find(k_Prefix, at + 1))
			{
				uint64_t pid = 0;
				const char* end = text.data() + text.size();
				const auto [next, error] = std::from_chars(text.data() + at + k_Prefix.size(), end, pid);
				if (error != std::errc())
					continue;
				const std::string_view rest(next, static_cast<size_t>(end - next));
				if (rest.starts_with(" start"))
					writers.Started.insert(pid);
				else if (rest.starts_with(" end"))
					writers.Ended.insert(pid);
			}
		}
		return files;
	}
}

int Test::RunLogWriter()
{
#if defined(DE_PLATFORM_WINDOWS)
	const wchar_t* directory = _wgetenv(k_LogWriterVariableW);
#else
	const char* directory = std::getenv(k_LogWriterVariable);
#endif
	const char* start = std::getenv(k_LogWriterStartVariable);
	if (!directory || !start)
		return 2;

	std::this_thread::sleep_until(std::chrono::system_clock::time_point(std::chrono::milliseconds(std::strtoll(start, nullptr, 10))));
	if (!Internal::OpenLogFile(std::filesystem::path(directory)))
		return 4;

	const uint64_t pid = ProcessId();
	DE_WARN("writer {} start", pid);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	DE_WARN("writer {} end", pid);

	const std::string text = ReadText(Log::GetFilePath());
	const bool kept = text.find(std::format("writer {} start", pid)) != std::string::npos
		&& text.find(std::format("writer {} end", pid)) != std::string::npos;
	return kept ? 0 : 3;
}

TEST_SUITE("Log")
{
	TEST_CASE("instances started together each keep a log of their own")
	{
		TempDirectory directory;
		constexpr int k_Rounds = 16;
		constexpr int k_Children = 4;
		for (int round = 0; round < k_Rounds; round++)
		{
			// A fresh folder each round: Windows reuses process ids, so a writer could match an earlier round's.
			std::error_code ignored;
			std::filesystem::remove_all(directory.Path, ignored);
			std::filesystem::create_directories(directory.Path, ignored);
			for (const char* name : { "Dingo.log", "Dingo.1.log", "Dingo.2.log" })
				std::ofstream(directory.Path / name, std::ios::binary) << "an earlier run\n";

			const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
			SetVariables(directory.Path, now.count() + 150);

			std::vector<Child> children;
			for (int i = 0; i < k_Children; i++)
			{
				Child child;
				if (child.Start())
					children.push_back(child);
			}
			if (children.size() != k_Children)
			{
				for (Child& child : children)
					child.Wait();
				const std::string message = std::format("only {} writers started", children.size());
				FAIL(message);
			}
			std::vector<uint64_t> pids;
			for (Child& child : children)
			{
				pids.push_back(child.Id());
				CHECK_MESSAGE(child.Wait() == 0, "round ", round, ", writer ", child.Id());
			}

			const std::map<std::filesystem::path, Writers> files = ReadWriters(directory.Path);
			for (const auto& [path, writers] : files)
			{
				std::set<uint64_t> all = writers.Started;
				all.insert(writers.Ended.begin(), writers.Ended.end());
				CHECK_MESSAGE(all.size() <= 1, "round ", round, ": ", FileSystem::PathToUtf8(path.filename()), " holds ", all.size(), " writers");
			}
			for (uint64_t pid : pids)
			{
				int started = 0;
				bool ended = false;
				for (const auto& [path, writers] : files)
				{
					if (writers.Started.contains(pid))
					{
						started++;
						ended = writers.Ended.contains(pid);
					}
				}
				CHECK_MESSAGE(started == 1, "round ", round, ": writer ", pid, "'s start is in ", started, " files");
				CHECK_MESSAGE(ended, "round ", round, ": writer ", pid, "'s end isn't beside its start");
			}
		}
	}
}
