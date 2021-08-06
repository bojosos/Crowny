#pragma once

#include <cstring>
#include <functional>
#include <memory>

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

#define CW_BIND_EVENT_FN(fn)                                                                                           \
    [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

namespace Crowny
{

    constexpr void HashCombine(std::size_t& seed) {}

    /**
     * @brief Hashes multiple variables of the same type and combines their hashes.
     *
     * @tparam Type
     * @tparam Rest
     * @param seed Output seed.
     * @param v First value to hash.
     * @param rest The rest of the values to hash.
     */
    template <typename T, typename... Rest> constexpr void HashCombine(std::size_t& seed, const T& v, Rest... rest)
    {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        HashCombine(seed, rest...);
    }

    /**
     * @brief Hashes a variable.
     *
     * @tparam Type.
     * @param Value.
     * @return size_t hash of the variable.
     */
    template <typename T> constexpr size_t Hash(const T& v)
    {
        std::hash<T> hasher;
        return hasher(v);
    }

    template <typename T> using Scope = std::unique_ptr<T>;

    /**
     * @brief Creates a unique pointer.
     *
     * @tparam Smart pointer type.
     */
    template <typename T, typename... Args> constexpr Scope<T> CreateScope(Args&&... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    template <typename T> using Ref = std::shared_ptr<T>;

    /**
     * @brief Creates a shared pointer.
     *
     * @tparam Smart pointer type.
     */
    template <typename T, typename... Args> constexpr Ref<T> CreateRef(Args&&... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    template <class T> void Cw_ZeroOut(T& s) { std::memset(&s, 0, sizeof(T)); }

    template <class T> void Cw_ZeroOut(T* arr, size_t count) { std::memset(arr, 0, sizeof(T) * count); }

    template <class T, size_t N> void Cw_ZeroOut(T (&arr)[N]) { std::memset(arr, 0, sizeof(T) * N); }

    template <class T> void Cw_Copy(T* dst, T* src, size_t count) { std::memcpy(dst, src, sizeof(T) * count); }

    template <class T, size_t N> void Cw_Copy(T (&dst)[N], T (&src)[N], size_t count)
    {
        std::memcpy(dst, src, sizeof(T) * count);
    }

    template <class T, size_t N> constexpr size_t Cw_Size(const T (&array)[N]) { return N; }

} // namespace Crowny

#include "Crowny/Common/Assert.h"
