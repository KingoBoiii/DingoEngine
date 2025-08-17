#pragma once

#include "Common.h"
#include <filesystem>

#include "Log.h"

#ifdef DE_PLATFORM_WINDOWS
#define DE_DEBUG_BREAK __debugbreak()
#endif

#ifdef DE_DEBUG
#define DE_ENABLE_ASSERTS
#endif

#define DE_ENABLE_VERIFY

#ifdef DE_ENABLE_ASSERTS
#define DE_CORE_ASSERT_MESSAGE_INTERNAL(...)  ::Dingo::Log::PrintAssertionMessage(::Dingo::Log::Type::Engine, "Assertion Failed " __VA_ARGS__)
#define DE_ASSERT_MESSAGE_INTERNAL(...)  ::Dingo::Log::PrintAssertionMessage(::Dingo::Log::Type::Client, "Assertion Failed " __VA_ARGS__)

#define DE_CORE_ASSERT(condition, ...) { if(!(condition)) { DE_CORE_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); DE_DEBUG_BREAK; } }
#define DE_ASSERT(condition, ...) { if(!(condition)) { DE_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); DE_DEBUG_BREAK; } }
#else
#define DE_CORE_ASSERT(condition, ...)
#define DE_ASSERT(condition, ...)
#endif

#ifdef DE_ENABLE_VERIFY
#define DE_CORE_VERIFY_MESSAGE_INTERNAL(...)  ::Dingo::Log::PrintAssertionMessage(::Dingo::Log::Type::Engine, "Verify Failed " __VA_ARGS__)
#define DE_VERIFY_MESSAGE_INTERNAL(...)  ::Dingo::Log::PrintAssertionMessage(::Dingo::Log::Type::Client, "Verify Failed " __VA_ARGS__)

#define DE_CORE_VERIFY(condition, ...) { if(!(condition)) { DE_CORE_VERIFY_MESSAGE_INTERNAL(__VA_ARGS__); DE_DEBUG_BREAK; } }
#define DE_VERIFY(condition, ...) { if(!(condition)) { DE_VERIFY_MESSAGE_INTERNAL(__VA_ARGS__); DE_DEBUG_BREAK; } }
#else
#define DE_CORE_VERIFY(condition, ...)
#define DE_VERIFY(condition, ...)
#endif
