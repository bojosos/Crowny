#pragma once

#include <memory>
#include <signal.h>

#ifdef CW_DEBUG
	#define CW_ENABLE_ASSERTS
#endif

#if defined(_MSC_VER)
	#define CW_DEBUGBREAK() __debugbreak()
#elif defined(CW_PLATFORM_LINUX)
	//#define CW_DEBUGBREAK() __builtin_trap()
	#define CW_DEBUGBREAK() asm("int $3")
#endif

#define BIT(x) (1 << x)

#define CW_BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

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

#include "Crowny/Common/Assert.h"
