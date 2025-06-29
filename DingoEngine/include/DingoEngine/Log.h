#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include <memory>

namespace spdlog
{
	class logger;
}

namespace DingoEngine
{

	class Log
	{
	public:
		static void Initialize();
		static void Shutdown();

		static std::shared_ptr<spdlog::logger> GetEngineLogger()
		{
			return s_EngineLogger;
		}
		static std::shared_ptr<spdlog::logger> GetClientLogger()
		{
			return s_ClientLogger;
		}
	private:
		static std::shared_ptr<spdlog::logger> s_EngineLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};

}

// Core log macros
#define DE_CORE_TRACE(...)		::DingoEngine::Log::GetEngineLogger()->trace(__VA_ARGS__)
#define DE_CORE_INFO(...)		::DingoEngine::Log::GetEngineLogger()->info(__VA_ARGS__)
#define DE_CORE_WARN(...)		::DingoEngine::Log::GetEngineLogger()->warn(__VA_ARGS__)
#define DE_CORE_ERROR(...)		::DingoEngine::Log::GetEngineLogger()->error(__VA_ARGS__)
#define DE_CORE_FATAL(...)		::DingoEngine::Log::GetEngineLogger()->critical(__VA_ARGS__)

// Client log macros
#define DE_TRACE(...)			::DingoEngine::Log::GetClientLogger()->trace(__VA_ARGS__)
#define DE_INFO(...)			::DingoEngine::Log::GetClientLogger()->info(__VA_ARGS__)
#define DE_WARN(...)			::DingoEngine::Log::GetClientLogger()->warn(__VA_ARGS__)
#define DE_ERROR(...)			::DingoEngine::Log::GetClientLogger()->error(__VA_ARGS__)
#define DE_FATAL(...)			::DingoEngine::Log::GetClientLogger()->critical(__VA_ARGS__)

#include "Assertion.h"
