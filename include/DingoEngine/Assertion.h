#pragma once

#include "Common.h"
#include <filesystem>

#include "Log.h"
#include "DingoEngine/Core/FatalError.h"

#if defined(DE_PLATFORM_WINDOWS)
#define DE_DEBUG_BREAK __debugbreak()
#elif defined(DE_PLATFORM_LINUX)
#include <csignal>
#define DE_DEBUG_BREAK std::raise(SIGTRAP)
#else
#include <cstdlib>
#define DE_DEBUG_BREAK std::abort()
#endif

#ifdef DE_DEBUG
#define DE_ENABLE_ASSERTS
#endif

#define DE_ENABLE_VERIFY

#ifdef DE_ENABLE_ASSERTS
#define DE_CORE_ASSERT_MESSAGE_INTERNAL(...)  ::Dingo::Log::PrintAssertionMessage(::Dingo::Log::Type::Engine, "Assertion Failed " __VA_ARGS__)
#define DE_ASSERT_MESSAGE_INTERNAL(...)  ::Dingo::Log::PrintAssertionMessage(::Dingo::Log::Type::Client, "Assertion Failed " __VA_ARGS__)

#define DE_CORE_ASSERT(condition, ...) do { if (!(condition)) { DE_CORE_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); DE_DEBUG_BREAK; } } while (0)
#define DE_ASSERT(condition, ...) do { if (!(condition)) { DE_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); DE_DEBUG_BREAK; } } while (0)
#else
#define DE_CORE_ASSERT(condition, ...) ((void)0)
#define DE_ASSERT(condition, ...) ((void)0)
#endif

#if defined(DE_ENABLE_VERIFY) && defined(DE_DISTRIBUTION)
// A shipped game has no debugger to break into: a failed verify is a fatal error with a message.
#define DE_CORE_VERIFY(condition, ...) do { if (!(condition)) { ::Dingo::FatalError("Verify Failed " __VA_ARGS__); } } while (0)
#define DE_VERIFY(condition, ...) do { if (!(condition)) { ::Dingo::FatalError("Verify Failed " __VA_ARGS__); } } while (0)
#elif defined(DE_ENABLE_VERIFY)
#define DE_CORE_VERIFY_MESSAGE_INTERNAL(...)  ::Dingo::Log::PrintAssertionMessage(::Dingo::Log::Type::Engine, "Verify Failed " __VA_ARGS__)
#define DE_VERIFY_MESSAGE_INTERNAL(...)  ::Dingo::Log::PrintAssertionMessage(::Dingo::Log::Type::Client, "Verify Failed " __VA_ARGS__)

#define DE_CORE_VERIFY(condition, ...) do { if (!(condition)) { DE_CORE_VERIFY_MESSAGE_INTERNAL(__VA_ARGS__); DE_DEBUG_BREAK; } } while (0)
#define DE_VERIFY(condition, ...) do { if (!(condition)) { DE_VERIFY_MESSAGE_INTERNAL(__VA_ARGS__); DE_DEBUG_BREAK; } } while (0)
#else
#define DE_CORE_VERIFY(condition, ...) ((void)0)
#define DE_VERIFY(condition, ...) ((void)0)
#endif
