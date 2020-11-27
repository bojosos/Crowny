#pragma once

#include "Crowny/Common/Log.h"
#include "Crowny/Common/Common.h"

#include <filesystem>

#define CW_EXPAND_MACRO(x) x
#define CW_STRINGIFY_MACRO(x) #x

#ifdef CW_ENABLE_ASSERTS
	#define CW_INTERNAL_ASSERT_IMPL(type, check, msg, ...) { if(!(check)) { CW##type##ERROR(msg, __VA_ARGS__); CW_DEBUGBREAK(); } }
	#define CW_INTERNAL_ASSERT_WITH_MSG(type, check, ...) CW_INTERNAL_ASSERT_IMPL(type, check, "Assertion failed: {0}", __VA_ARGS__)
	#define CW_INTERNAL_ASSERT_NO_MSG(type, check) CW_INTERNAL_ASSERT_IMPL(type, check, "Assertion '{0}' failed at {1}:{2}", CW_STRINGIFY_MACRO(check), std::filesystem::path(__FILE__).filename().string(), __LINE__)

	#define CW_INTERNAL_ASSERT_GET_MACRO_NAME(arg1, arg2, macro, ...) macro
	#define CW_INTERNAL_ASSERT_GET_MACRO(...) CW_EXPAND_MACRO( CW_INTERNAL_ASSERT_GET_MACRO_NAME(__VA_ARGS__, CW_INTERNAL_ASSERT_WITH_MSG, CW_INTERNAL_ASSERT_NO_MSG) )

	#define CW_ASSERT(...) CW_EXPAND_MACRO( CW_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_, __VA_ARGS__) )
	#define CW_ENGINE_ASSERT(...) CW_EXPAND_MACRO( CW_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_ENGINE_, __VA_ARGS__) )
#else
	#define CW_ASSERT(...)
	#define CW_ENGINE_ASSERT(...)
#endif