#include "depch.h"
#include "DingoEngine/Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/sinks/dist_sink.h>

#include "DingoEngine/Core/CrashHandler.h"
#include "DingoEngine/Core/LogLifecycle.h"

#include <fstream>
#include <mutex>

#ifdef DE_PLATFORM_LINUX
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace Dingo
{

	static std::shared_ptr<spdlog::logger> s_EngineLogger;
	static std::shared_ptr<spdlog::logger> s_ClientLogger;
	static std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> s_EarlySink;

	namespace
	{
		// The loggers' one sink: the file joins and the early buffer leaves under its lock, so a
		// message another thread logs meanwhile lands in exactly one of them.
		class LogSinks final : public spdlog::sinks::dist_sink<std::mutex>
		{
		public:
			using dist_sink<std::mutex>::dist_sink;

			void ReplaceEarlySink(const std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt>& early, const spdlog::sink_ptr& file)
			{
				std::lock_guard<std::mutex> lock(mutex_);
				if (file)
				{
					for (const spdlog::details::log_msg_buffer& message : early->last_raw())
						file->log(message);
					sinks_.push_back(file);
				}
				std::erase(sinks_, early);
			}
		};

		// spdlog's file sinks take a narrow name, which on Windows can't hold every user's
		// %LOCALAPPDATA%; a std::filesystem::path can. Opened for appending, as the crash handler
		// writes to the same file through its own handle.
		class FileSink final : public spdlog::sinks::base_sink<std::mutex>
		{
		public:
			explicit FileSink(const std::filesystem::path& path)
			{
				std::ofstream(path, std::ios::binary | std::ios::trunc).close();
				m_File.open(path, std::ios::binary | std::ios::app);
			}

			bool IsOpen() const { return m_File.is_open(); }

		protected:
			void sink_it_(const spdlog::details::log_msg& message) override
			{
				spdlog::memory_buf_t text;
				formatter_->format(message, text);
				m_File.write(text.data(), static_cast<std::streamsize>(text.size()));
			}

			void flush_() override { m_File.flush(); }

		private:
			std::ofstream m_File;
		};
	}

	static std::shared_ptr<LogSinks> s_Sinks;
	static std::filesystem::path s_FilePath;

	static thread_local uint32_t t_LoggingDepth = 0;

	namespace
	{
		constexpr size_t k_EarlyMessages = 4096;
		constexpr const char* k_FilePattern = "[%T] [%l] %n: %v";

		struct LoggingScope
		{
			LoggingScope() { ++t_LoggingDepth; }
			~LoggingScope() { --t_LoggingDepth; }
		};

		uint64_t ProcessId()
		{
#ifdef DE_PLATFORM_WINDOWS
			return GetCurrentProcessId();
#elif defined(DE_PLATFORM_LINUX)
			return static_cast<uint64_t>(getpid());
#else
			return 0;
#endif
		}

		std::string Utf8(const std::filesystem::path& path)
		{
			const std::u8string text = path.u8string();
			return std::string(text.begin(), text.end());
		}

		std::filesystem::path RunLogPath(const std::filesystem::path& directory, uint32_t run)
		{
			return directory / (run == 0 ? std::string("Dingo.log") : std::format("Dingo.{}.log", run));
		}

		// Logs written beside a Dingo.log another instance held open, and earlier runs' logs a
		// rotation couldn't shift, oldest dropped first.
		void PruneSideLogs(const std::filesystem::path& directory, uint32_t keep)
		{
			std::error_code error;
			std::vector<std::pair<std::filesystem::file_time_type, std::filesystem::path>> logs;
			for (const auto& entry : std::filesystem::directory_iterator(directory, error))
			{
				const std::u8string name = entry.path().filename().u8string();
				if (name.starts_with(u8"Dingo-") && name.ends_with(u8".log"))
					logs.emplace_back(entry.last_write_time(error), entry.path());
			}
			if (logs.size() <= keep)
				return;
			std::sort(logs.begin(), logs.end());
			for (size_t i = 0; i < logs.size() - keep; i++)
				std::filesystem::remove(logs[i].second, error);
		}

#ifdef DE_PLATFORM_LINUX
		// Linux renames a file another process has open, so a running instance holds a lock on its
		// Dingo.log for the others to find. (Windows refuses the rename, which tells them the same.)
		int s_LockFd = -1;

		bool IsLogInUse(const std::filesystem::path& path)
		{
			const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
			if (fd < 0)
				return false;
			const bool inUse = flock(fd, LOCK_EX | LOCK_NB) != 0;
			close(fd);
			return inUse;
		}

		void HoldLog(const std::filesystem::path& path)
		{
			s_LockFd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
			if (s_LockFd >= 0)
				flock(s_LockFd, LOCK_EX | LOCK_NB);
		}
#else
		bool IsLogInUse(const std::filesystem::path&) { return false; }
		void HoldLog(const std::filesystem::path&) {}
#endif

		// Dingo.log becomes Dingo.1.log and so on, keeping `keepRuns` in all. The newest log first
		// moves aside under a side-log name, so a rotation that fails part way (a file held open, a
		// crash) never overwrites a log and leaves at worst a side log the pruning keeps in bounds.
		std::filesystem::path PickLogPath(const std::filesystem::path& directory, uint32_t keepRuns)
		{
			std::error_code error;
			const std::filesystem::path newest = RunLogPath(directory, 0);
			const std::filesystem::path sideLog = directory / std::format("Dingo-{}.log", ProcessId());
			if (!std::filesystem::exists(newest, error))
				return newest;
			if (IsLogInUse(newest))
			{
				PruneSideLogs(directory, keepRuns - 1);
				return sideLog;
			}

			const std::filesystem::path aside = directory / std::format("Dingo-{}-previous.log", ProcessId());
			std::filesystem::rename(newest, aside, error);
			if (error)
			{
				PruneSideLogs(directory, keepRuns - 1);
				return sideLog;
			}

			bool shifted = true;
			std::filesystem::remove(RunLogPath(directory, keepRuns - 1), error);
			for (uint32_t run = keepRuns - 1; run > 1 && shifted; --run)
			{
				const std::filesystem::path from = RunLogPath(directory, run - 1);
				if (!std::filesystem::exists(from, error))
					continue;
				std::filesystem::rename(from, RunLogPath(directory, run), error);
				shifted = !error;
			}
			if (keepRuns == 1)
				std::filesystem::remove(aside, error);
			else if (shifted)
				std::filesystem::rename(aside, RunLogPath(directory, 1), error);
			PruneSideLogs(directory, keepRuns - 1);
			return newest;
		}

		void DropEarlySink(const spdlog::sink_ptr& file)
		{
			if (!s_EarlySink)
				return;
			s_Sinks->ReplaceEarlySink(s_EarlySink, file);
			s_EarlySink.reset();
		}
	}

	bool Internal::IsLoggingOnThisThread()
	{
		return t_LoggingDepth != 0;
	}

	namespace Utils
	{

		static std::shared_ptr<spdlog::logger> GetLogger(Log::Type type)
		{
			switch (type)
			{
				case Log::Type::Engine: return s_EngineLogger;
				case Log::Type::Client: return s_ClientLogger;
				default: break;
			}

			DE_CORE_ASSERT(false, "Unknown Log Type");
			return nullptr;
		}

		static spdlog::level::level_enum ToSpdlogLevel(Log::Level level)
		{
			switch (level)
			{
				case Log::Level::Trace:    return spdlog::level::trace;
				case Log::Level::Info:     return spdlog::level::info;
				case Log::Level::Warn:     return spdlog::level::warn;
				case Log::Level::Error:    return spdlog::level::err;
				case Log::Level::Fatal:    return spdlog::level::critical;
			}
			return spdlog::level::trace;
		}

	}

	void Internal::InitializeLog()
	{
		auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		console->set_pattern("%^[%T] %n: %v%$");
		s_EarlySink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(k_EarlyMessages);
		s_Sinks = std::make_shared<LogSinks>(std::vector<spdlog::sink_ptr>{ console, s_EarlySink });
		std::vector<spdlog::sink_ptr> logSinks{ s_Sinks };

		// flush_on(trace) forced a synchronous file flush on EVERY log call — a
		// guaranteed I/O stall if anything ever logs per frame. Warnings and above
		// still flush immediately (crash diagnosis); lower levels rely on spdlog's
		// buffering and the flush in Shutdown.
		s_EngineLogger = std::make_shared<spdlog::logger>("Dingo", begin(logSinks), end(logSinks));
		spdlog::register_logger(s_EngineLogger);
		s_EngineLogger->set_level(spdlog::level::trace);
		s_EngineLogger->flush_on(spdlog::level::warn);

		s_ClientLogger = std::make_shared<spdlog::logger>("Game", begin(logSinks), end(logSinks));
		spdlog::register_logger(s_ClientLogger);
		s_ClientLogger->set_level(spdlog::level::trace);
		s_ClientLogger->flush_on(spdlog::level::warn);
	}

	void Internal::ShutdownLog()
	{
		Log::Flush();
		s_ClientLogger.reset();
		s_EngineLogger.reset();
		s_EarlySink.reset();
		s_Sinks.reset();
		spdlog::drop_all();
	}

	bool Internal::OpenLogFile(const std::filesystem::path& directory, uint32_t keepRuns)
	{
		if (!s_FilePath.empty() || !s_EngineLogger)
			return !s_FilePath.empty();

		std::error_code error;
		std::filesystem::create_directories(directory, error);
		const std::filesystem::path path = PickLogPath(directory, (std::max)(keepRuns, 1u));

		const auto fileSink = std::make_shared<FileSink>(path);
		if (!fileSink->IsOpen())
		{
			DropEarlySink(nullptr);
			DE_CORE_ERROR("Log: can't write a log file in '{}'; logging to the console only", Utf8(directory));
			return false;
		}
		HoldLog(path);

		fileSink->set_pattern(k_FilePattern);
		if (s_EarlySink)
			DropEarlySink(fileSink);
		else
			s_Sinks->add_sink(fileSink);

		s_FilePath = std::filesystem::absolute(path, error);
		if (error)
			s_FilePath = path;
		DE_CORE_INFO("Log file: {}", Utf8(s_FilePath));
		return true;
	}

	const std::filesystem::path& Log::GetFilePath()
	{
		return s_FilePath;
	}

	void Log::Flush()
	{
		LoggingScope scope;
		if (s_EngineLogger) s_EngineLogger->flush();
		if (s_ClientLogger) s_ClientLogger->flush();
	}

	void Log::PrintAssertionMessage(Log::Type type, std::string_view message)
	{
		ReportAssertion(type, {}, message);
	}

	void Log::ReportAssertion(Log::Type type, std::string_view tag, std::string_view message)
	{
		if (tag.empty())
			PrintInternal(type, Log::Level::Fatal, std::format("Assertion failed: {0}", message));
		else
			PrintInternalTag(type, Log::Level::Fatal, tag, message);

#if !defined(DE_RELEASE) && defined(DE_PLATFORM_WINDOWS)
		const std::string text = tag.empty() ? std::string(message) : std::format("{}: {}", tag, message);
		MessageBoxA(nullptr, text.c_str(), "Dingo Assert", MB_OK | MB_ICONERROR);
#endif
	}

	bool Log::ShouldLog(Log::Type type, Log::Level level)
	{
		auto logger = Utils::GetLogger(type);
		return logger && logger->should_log(Utils::ToSpdlogLevel(level));
	}

	void Log::PrintInternal(Log::Type type, Log::Level level, const std::string_view formatted)
	{
		LoggingScope scope;
		auto logger = Utils::GetLogger(type);
		logger->log(Utils::ToSpdlogLevel(level), formatted);
	}

	void Log::PrintInternalTag(Log::Type type, Log::Level level, const std::string_view tag, const std::string_view formatted)
	{
		LoggingScope scope;
		auto logger = Utils::GetLogger(type);
		logger->log(Utils::ToSpdlogLevel(level), "[{}] {}", tag, formatted);
	}

}
