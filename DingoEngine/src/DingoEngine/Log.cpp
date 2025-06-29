#include "depch.h"
#include "DingoEngine/Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace DingoEngine
{

	std::shared_ptr<spdlog::logger> Log::s_EngineLogger;
	std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

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

}
