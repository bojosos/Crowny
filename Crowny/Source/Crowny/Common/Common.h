#pragma once

#include "Crowny/Common/Hash.h"

#include <cstddef>
#include <cstring>
#include <functional>

// Forward-declare cereal::access so CW_SERIALIZABLE can friend it
namespace cereal { class access; }

#ifdef CW_DEBUG
#define CW_ENABLE_ASSERTS
#endif

#if defined(_MSC_VER)
#define CW_DEBUGBREAK() __debugbreak()
#elif defined(CW_PLATFORM_LINUX)
// #define CW_DEBUGBREAK() __builtin_trap()
#define CW_DEBUGBREAK() asm("int $3")
#endif

#if defined(__clang__)
#define CW_STDCALL __attribute__((stdcall))
#elif defined(__GNUC__)
#define CW_STDCALL __attribute__((stdcall))
#elif defined(__INTEL_COMPILER)
#define CW_STDCALL __stdcall
#elif defined(_MSC_VER)
#define CW_STDCALL __stdcall
#endif

#ifndef WIN32
#define stricmp strcasecmp
#endif

#define BIT(x) (1 << x)
#define M_PI 3.14159265358979323846

#define CW_BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

#define CW_STRINGIFY(token) #token

// Marks a variable, parameter or return value as intentionally unused (cross-platform).
// Prefer the attribute on declarations; use the statement form for values you cannot annotate.
#define CW_MAYBE_UNUSED [[maybe_unused]]
#define CW_UNUSED(...) static_cast<void>(__VA_ARGS__)

class BinaryDataStreamInputArchive;
class BinaryDataStreamOutputArchive;

namespace Crowny
{

#include <cstddef>

#define CW_SIMPLESERIALIZABLE(...) template <typename Archive> friend void Serialize(Archive& archive, __VA_ARGS__& type);

#define CW_SERIALIZABLE(...)                                                                                                                         \
    friend class ::cereal::access;                                                                                                                  \
    friend void Save(BinaryDataStreamOutputArchive& ar, const __VA_ARGS__& type);                                                                    \
    friend void Load(BinaryDataStreamInputArchive& ar, __VA_ARGS__& type);

    constexpr void HashCombine(std::size_t& seed) {}

    /**
     * @brief Hashes multiple variables of the same type and combines their hashes.
     *
     * @tparam Type of the first value.
     * @tparam Types of the rest of the values.
     * @param outSeed Output seed.
     * @param value First value to hash.
     * @param rest The rest of the values to hash.
     */
    template <typename T, typename... Rest> constexpr void HashCombine(std::size_t& outSeed, const T& value, const Rest&... rest)
    {
        Hashing::Hasher<T> hasher;
        outSeed ^= hasher(value) + 0x9e3779b9 + (outSeed << 6) + (outSeed >> 2);
        HashCombine(outSeed, rest...);
    }

    /**
     * @brief Hashes a variable.
     *
     * @tparam Type.
     * @param Value.
     * @return size_t hash of the variable.
     */
    template <typename T> constexpr size_t Hash(const T& value)
    {
        Hashing::Hasher<T> hasher;
        return hasher(value);
    }

    template <class T> void Cw_ZeroOut(T& s) { std::memset(&s, 0, sizeof(T)); }

    template <class T> void Cw_ZeroOut(T* arr, size_t count) { std::memset(arr, 0, sizeof(T) * count); }

    template <class T, size_t N> void Cw_ZeroOut(T (&arr)[N]) { std::memset(arr, 0, sizeof(T) * N); }

    template <class T> void Cw_Copy(T* dst, T* src, size_t count) { std::memcpy(dst, src, sizeof(T) * count); }

    template <class T, size_t N> void Cw_Copy(T (&dst)[N], T (&src)[N], size_t count) { std::memcpy(dst, src, sizeof(T) * count); }

    template <class T, size_t N> constexpr size_t Cw_Size(const T (&array)[N]) { return N; }

} // namespace Crowny

#include "Crowny/Common/Assert.h"
