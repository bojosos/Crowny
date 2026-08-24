#pragma once

#include "Crowny/Common/Hash.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace Crowny
{
    /**
     * @brief Non-owning string view with a precomputed 64-bit CityHash hash.
     *
     * The referenced characters must outlive this object. Equality verifies the
     * hash, length, and characters, so a hash collision cannot make different
     * strings compare equal.
     */
    class HashedString
    {
    public:
        using HashValue = uint64_t;

        constexpr HashedString() noexcept = default;
        constexpr explicit HashedString(std::string_view value) noexcept
          : m_Data(value.data()), m_Size(value.size()), m_Hash(Hashing::CityHash64(value))
        {
        }
        constexpr HashedString(const char* data, size_t size) noexcept : m_Data(data), m_Size(size), m_Hash(Hashing::CityHash64(data, size)) {}

        constexpr const char* Data() const noexcept { return m_Data; }
        constexpr size_t GetSize() const noexcept { return m_Size; }
        constexpr HashValue GetHash() const noexcept { return m_Hash; }
        constexpr bool IsEmpty() const noexcept { return m_Size == 0; }
        constexpr std::string_view GetView() const noexcept { return m_Data == nullptr ? std::string_view() : std::string_view(m_Data, m_Size); }

        constexpr bool operator==(const HashedString& rhs) const noexcept
        {
            if (m_Hash != rhs.m_Hash || m_Size != rhs.m_Size)
                return false;

            for (size_t i = 0; i < m_Size; i++)
            {
                if (m_Data[i] != rhs.m_Data[i])
                    return false;
            }
            return true;
        }

        constexpr bool operator!=(const HashedString& rhs) const noexcept { return !(*this == rhs); }

    private:
        const char* m_Data = nullptr;
        size_t m_Size = 0;
        HashValue m_Hash = Hashing::Detail::K2;
    };

    /** Hashes owning strings and non-owning views with CityHash64. */
    struct StringHash
    {
        using is_transparent = void;

        constexpr size_t operator()(std::string_view value) const noexcept { return static_cast<size_t>(Hashing::CityHash64(value)); }
        size_t operator()(const std::string& value) const noexcept { return (*this)(std::string_view(value)); }
        size_t operator()(const char* value) const noexcept { return (*this)(std::string_view(value)); }
        size_t operator()(const HashedString& value) const noexcept { return static_cast<size_t>(value.GetHash()); }
    };

    /** Compares heterogeneous string keys by their complete text. */
    struct StringEqual
    {
        using is_transparent = void;

        bool operator()(std::string_view lhs, std::string_view rhs) const noexcept { return lhs == rhs; }
        bool operator()(const HashedString& lhs, const HashedString& rhs) const noexcept { return lhs == rhs; }
        bool operator()(const HashedString& lhs, std::string_view rhs) const noexcept { return lhs.GetView() == rhs; }
        bool operator()(std::string_view lhs, const HashedString& rhs) const noexcept { return lhs == rhs.GetView(); }
    };

    namespace Literals
    {
        constexpr HashedString operator""_hstr(const char* value, size_t size) noexcept { return HashedString(value, size); }
    } // namespace Literals
} // namespace Crowny

namespace std
{
    template <> struct hash<Crowny::HashedString>
    {
        size_t operator()(const Crowny::HashedString& value) const noexcept { return static_cast<size_t>(value.GetHash()); }
    };
} // namespace std
