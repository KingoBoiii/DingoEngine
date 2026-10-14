#pragma once

#include "Common.h"
#include <filesystem>

#include "Log.h"
#include "DingoEngine/Core/FatalError.h"

#if defined(DE_PLATFORM_WINDOWS)
/// Breaks into the debugger (__debugbreak) on Windows.
#define DE_DEBUG_BREAK __debugbreak()
#elif defined(DE_PLATFORM_LINUX)
#include <csignal>
/// Raises SIGTRAP on Linux: a debugger stops there, and without one the process dies.
#define DE_DEBUG_BREAK std::raise(SIGTRAP)
#else
#include <cstdlib>
/// Aborts the process on platforms with no debugger break.
#define DE_DEBUG_BREAK std::abort()
#endif

#ifdef DE_DEBUG
/// Defined in Debug builds (DE_DEBUG) only; DE_ASSERT and DE_CORE_ASSERT check only while it is.
#define DE_ENABLE_ASSERTS
#endif

/// Always defined: DE_VERIFY and DE_CORE_VERIFY check in every configuration.
#define DE_ENABLE_VERIFY

#ifdef DE_ENABLE_ASSERTS
/// Implementation detail of DE_CORE_ASSERT.
#define DE_CORE_ASSERT_MESSAGE_INTERNAL(...)  ::Dingo::Log::PrintAssertionMessage(::Dingo::Log::Type::Engine, "Assertion Failed " __VA_ARGS__)
/// Implementation detail of DE_ASSERT.
#define DE_ASSERT_MESSAGE_INTERNAL(...)  ::Dingo::Log::PrintAssertionMessage(::Dingo::Log::Type::Client, "Assertion Failed " __VA_ARGS__)

/// Debug builds only: when condition is false, logs a fatal "Assertion Failed" message to the
/// engine log (plus a message box on Windows) and breaks into the debugger. Other builds compile
/// it to nothing, so condition is not evaluated there. The optional message must be a plain string
/// literal, not a format string: format arguments do not compile.
#define DE_CORE_ASSERT(condition, ...) do { if (!(condition)) { DE_CORE_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); DE_DEBUG_BREAK; } } while (0)
/// Like DE_CORE_ASSERT, but logs to the client (game) log. The message is a plain string literal.
#define DE_ASSERT(condition, ...) do { if (!(condition)) { DE_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); DE_DEBUG_BREAK; } } while (0)
#else
/// Compiled out: condition is not evaluated.
#define DE_CORE_ASSERT(condition, ...) ((void)0)
/// Compiled out: condition is not evaluated.
#define DE_ASSERT(condition, ...) ((void)0)
#endif

#if defined(DE_ENABLE_VERIFY) && defined(DE_DISTRIBUTION)
/// Distribution build: a failed check calls FatalError("Verify Failed <message>"), which logs,
/// shows a message box on Windows and exits with code 1. The message is a plain string literal.
/// A shipped game has no debugger to break into: a failed verify is a fatal error with a message.
#define DE_CORE_VERIFY(condition, ...) do { if (!(condition)) { ::Dingo::FatalError("Verify Failed " __VA_ARGS__); } } while (0)
/// Distribution build: like DE_CORE_VERIFY, for game code. Ends in FatalError on failure.
#define DE_VERIFY(condition, ...) do { if (!(condition)) { ::Dingo::FatalError("Verify Failed " __VA_ARGS__); } } while (0)
#elif defined(DE_ENABLE_VERIFY)
/// Implementation detail of DE_CORE_VERIFY.
#define DE_CORE_VERIFY_MESSAGE_INTERNAL(...)  ::Dingo::Log::PrintAssertionMessage(::Dingo::Log::Type::Engine, "Verify Failed " __VA_ARGS__)
/// Implementation detail of DE_VERIFY.
#define DE_VERIFY_MESSAGE_INTERNAL(...)  ::Dingo::Log::PrintAssertionMessage(::Dingo::Log::Type::Client, "Verify Failed " __VA_ARGS__)

/// Like DE_CORE_ASSERT but checked in every configuration, for conditions the program cannot go
/// on without: logs a fatal "Verify Failed" message and breaks into the debugger. In a
/// Distribution build it ends in FatalError instead. The message is a plain string literal.
#define DE_CORE_VERIFY(condition, ...) do { if (!(condition)) { DE_CORE_VERIFY_MESSAGE_INTERNAL(__VA_ARGS__); DE_DEBUG_BREAK; } } while (0)
/// Like DE_CORE_VERIFY, but logs to the client (game) log.
#define DE_VERIFY(condition, ...) do { if (!(condition)) { DE_VERIFY_MESSAGE_INTERNAL(__VA_ARGS__); DE_DEBUG_BREAK; } } while (0)
#else
/// Compiled out: condition is not evaluated.
#define DE_CORE_VERIFY(condition, ...) ((void)0)
/// Compiled out: condition is not evaluated.
#define DE_VERIFY(condition, ...) ((void)0)
#endif
