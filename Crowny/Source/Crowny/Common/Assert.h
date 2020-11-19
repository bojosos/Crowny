#pragma once

#include "Crowny/Common/Log.h"
#include "Crowny/Common/Common.h"

#include <filesystem>

#define CW_EXAPND_MACRO(x) x
#define CW_STRING_MACRO(x) #x

#ifdef CW_ENABLE_ASSERTS
    #define CW_INTERNAL_ASSERT_IMPL(type, condtion, message, ...) { if (!(condtion)) { CW##type##ERROR(message, __VA_ARGS__); CW_DEBUGBREAK(); } }
    #define CW_INTERNAL_ASSERT_WITH_MSG(type, condition, ...) CW_INTERNAL_ASSERT_IMPL(type, condition, "Assertion failed: {0}", __VA_ARGS__)
    #define CW_INTERNAL_ASSERT_NO_MSG(type, condition) CW_INTERNAL_ASSERT_IMPL(type, condition, "Assertion '{0}' failed at {1}:{2}", CW_STRING_MACRO(condition), std::filesystem::path(__FILE__).filename().string(), __LINE__)

    #define CW_INTERNAL_ASSERT_GET_MACRO_NAME(arg1, arg2, macro, ...) macro
    #define CW_INTERNAL_ASSERT_GET_MACRO(...) CW_EXPAND_MACRO( CW_INTERNAL_ASSERT_GET_MACRO_NAME(__VA_ARGS__, CW_INTERNAL_ASSERT_WITH_MSG, CW_INTERNAL_ASSERT_NO_MSG)

    #define CW_ASSERT(...) CW_EXAPND_MACRO(CW_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_, __VA_ARGS__))
    #define CW_ENGINE_ASSERT(...) CW_EXPAND_MACRO(CW_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__), (_ENGINE_, __VA_ARGS__))
#else
    #define CW_ASSERT(...)
    #define CW_ENGINE_ASSERT(...)
#endif