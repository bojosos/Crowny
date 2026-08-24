#pragma once

#include "Crowny/Common/DataStream.h"

#include <limits>

namespace Crowny
{

    enum class CompressionMethod
    {
        // Really fast compression, however, compression ratio will be slightly lower
        FastLZ,
        Deflate,
        Zstd
    };

    enum class FastLZLevel
    {
        Fastest = 1,
        BetterRatio = 2
    };

    class Compression
    {
    public:
        static constexpr uint64_t Error = std::numeric_limits<uint64_t>::max();

        static uint64_t CompressBound(uint64_t srcSize, CompressionMethod method = CompressionMethod::FastLZ);
        // Compatibility overload. The caller must provide at least CompressBound(srcSize) bytes in dest.
        static uint64_t Compress(uint8_t* dest, const uint8_t* src, uint64_t srcSize, CompressionMethod method = CompressionMethod::FastLZ);
        static uint64_t Compress(uint8_t* dest, uint64_t destSize, const uint8_t* src, uint64_t srcSize,
                                 CompressionMethod method = CompressionMethod::FastLZ, FastLZLevel level = FastLZLevel::Fastest);
        static uint64_t Decompress(uint8_t* dest, uint64_t maxDestSize, const uint8_t* src, uint64_t srcSize,
                                   CompressionMethod method = CompressionMethod::FastLZ);

        static bool Compress(Vector<uint8_t>& dest, const Vector<uint8_t>& src, CompressionMethod method = CompressionMethod::FastLZ,
                             FastLZLevel level = FastLZLevel::Fastest);
        static bool Decompress(Vector<uint8_t>& dest, const Vector<uint8_t>& src, uint64_t decompressedSize,
                               CompressionMethod method = CompressionMethod::FastLZ);

        // Stream compression uses a chunked, self-describing Crowny frame.
        static bool Compress(const Ref<DataStream>& src, const Ref<DataStream>& dest, CompressionMethod method = CompressionMethod::FastLZ,
                             FastLZLevel level = FastLZLevel::Fastest);
        static bool Decompress(const Ref<DataStream>& src, const Ref<DataStream>& dest);
        static Ref<MemoryDataStream> Compress(const Ref<DataStream>& src, CompressionMethod method = CompressionMethod::FastLZ,
                                              FastLZLevel level = FastLZLevel::Fastest);
        static Ref<MemoryDataStream> Decompress(const Ref<DataStream>& src);

    private:
    };
} // namespace Crowny
