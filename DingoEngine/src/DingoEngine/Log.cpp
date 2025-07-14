#include "depch.h"
#include "DingoEngine/Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace Dingo
{

	static std::shared_ptr<spdlog::logger> s_EngineLogger;
	static std::shared_ptr<spdlog::logger> s_ClientLogger;

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

	}

	void Log::Initialize()
	{
		std::vector<spdlog::sink_ptr> logSinks;
		logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
		logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("Dingo.log", true));

		logSinks[0]->set_pattern("%^[%T] %n: %v%$");
		logSinks[1]->set_pattern("[%T] [%l] %n: %v");

		s_EngineLogger = std::make_shared<spdlog::logger>("Dingo", begin(logSinks), end(logSinks));
		spdlog::register_logger(s_EngineLogger);
		s_EngineLogger->set_level(spdlog::level::trace);
		s_EngineLogger->flush_on(spdlog::level::trace);

		s_ClientLogger = std::make_shared<spdlog::logger>("Game", begin(logSinks), end(logSinks));
		spdlog::register_logger(s_ClientLogger);
		s_ClientLogger->set_level(spdlog::level::trace);
		s_ClientLogger->flush_on(spdlog::level::trace);
	}

	void Log::Shutdown()
	{
		s_ClientLogger.reset();
		s_EngineLogger.reset();
		spdlog::drop_all();
	}

	void Log::PrintInternal(Log::Type type, Log::Level level, const std::string_view formatted)
	{
		auto logger = Utils::GetLogger(type);

		switch (level)
		{
			case Dingo::Log::Level::Trace:
				logger->trace(formatted);
				break;
			case Dingo::Log::Level::Info:
				logger->info(formatted);
				break;
			case Dingo::Log::Level::Warn:
				logger->warn(formatted);
				break;
			case Dingo::Log::Level::Error:
				logger->error(formatted);
				break;
			case Dingo::Log::Level::Fatal:
				logger->critical(formatted);
				break;
		}
	}

	void Log::PrintInternalTag(Log::Type type, Log::Level level, const std::string_view tag, const std::string_view formatted)
	{
		auto logger = Utils::GetLogger(type);

		switch (level)
		{
			case Dingo::Log::Level::Trace:
				logger->trace("[{}] {}", tag, formatted);
				break;
			case Dingo::Log::Level::Info:
				logger->info("[{}] {}", tag, formatted);
				break;
			case Dingo::Log::Level::Warn:
				logger->warn("[{}] {}", tag, formatted);
				break;
			case Dingo::Log::Level::Error:
				logger->error("[{}] {}", tag, formatted);
				break;
			case Dingo::Log::Level::Fatal:
				logger->critical("[{}] {}", tag, formatted);
				break;
		}
	}

}
