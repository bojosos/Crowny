#pragma once

#include <memory>

#ifdef CW_DEBUG
#define CW_ENABLE_ASSERTS
#endif

#if defined(_MSC_VER)
	#define CW_DEBUGBREAK() __debugbreak()
#elif defined(CW_PLATFORM_LINUX)
	#define CW_DEBUGBREAK() raise(SIGTRAP)
#endif

#ifdef CW_ENABLE_ASSERTS
	#define CW_CLIENT_ASSERT(x, ...) { if(!(x)) { CW_CLIENT_ERROR("Assertion Failed: {0}", __VA_ARGS__); CW_DEBUGBREAK(); } }
	#define CW_ENGINE_ASSERT(x, ...) { if(!(x)) { CW_ENGINE_ERROR("Assertion Failed: {0}", __VA_ARGS__); CW_DEBUGBREAK(); } }
#else
	#define CW_CLIENT_ASSERT(x, ...)
	#define CW_ENGINE_ASSERT(x, ...)
#endif

#define BIT(x) (1 << x)

#define CW_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)

namespace Crowny {

	template<typename T>
	using Scope = std::unique_ptr<T>;
	template<typename T, typename ... Args>
	constexpr Scope<T> CreateScope(Args&& ... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	template<typename T>
	using Ref = std::shared_ptr<T>;
	template<typename T, typename ... Args>
	constexpr Ref<T> CreateRef(Args&& ... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}

}