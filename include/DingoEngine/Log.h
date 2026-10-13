#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <format>

#include <glm/glm.hpp>

namespace Dingo
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
		// This run's log file, or empty before the Application opened it.
		static const std::filesystem::path& GetFilePath();
		static void Flush();

		// True if a message at this level would actually be emitted. Print/PrintTag
		// check it before formatting, so a raised log level skips std::format cost.
		static bool ShouldLog(Log::Type type, Log::Level level);

		template<typename... Args>
		static void Print(Log::Type type, Log::Level level, std::format_string<Args...> format, Args&&... args);

		template<typename... Args>
		static void PrintTag(Log::Type type, Log::Level level, std::string_view tag, std::format_string<Args...> format, Args&&... args);

		/**************************************************
		***		ASSERTION								***
		**************************************************/

		template<typename... Args>
		static void PrintAssertionMessage(Log::Type type, std::string_view tag, std::format_string<Args...> format, Args&&... args);

		static void PrintAssertionMessage(Log::Type type, std::string_view message);

	private:
		// Logs it and, in a Debug or Distribution build on Windows, shows it in a message box.
		static void ReportAssertion(Log::Type type, std::string_view tag, std::string_view message);
		static void PrintInternal(Log::Type type, Log::Level level, const std::string_view formatted);
		static void PrintInternalTag(Log::Type type, Log::Level level, const std::string_view tag, const std::string_view formatted);
	};

}

// Core log macros
#define DE_CORE_TRACE(...)				::Dingo::Log::Print(::Dingo::Log::Type::Engine, ::Dingo::Log::Level::Trace, __VA_ARGS__)
#define DE_CORE_INFO(...)				::Dingo::Log::Print(::Dingo::Log::Type::Engine, ::Dingo::Log::Level::Info, __VA_ARGS__)
#define DE_CORE_WARN(...)				::Dingo::Log::Print(::Dingo::Log::Type::Engine, ::Dingo::Log::Level::Warn, __VA_ARGS__)
#define DE_CORE_ERROR(...)				::Dingo::Log::Print(::Dingo::Log::Type::Engine, ::Dingo::Log::Level::Error, __VA_ARGS__)
#define DE_CORE_FATAL(...)				::Dingo::Log::Print(::Dingo::Log::Type::Engine, ::Dingo::Log::Level::Fatal, __VA_ARGS__)

// Client log macros
#define DE_TRACE(...)					::Dingo::Log::Print(::Dingo::Log::Type::Client, ::Dingo::Log::Level::Trace, __VA_ARGS__)
#define DE_INFO(...)					::Dingo::Log::Print(::Dingo::Log::Type::Client, ::Dingo::Log::Level::Info, __VA_ARGS__)
#define DE_WARN(...)					::Dingo::Log::Print(::Dingo::Log::Type::Client, ::Dingo::Log::Level::Warn, __VA_ARGS__)
#define DE_ERROR(...)					::Dingo::Log::Print(::Dingo::Log::Type::Client, ::Dingo::Log::Level::Error, __VA_ARGS__)
#define DE_FATAL(...)					::Dingo::Log::Print(::Dingo::Log::Type::Client, ::Dingo::Log::Level::Fatal, __VA_ARGS__)


// Engine log macros (tagged)
#define DE_CORE_TRACE_TAG(tag, ...)		::Dingo::Log::PrintTag(::Dingo::Log::Type::Engine, ::Dingo::Log::Level::Trace, tag, __VA_ARGS__)
#define DE_CORE_INFO_TAG(tag, ...)		::Dingo::Log::PrintTag(::Dingo::Log::Type::Engine, ::Dingo::Log::Level::Info, tag, __VA_ARGS__)
#define DE_CORE_WARN_TAG(tag, ...)		::Dingo::Log::PrintTag(::Dingo::Log::Type::Engine, ::Dingo::Log::Level::Warn, tag, __VA_ARGS__)
#define DE_CORE_ERROR_TAG(tag, ...)		::Dingo::Log::PrintTag(::Dingo::Log::Type::Engine, ::Dingo::Log::Level::Error, tag, __VA_ARGS__)
#define DE_CORE_FATAL_TAG(tag, ...)		::Dingo::Log::PrintTag(::Dingo::Log::Type::Engine, ::Dingo::Log::Level::Fatal, tag, __VA_ARGS__)

// Client log macros (tagged)
#define DE_TRACE_TAG(tag, ...)			::Dingo::Log::PrintTag(::Dingo::Log::Type::Client, ::Dingo::Log::Level::Trace, tag, __VA_ARGS__)
#define DE_INFO_TAG(tag, ...)			::Dingo::Log::PrintTag(::Dingo::Log::Type::Client, ::Dingo::Log::Level::Info, tag, __VA_ARGS__)
#define DE_WARN_TAG(tag, ...)			::Dingo::Log::PrintTag(::Dingo::Log::Type::Client, ::Dingo::Log::Level::Warn, tag, __VA_ARGS__)
#define DE_ERROR_TAG(tag, ...)			::Dingo::Log::PrintTag(::Dingo::Log::Type::Client, ::Dingo::Log::Level::Error, tag, __VA_ARGS__)
#define DE_FATAL_TAG(tag, ...)			::Dingo::Log::PrintTag(::Dingo::Log::Type::Client, ::Dingo::Log::Level::Fatal, tag, __VA_ARGS__)

namespace Dingo
{

	template<typename ...Args>
	inline void Log::Print(Log::Type type, Log::Level level, std::format_string<Args...> format, Args && ...args)
	{
		if (!ShouldLog(type, level))
			return;

		PrintInternal(type, level, std::format(format, std::forward<Args>(args)...));
	}

	template<typename ...Args>
	inline void Log::PrintTag(Log::Type type, Log::Level level, std::string_view tag, std::format_string<Args...> format, Args && ...args)
	{
		if (!ShouldLog(type, level))
			return;

		PrintInternalTag(type, level, tag, std::format(format, std::forward<Args>(args)...));
	}

	template<typename... Args>
	void Log::PrintAssertionMessage(Log::Type type, std::string_view tag, std::format_string<Args...> format, Args&&... args)
	{
		ReportAssertion(type, tag, std::format(format, std::forward<Args>(args)...));
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


