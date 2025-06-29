#pragma once
#include <memory>
#include <format>

#include <glm/glm.hpp>

namespace DingoEngine
{

	class Log
	{
	public:
		enum class Type : uint8_t
		{
			Engine,
			Client
		};

		enum class Level : uint8_t
		{
			Trace,
			Info,
			Warn,
			Error,
			Fatal
		};

	public:
		static void Initialize();
		static void Shutdown();

		template<typename... Args>
		static void Print(Log::Type type, Log::Level level, std::format_string<Args...> format, Args&&... args);

		template<typename... Args>
		static void PrintTag(Log::Type type, Log::Level level, std::string_view tag, std::format_string<Args...> format, Args&&... args);

	private:
		static void PrintInternal(Log::Type type, Log::Level level, const std::string_view formatted);
		static void PrintInternalTag(Log::Type type, Log::Level level, const std::string_view tag, const std::string_view formatted);
	};

}

// Core log macros
#define DE_CORE_TRACE(...)				::DingoEngine::Log::Print(::DingoEngine::Log::Type::Engine, ::DingoEngine::Log::Level::Trace, __VA_ARGS__)
#define DE_CORE_INFO(...)				::DingoEngine::Log::Print(::DingoEngine::Log::Type::Engine, ::DingoEngine::Log::Level::Info, __VA_ARGS__)
#define DE_CORE_WARN(...)				::DingoEngine::Log::Print(::DingoEngine::Log::Type::Engine, ::DingoEngine::Log::Level::Warn, __VA_ARGS__)
#define DE_CORE_ERROR(...)				::DingoEngine::Log::Print(::DingoEngine::Log::Type::Engine, ::DingoEngine::Log::Level::Error, __VA_ARGS__)
#define DE_CORE_FATAL(...)				::DingoEngine::Log::Print(::DingoEngine::Log::Type::Engine, ::DingoEngine::Log::Level::Fatal, __VA_ARGS__)

// Client log macros
#define DE_TRACE(...)					::DingoEngine::Log::Print(::DingoEngine::Log::Type::Client, ::DingoEngine::Log::Level::Trace, __VA_ARGS__)
#define DE_INFO(...)					::DingoEngine::Log::Print(::DingoEngine::Log::Type::Client, ::DingoEngine::Log::Level::Info, __VA_ARGS__)
#define DE_WARN(...)					::DingoEngine::Log::Print(::DingoEngine::Log::Type::Client, ::DingoEngine::Log::Level::Warn, __VA_ARGS__)
#define DE_ERROR(...)					::DingoEngine::Log::Print(::DingoEngine::Log::Type::Client, ::DingoEngine::Log::Level::Error, __VA_ARGS__)
#define DE_FATAL(...)					::DingoEngine::Log::Print(::DingoEngine::Log::Type::Client, ::DingoEngine::Log::Level::Fatal, __VA_ARGS__)


// Engine log macros (tagged)
#define DE_CORE_TRACE_TAG(tag, ...)		::DingoEngine::Log::PrintTag(::DingoEngine::Log::Type::Engine, ::DingoEngine::Log::Level::Trace, tag, __VA_ARGS__)
#define DE_CORE_INFO_TAG(tag, ...)		::DingoEngine::Log::PrintTag(::DingoEngine::Log::Type::Engine, ::DingoEngine::Log::Level::Info, tag, __VA_ARGS__)
#define DE_CORE_WARN_TAG(tag, ...)		::DingoEngine::Log::PrintTag(::DingoEngine::Log::Type::Engine, ::DingoEngine::Log::Level::Warn, tag, __VA_ARGS__)
#define DE_CORE_ERROR_TAG(tag, ...)		::DingoEngine::Log::PrintTag(::DingoEngine::Log::Type::Engine, ::DingoEngine::Log::Level::Error, tag, __VA_ARGS__)
#define DE_CORE_FATAL_TAG(tag, ...)		::DingoEngine::Log::PrintTag(::DingoEngine::Log::Type::Engine, ::DingoEngine::Log::Level::Fatal, tag, __VA_ARGS__)

// Client log macros (tagged)
#define DE_TRACE_TAG(tag, ...)			::DingoEngine::Log::PrintTag(::DingoEngine::Log::Type::Client, ::DingoEngine::Log::Level::Trace, tag, __VA_ARGS__)
#define DE_INFO_TAG(tag, ...)			::DingoEngine::Log::PrintTag(::DingoEngine::Log::Type::Client, ::DingoEngine::Log::Level::Info, tag, __VA_ARGS__)
#define DE_WARN_TAG(tag, ...)			::DingoEngine::Log::PrintTag(::DingoEngine::Log::Type::Client, ::DingoEngine::Log::Level::Warn, tag, __VA_ARGS__)
#define DE_ERROR_TAG(tag, ...)			::DingoEngine::Log::PrintTag(::DingoEngine::Log::Type::Client, ::DingoEngine::Log::Level::Error, tag, __VA_ARGS__)
#define DE_FATAL_TAG(tag, ...)			::DingoEngine::Log::PrintTag(::DingoEngine::Log::Type::Client, ::DingoEngine::Log::Level::Fatal, tag, __VA_ARGS__)

namespace DingoEngine
{

	template<typename ...Args>
	inline void Log::Print(Log::Type type, Log::Level level, std::format_string<Args...> format, Args && ...args)
	{
		PrintInternal(type, level, std::format(format, std::forward<Args>(args)...));
	}

	template<typename ...Args>
	inline void Log::PrintTag(Log::Type type, Log::Level level, std::string_view tag, std::format_string<Args...> format, Args && ...args)
	{
		PrintInternalTag(type, level, tag, std::format(format, std::forward<Args>(args)...));
	}

}

template<>
struct std::formatter<glm::vec2> : std::formatter<std::string>
{
	template<typename FormatContext>
	auto format(const glm::vec2& vec, FormatContext& ctx) const
	{
		return std::formatter<std::string>::format(std::format("({}, {})", vec.x, vec.y), ctx);
	}
};

template<>
struct std::formatter<glm::vec3> : std::formatter<std::string>
{
	template<typename FormatContext>
	auto format(const glm::vec3& vec, FormatContext& ctx) const
	{
		return std::formatter<std::string>::format(std::format("({}, {}, {})", vec.x, vec.y, vec.z), ctx);
	}
};

template<>
struct std::formatter<glm::vec4> : std::formatter<std::string>
{
	template<typename FormatContext>
	auto format(const glm::vec4& vec, FormatContext& ctx) const
	{
		return std::formatter<std::string>::format(std::format("({}, {}, {}, {})", vec.x, vec.y, vec.z, vec.w), ctx);
	}
};


