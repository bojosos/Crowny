#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

// CityHash, by Geoff Pike and Jyrki Alakuijala.
// This constexpr port of CityHash64 is derived from Google's CityHash source.
//
// Copyright (c) 2011 Google, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

namespace Crowny::Hashing
{
    inline constexpr uint32_t CITY_HASH64_VERSION = 1;

    namespace Detail
    {
        inline constexpr uint64_t K0 = 0xc3a5c85c97cb3127ULL;
        inline constexpr uint64_t K1 = 0xb492b66fbe98f273ULL;
        inline constexpr uint64_t K2 = 0x9ae16a3b2f90404fULL;
        inline constexpr uint64_t KMUL = 0x9ddfea08eb382d69ULL;

        struct Uint64Pair
        {
            uint64_t First;
            uint64_t Second;
        };

        constexpr uint32_t Fetch32(const char* data) noexcept
        {
            return static_cast<uint32_t>(static_cast<uint8_t>(data[0])) | (static_cast<uint32_t>(static_cast<uint8_t>(data[1])) << 8) |
                   (static_cast<uint32_t>(static_cast<uint8_t>(data[2])) << 16) | (static_cast<uint32_t>(static_cast<uint8_t>(data[3])) << 24);
        }

        constexpr uint64_t Fetch64(const char* data) noexcept
        {
            return static_cast<uint64_t>(Fetch32(data)) | (static_cast<uint64_t>(Fetch32(data + 4)) << 32);
        }

        constexpr uint64_t Rotate(uint64_t value, uint32_t shift) noexcept
        {
            return shift == 0 ? value : ((value >> shift) | (value << (64 - shift)));
        }

        constexpr uint64_t ShiftMix(uint64_t value) noexcept { return value ^ (value >> 47); }

        constexpr uint64_t ByteSwap64(uint64_t value) noexcept
        {
            value = ((value & 0x00ff00ff00ff00ffULL) << 8) | ((value >> 8) & 0x00ff00ff00ff00ffULL);
            value = ((value & 0x0000ffff0000ffffULL) << 16) | ((value >> 16) & 0x0000ffff0000ffffULL);
            return (value << 32) | (value >> 32);
        }

        constexpr uint64_t HashLen16(uint64_t first, uint64_t second, uint64_t multiplier = KMUL) noexcept
        {
            uint64_t a = (first ^ second) * multiplier;
            a ^= a >> 47;
            uint64_t b = (second ^ a) * multiplier;
            b ^= b >> 47;
            return b * multiplier;
        }

        constexpr uint64_t HashLen0To16(const char* data, size_t size) noexcept
        {
            if (size >= 8)
            {
                const uint64_t multiplier = K2 + size * 2;
                const uint64_t a = Fetch64(data) + K2;
                const uint64_t b = Fetch64(data + size - 8);
                const uint64_t c = Rotate(b, 37) * multiplier + a;
                const uint64_t d = (Rotate(a, 25) + b) * multiplier;
                return HashLen16(c, d, multiplier);
            }
            if (size >= 4)
            {
                const uint64_t multiplier = K2 + size * 2;
                const uint64_t a = Fetch32(data);
                return HashLen16(size + (a << 3), Fetch32(data + size - 4), multiplier);
            }
            if (size > 0)
            {
                const uint8_t a = static_cast<uint8_t>(data[0]);
                const uint8_t b = static_cast<uint8_t>(data[size >> 1]);
                const uint8_t c = static_cast<uint8_t>(data[size - 1]);
                const uint32_t y = static_cast<uint32_t>(a) + (static_cast<uint32_t>(b) << 8);
                const uint32_t z = static_cast<uint32_t>(size) + (static_cast<uint32_t>(c) << 2);
                return ShiftMix(y * K2 ^ z * K0) * K2;
            }
            return K2;
        }

        constexpr uint64_t HashLen17To32(const char* data, size_t size) noexcept
        {
            const uint64_t multiplier = K2 + size * 2;
            const uint64_t a = Fetch64(data) * K1;
            const uint64_t b = Fetch64(data + 8);
            const uint64_t c = Fetch64(data + size - 8) * multiplier;
            const uint64_t d = Fetch64(data + size - 16) * K2;
            return HashLen16(Rotate(a + b, 43) + Rotate(c, 30) + d, a + Rotate(b + K2, 18) + c, multiplier);
        }

        constexpr Uint64Pair WeakHashLen32WithSeeds(uint64_t w, uint64_t x, uint64_t y, uint64_t z, uint64_t a, uint64_t b) noexcept
        {
            a += w;
            b = Rotate(b + a + z, 21);
            const uint64_t c = a;
            a += x;
            a += y;
            b += Rotate(a, 44);
            return { a + z, b + c };
        }

        constexpr Uint64Pair WeakHashLen32WithSeeds(const char* data, uint64_t a, uint64_t b) noexcept
        {
            return WeakHashLen32WithSeeds(Fetch64(data), Fetch64(data + 8), Fetch64(data + 16), Fetch64(data + 24), a, b);
        }

        constexpr uint64_t HashLen33To64(const char* data, size_t size) noexcept
        {
            const uint64_t multiplier = K2 + size * 2;
            uint64_t a = Fetch64(data) * K2;
            uint64_t b = Fetch64(data + 8);
            const uint64_t c = Fetch64(data + size - 24);
            const uint64_t d = Fetch64(data + size - 32);
            const uint64_t e = Fetch64(data + 16) * K2;
            const uint64_t f = Fetch64(data + 24) * 9;
            const uint64_t g = Fetch64(data + size - 8);
            const uint64_t h = Fetch64(data + size - 16) * multiplier;
            const uint64_t u = Rotate(a + g, 43) + (Rotate(b, 30) + c) * 9;
            const uint64_t v = ((a + g) ^ d) + f + 1;
            const uint64_t w = ByteSwap64((u + v) * multiplier) + h;
            const uint64_t x = Rotate(e + f, 42) + c;
            const uint64_t y = (ByteSwap64((v + w) * multiplier) + g) * multiplier;
            const uint64_t z = e + f + c;
            a = ByteSwap64((x + z) * multiplier + y) + b;
            b = ShiftMix((z + a) * multiplier + d + h) * multiplier;
            return b + x;
        }
    } // namespace Detail

    /** Stable, non-cryptographic 64-bit hash for byte strings. */
    constexpr uint64_t CityHash64(const char* data, size_t size) noexcept
    {
        using namespace Detail;
        if (size <= 32)
            return size <= 16 ? HashLen0To16(data, size) : HashLen17To32(data, size);
        if (size <= 64)
            return HashLen33To64(data, size);

        uint64_t x = Fetch64(data + size - 40);
        uint64_t y = Fetch64(data + size - 16) + Fetch64(data + size - 56);
        uint64_t z = HashLen16(Fetch64(data + size - 48) + size, Fetch64(data + size - 24));
        Uint64Pair v = WeakHashLen32WithSeeds(data + size - 64, size, z);
        Uint64Pair w = WeakHashLen32WithSeeds(data + size - 32, y + K1, x);
        x = x * K1 + Fetch64(data);

        size = (size - 1) & ~static_cast<size_t>(63);
        do
        {
            x = Rotate(x + y + v.First + Fetch64(data + 8), 37) * K1;
            y = Rotate(y + v.Second + Fetch64(data + 48), 42) * K1;
            x ^= w.Second;
            y += v.First + Fetch64(data + 40);
            z = Rotate(z + w.First, 33) * K1;
            v = WeakHashLen32WithSeeds(data, v.Second * K1, x + w.First);
            w = WeakHashLen32WithSeeds(data + 32, z + w.Second, y + Fetch64(data + 16));
            const uint64_t swap = z;
            z = x;
            x = swap;
            data += 64;
            size -= 64;
        } while (size != 0);

        return HashLen16(HashLen16(v.First, w.First) + ShiftMix(y) * K1 + z, HashLen16(v.Second, w.Second) + x);
    }

    constexpr uint64_t CityHash64(std::string_view value) noexcept { return CityHash64(value.data(), value.size()); }

    constexpr uint64_t CityHash64WithSeeds(const char* data, size_t size, uint64_t seed0, uint64_t seed1) noexcept
    {
        return Detail::HashLen16(CityHash64(data, size) - seed0, seed1);
    }

    constexpr uint64_t CityHash64WithSeed(const char* data, size_t size, uint64_t seed) noexcept
    {
        return CityHash64WithSeeds(data, size, Detail::K2, seed);
    }

    /** Uses CityHash for byte strings and std::hash for other key types. */
    template <typename T> struct Hasher : std::hash<T>
    {
    };

    template <> struct Hasher<std::string>
    {
        using is_transparent = void;

        constexpr size_t operator()(std::string_view value) const noexcept { return static_cast<size_t>(CityHash64(value)); }
        constexpr size_t operator()(const std::string& value) const noexcept { return (*this)(std::string_view(value)); }
        constexpr size_t operator()(const char* value) const noexcept { return (*this)(std::string_view(value)); }
    };

    template <> struct Hasher<std::string_view>
    {
        using is_transparent = void;

        constexpr size_t operator()(std::string_view value) const noexcept { return static_cast<size_t>(CityHash64(value)); }
        constexpr size_t operator()(const std::string& value) const noexcept { return (*this)(std::string_view(value)); }
        constexpr size_t operator()(const char* value) const noexcept { return (*this)(std::string_view(value)); }
    };
} // namespace Crowny::Hashing
