#include "depch.h"
#include "DingoEngine/Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/ringbuffer_sink.h>

#include "DingoEngine/Core/CrashHandler.h"

#ifdef DE_PLATFORM_LINUX
#include <unistd.h>
#endif

namespace Dingo
{

	static std::shared_ptr<spdlog::logger> s_EngineLogger;
	static std::shared_ptr<spdlog::logger> s_ClientLogger;
	static std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> s_EarlySink;
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

		std::filesystem::path RunLogPath(const std::filesystem::path& directory, uint32_t run)
		{
			return directory / (run == 0 ? std::string("Dingo.log") : std::format("Dingo.{}.log", run));
		}

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

		void AddSink(const spdlog::sink_ptr& sink)
		{
			for (const auto& logger : { s_EngineLogger, s_ClientLogger })
				logger->sinks().push_back(sink);
		}

		void RemoveSink(const spdlog::sink_ptr& sink)
		{
			for (const auto& logger : { s_EngineLogger, s_ClientLogger })
				std::erase(logger->sinks(), sink);
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

	void Log::Initialize()
	{
		std::vector<spdlog::sink_ptr> logSinks;
		logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
		s_EarlySink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(k_EarlyMessages);
		logSinks.emplace_back(s_EarlySink);

		logSinks[0]->set_pattern("%^[%T] %n: %v%$");

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

	void Log::Shutdown()
	{
		Flush();
		s_ClientLogger.reset();
		s_EngineLogger.reset();
		s_EarlySink.reset();
		spdlog::drop_all();
	}

	bool Log::OpenFile(const std::filesystem::path& directory, uint32_t keepRuns)
	{
		if (!s_FilePath.empty() || !s_EngineLogger)
			return !s_FilePath.empty();

		std::error_code error;
		std::filesystem::create_directories(directory, error);

		// A rename fails while another instance still has the file open (Windows), and that log
		// must not be truncated: this run writes beside it.
		std::filesystem::path path = RunLogPath(directory, 0);
		bool rotated = true;
		for (uint32_t run = (std::max)(keepRuns, 1u) - 1; run > 0; --run)
		{
			const std::filesystem::path from = RunLogPath(directory, run - 1);
			if (!std::filesystem::exists(from, error))
				continue;
			std::filesystem::rename(from, RunLogPath(directory, run), error);
			if (error && run == 1)
				rotated = false;
		}
		if (!rotated)
			path = directory / std::format("Dingo-{}.log", ProcessId());

		std::shared_ptr<spdlog::sinks::basic_file_sink_mt> fileSink;
		try
		{
			fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
		}
		catch (const spdlog::spdlog_ex& exception)
		{
			DE_CORE_ERROR("Log: can't write a log file in '{}' ({}); logging to the console only", directory.string(), exception.what());
			return false;
		}

		fileSink->set_pattern(k_FilePattern);
		if (s_EarlySink)
		{
			for (const spdlog::details::log_msg_buffer& message : s_EarlySink->last_raw())
				fileSink->log(message);
			RemoveSink(s_EarlySink);
			s_EarlySink.reset();
		}
		AddSink(fileSink);

		s_FilePath = std::filesystem::absolute(path, error);
		if (error)
			s_FilePath = path;
		DE_CORE_INFO("Log file: {}", s_FilePath.string());
		return true;
	}

	const std::filesystem::path& Log::GetFilePath()
	{
		return s_FilePath;
	}

	void Log::Flush()
	{
		if (s_EngineLogger) s_EngineLogger->flush();
		if (s_ClientLogger) s_ClientLogger->flush();
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
