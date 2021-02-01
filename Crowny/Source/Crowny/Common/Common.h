#pragma once

#include <memory>
#include <functional>

#ifdef CW_DEBUG
	#define CW_ENABLE_ASSERTS
#endif

#if defined(_MSC_VER)
	#define CW_DEBUGBREAK() __debugbreak()
#elif defined(CW_PLATFORM_LINUX)
	//#define CW_DEBUGBREAK() __builtin_trap()
	#define CW_DEBUGBREAK() asm("int $3") // it's 2021..... we deserve better
#endif

#define BIT(x) (1 << x)

#define CW_BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

namespace Crowny {

	constexpr void HashCombine(std::size_t& seed) { }

	template <typename T, typename... Rest>
	constexpr void HashCombine(std::size_t& seed, const T& v, Rest... rest)
	{
		std::hash<T> hasher;
		seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
		HashCombine(seed, rest...);
	}

	template <typename T>
	constexpr size_t Hash(const T& v)
	{
		std::hash<T> hasher;
		return hasher(v);
	}

	template <typename T>
	using Scope = std::unique_ptr<T>;
	template<typename T, typename ... Args>
	constexpr Scope<T> CreateScope(Args&& ... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	template <typename T>
	using Ref = std::shared_ptr<T>;
	template <typename T, typename ... Args>
	constexpr Ref<T> CreateRef(Args&& ... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}

}

#include "Crowny/Common/Assert.h"
