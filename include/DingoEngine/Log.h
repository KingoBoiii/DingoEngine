#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <format>

#include <glm/glm.hpp>

namespace Dingo
{

	/// Static logging front end (spdlog underneath), safe to call from any thread. Messages go to
	/// the console and to this run's Dingo.log. Prefer the DE_* macros below to calling Print.
	class Log
	{
	public:
		/// Which logger a message goes to.
		enum class Type : uint8_t
		{
			Engine,	///< The engine's own messages (DE_CORE_* macros).
			Client	///< The game's messages (DE_* macros).
		};

		/// Severity of a message, lowest first.
		enum class Level : uint8_t
		{
			Trace,
			Info,
			Warn,	///< Flushed to the file at once, like Error and Fatal.
			Error,
			Fatal	///< Only logs at the highest severity; it neither exits nor breaks.
		};

	public:
		/// This run's log file, or empty before the Application opened it.
		static const std::filesystem::path& GetFilePath();
		/// Writes every buffered message to the console and the log file now.
		static void Flush();

		/// True if a message at this level would actually be emitted. Print/PrintTag
		/// check it before formatting, so a raised log level skips std::format cost.
		static bool ShouldLog(Log::Type type, Log::Level level);

		/// Formats with std::format and logs at the given level; the format string is checked at
		/// compile time. The DE_* macros call this.
		template<typename... Args>
		static void Print(Log::Type type, Log::Level level, std::format_string<Args...> format, Args&&... args);

		/// Like Print, with the message prefixed by "[tag] ". The DE_*_TAG macros call this.
		template<typename... Args>
		static void PrintTag(Log::Type type, Log::Level level, std::string_view tag, std::format_string<Args...> format, Args&&... args);

		/**************************************************
		***		ASSERTION								***
		**************************************************/

		/// Logs an assertion failure at Fatal level under the tag, formatted with std::format.
		/// Used by the assert and verify macros, which do not pass format arguments.
		template<typename... Args>
		static void PrintAssertionMessage(Log::Type type, std::string_view tag, std::format_string<Args...> format, Args&&... args);

		/// Logs "Assertion failed: <message>" at Fatal level, with no format processing. On Windows
		/// Debug and Distribution builds it also shows a message box.
		static void PrintAssertionMessage(Log::Type type, std::string_view message);

	private:
		// Logs it and, in a Debug or Distribution build on Windows, shows it in a message box.
		static void ReportAssertion(Log::Type type, std::string_view tag, std::string_view message);
		static void PrintInternal(Log::Type type, Log::Level level, const std::string_view formatted);
		static void PrintInternalTag(Log::Type type, Log::Level level, const std::string_view tag, const std::string_view formatted);
	};

}

// Core log macros
/// Log through the engine logger at the macro's level (TRACE, INFO, WARN, ERROR or FATAL), with
/// std::format syntax: DE_CORE_INFO("Loaded {} meshes", count). FATAL only logs; it does not exit.
///@{
#define DE_CORE_TRACE(...)				::Dingo::Log::Print(::Dingo::Log::Type::Engine, ::Dingo::Log::Level::Trace, __VA_ARGS__)
#define DE_CORE_INFO(...)				::Dingo::Log::Print(::Dingo::Log::Type::Engine, ::Dingo::Log::Level::Info, __VA_ARGS__)
#define DE_CORE_WARN(...)				::Dingo::Log::Print(::Dingo::Log::Type::Engine, ::Dingo::Log::Level::Warn, __VA_ARGS__)
#define DE_CORE_ERROR(...)				::Dingo::Log::Print(::Dingo::Log::Type::Engine, ::Dingo::Log::Level::Error, __VA_ARGS__)
#define DE_CORE_FATAL(...)				::Dingo::Log::Print(::Dingo::Log::Type::Engine, ::Dingo::Log::Level::Fatal, __VA_ARGS__)
///@}

// Client log macros
/// Log through the game's logger at the macro's level (TRACE, INFO, WARN, ERROR or FATAL), with
/// std::format syntax: DE_INFO("Score {}", score). FATAL only logs; it does not exit.
///@{
#define DE_TRACE(...)					::Dingo::Log::Print(::Dingo::Log::Type::Client, ::Dingo::Log::Level::Trace, __VA_ARGS__)
#define DE_INFO(...)					::Dingo::Log::Print(::Dingo::Log::Type::Client, ::Dingo::Log::Level::Info, __VA_ARGS__)
#define DE_WARN(...)					::Dingo::Log::Print(::Dingo::Log::Type::Client, ::Dingo::Log::Level::Warn, __VA_ARGS__)
#define DE_ERROR(...)					::Dingo::Log::Print(::Dingo::Log::Type::Client, ::Dingo::Log::Level::Error, __VA_ARGS__)
#define DE_FATAL(...)					::Dingo::Log::Print(::Dingo::Log::Type::Client, ::Dingo::Log::Level::Fatal, __VA_ARGS__)
///@}


// Engine log macros (tagged)
/// Like the DE_CORE_* macros, with the message prefixed by "[tag] ": DE_CORE_WARN_TAG("Audio",
/// "No device {}", id). The tag is a string_view.
///@{
#define DE_CORE_TRACE_TAG(tag, ...)		::Dingo::Log::PrintTag(::Dingo::Log::Type::Engine, ::Dingo::Log::Level::Trace, tag, __VA_ARGS__)
#define DE_CORE_INFO_TAG(tag, ...)		::Dingo::Log::PrintTag(::Dingo::Log::Type::Engine, ::Dingo::Log::Level::Info, tag, __VA_ARGS__)
#define DE_CORE_WARN_TAG(tag, ...)		::Dingo::Log::PrintTag(::Dingo::Log::Type::Engine, ::Dingo::Log::Level::Warn, tag, __VA_ARGS__)
#define DE_CORE_ERROR_TAG(tag, ...)		::Dingo::Log::PrintTag(::Dingo::Log::Type::Engine, ::Dingo::Log::Level::Error, tag, __VA_ARGS__)
#define DE_CORE_FATAL_TAG(tag, ...)		::Dingo::Log::PrintTag(::Dingo::Log::Type::Engine, ::Dingo::Log::Level::Fatal, tag, __VA_ARGS__)
///@}

// Client log macros (tagged)
/// Like the DE_* macros, with the message prefixed by "[tag] ":
/// DE_WARN_TAG("Save", "Slot {}", name).
///@{
#define DE_TRACE_TAG(tag, ...)			::Dingo::Log::PrintTag(::Dingo::Log::Type::Client, ::Dingo::Log::Level::Trace, tag, __VA_ARGS__)
#define DE_INFO_TAG(tag, ...)			::Dingo::Log::PrintTag(::Dingo::Log::Type::Client, ::Dingo::Log::Level::Info, tag, __VA_ARGS__)
#define DE_WARN_TAG(tag, ...)			::Dingo::Log::PrintTag(::Dingo::Log::Type::Client, ::Dingo::Log::Level::Warn, tag, __VA_ARGS__)
#define DE_ERROR_TAG(tag, ...)			::Dingo::Log::PrintTag(::Dingo::Log::Type::Client, ::Dingo::Log::Level::Error, tag, __VA_ARGS__)
#define DE_FATAL_TAG(tag, ...)			::Dingo::Log::PrintTag(::Dingo::Log::Type::Client, ::Dingo::Log::Level::Fatal, tag, __VA_ARGS__)
///@}

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

/// Lets a glm::vec2 be a log or std::format argument; prints "(x, y)".
template<>
struct std::formatter<glm::vec2> : std::formatter<std::string>
{
	/// Formats the vector as "(x, y)".
	template<typename FormatContext>
	auto format(const glm::vec2& vec, FormatContext& ctx) const
	{
		return std::formatter<std::string>::format(std::format("({}, {})", vec.x, vec.y), ctx);
	}
};

/// Lets a glm::vec3 be a log or std::format argument; prints "(x, y, z)".
template<>
struct std::formatter<glm::vec3> : std::formatter<std::string>
{
	/// Formats the vector as "(x, y, z)".
	template<typename FormatContext>
	auto format(const glm::vec3& vec, FormatContext& ctx) const
	{
		return std::formatter<std::string>::format(std::format("({}, {}, {})", vec.x, vec.y, vec.z), ctx);
	}
};

/// Lets a glm::vec4 be a log or std::format argument; prints "(x, y, z, w)".
template<>
struct std::formatter<glm::vec4> : std::formatter<std::string>
{
	/// Formats the vector as "(x, y, z, w)".
	template<typename FormatContext>
	auto format(const glm::vec4& vec, FormatContext& ctx) const
	{
		return std::formatter<std::string>::format(std::format("({}, {}, {}, {})", vec.x, vec.y, vec.z, vec.w), ctx);
	}
};


