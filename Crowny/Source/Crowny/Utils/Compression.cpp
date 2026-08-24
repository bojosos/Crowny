#include "cwpch.h"

#include "Crowny/Utils/Compression.h"
#include "Vendor/fastlz/fastlz.h"

namespace Crowny
{
    namespace
    {
        constexpr uint64_t FastLZMinInputSize = 16;
        constexpr uint32_t StreamChunkSize = 64 * 1024;
        constexpr uint8_t StreamVersion = 1;
        constexpr uint8_t StreamBlockCompressed = 1;
        constexpr uint8_t StreamMagic[] = { 'C', 'W', 'F', 'Z' };

        bool IsValidLevel(FastLZLevel level) { return level == FastLZLevel::Fastest || level == FastLZLevel::BetterRatio; }

        bool WriteExact(const Ref<DataStream>& stream, const void* data, size_t size) { return stream->Write(data, size) == size; }

        bool ReadExact(const Ref<DataStream>& stream, void* data, size_t size) { return stream->Read(data, size) == size; }

        void EncodeU32(uint8_t* dest, uint32_t value)
        {
            dest[0] = static_cast<uint8_t>(value);
            dest[1] = static_cast<uint8_t>(value >> 8);
            dest[2] = static_cast<uint8_t>(value >> 16);
            dest[3] = static_cast<uint8_t>(value >> 24);
        }

        uint32_t DecodeU32(const uint8_t* src)
        {
            return static_cast<uint32_t>(src[0]) | (static_cast<uint32_t>(src[1]) << 8) | (static_cast<uint32_t>(src[2]) << 16) |
                   (static_cast<uint32_t>(src[3]) << 24);
        }
    } // namespace

    uint64_t Compression::CompressBound(uint64_t srcSize, CompressionMethod method)
    {
        if (method != CompressionMethod::FastLZ || srcSize > static_cast<uint64_t>(std::numeric_limits<int>::max()))
            return Error;
        if (srcSize < FastLZMinInputSize)
            return srcSize;

        const uint64_t overhead = (srcSize + 19) / 20;
        if (srcSize > std::numeric_limits<uint64_t>::max() - overhead)
            return Error;
        return std::max<uint64_t>(66, srcSize + overhead);
    }

    uint64_t Compression::Compress(uint8_t* dest, uint64_t destSize, const uint8_t* src, uint64_t srcSize, CompressionMethod method,
                                   FastLZLevel level)
    {
        const uint64_t bound = CompressBound(srcSize, method);
        if (bound == Error || !IsValidLevel(level) || destSize < bound)
            return Error;
        if (srcSize == 0)
            return 0;
        if (dest == nullptr || src == nullptr)
            return Error;
        if (srcSize < FastLZMinInputSize)
        {
            std::memcpy(dest, src, static_cast<size_t>(srcSize));
            return srcSize;
        }

        const int result = fastlz_compress_level(static_cast<int>(level), src, static_cast<int>(srcSize), dest);
        return result > 0 ? static_cast<uint64_t>(result) : Error;
    }

    uint64_t Compression::Compress(uint8_t* dest, const uint8_t* src, uint64_t srcSize, CompressionMethod method)
    {
        const uint64_t bound = CompressBound(srcSize, method);
        return bound == Error ? Error : Compress(dest, bound, src, srcSize, method);
    }

    uint64_t Compression::Decompress(uint8_t* dest, uint64_t maxDestSize, const uint8_t* src, uint64_t srcSize, CompressionMethod method)
    {
        if (method != CompressionMethod::FastLZ || maxDestSize > static_cast<uint64_t>(std::numeric_limits<int>::max()) ||
            srcSize > static_cast<uint64_t>(std::numeric_limits<int>::max()))
            return Error;
        if (maxDestSize == 0)
            return srcSize == 0 ? 0 : Error;
        if (dest == nullptr || src == nullptr)
            return Error;
        if (maxDestSize < FastLZMinInputSize)
        {
            if (srcSize != maxDestSize)
                return Error;
            std::memcpy(dest, src, static_cast<size_t>(srcSize));
            return srcSize;
        }

        const int result = fastlz_decompress(src, static_cast<int>(srcSize), dest, static_cast<int>(maxDestSize));
        return result > 0 ? static_cast<uint64_t>(result) : Error;
    }

    bool Compression::Compress(Vector<uint8_t>& dest, const Vector<uint8_t>& src, CompressionMethod method, FastLZLevel level)
    {
        const uint64_t bound = CompressBound(src.size(), method);
        if (bound == Error)
            return false;

        dest.resize(static_cast<size_t>(bound));
        const uint64_t size = Compress(dest.data(), dest.size(), src.data(), src.size(), method, level);
        if (size == Error)
        {
            dest.clear();
            return false;
        }

        dest.resize(static_cast<size_t>(size));
        return true;
    }

    bool Compression::Decompress(Vector<uint8_t>& dest, const Vector<uint8_t>& src, uint64_t decompressedSize, CompressionMethod method)
    {
        if (decompressedSize > static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
            return false;

        dest.resize(static_cast<size_t>(decompressedSize));
        const uint64_t size = Decompress(dest.data(), dest.size(), src.data(), src.size(), method);
        if (size != decompressedSize)
        {
            dest.clear();
            return false;
        }

        return true;
    }

    bool Compression::Compress(const Ref<DataStream>& src, const Ref<DataStream>& dest, CompressionMethod method, FastLZLevel level)
    {
        if (!src || !dest || !src->IsReadable() || !dest->IsWritable() || method != CompressionMethod::FastLZ || !IsValidLevel(level))
            return false;

        const uint8_t header[] = { StreamMagic[0],
                                   StreamMagic[1],
                                   StreamMagic[2],
                                   StreamMagic[3],
                                   StreamVersion,
                                   static_cast<uint8_t>(method),
                                   static_cast<uint8_t>(level),
                                   0 };
        if (!WriteExact(dest, header, sizeof(header)))
            return false;

        Vector<uint8_t> input(StreamChunkSize);
        Vector<uint8_t> compressed;
        while (!src->Eof())
        {
            const size_t read = src->Read(input.data(), input.size());
            if (read == 0)
                return false;

            input.resize(read);
            const bool compressionSucceeded = Compress(compressed, input, method, level);
            const bool useCompressed = compressionSucceeded && compressed.size() < input.size();
            const Vector<uint8_t>& payload = useCompressed ? compressed : input;

            uint8_t blockHeader[9];
            EncodeU32(blockHeader, static_cast<uint32_t>(input.size()));
            EncodeU32(blockHeader + 4, static_cast<uint32_t>(payload.size()));
            blockHeader[8] = useCompressed ? StreamBlockCompressed : 0;
            if (!WriteExact(dest, blockHeader, sizeof(blockHeader)) || !WriteExact(dest, payload.data(), payload.size()))
                return false;

            input.resize(StreamChunkSize);
        }

        const uint8_t terminator[9] = {};
        return WriteExact(dest, terminator, sizeof(terminator));
    }

    bool Compression::Decompress(const Ref<DataStream>& src, const Ref<DataStream>& dest)
    {
        if (!src || !dest || !src->IsReadable() || !dest->IsWritable())
            return false;

        uint8_t header[8];
        if (!ReadExact(src, header, sizeof(header)) || !std::equal(std::begin(StreamMagic), std::end(StreamMagic), header) ||
            header[4] != StreamVersion || header[5] != static_cast<uint8_t>(CompressionMethod::FastLZ) ||
            !IsValidLevel(static_cast<FastLZLevel>(header[6])) || header[7] != 0)
            return false;

        Vector<uint8_t> payload;
        Vector<uint8_t> decompressed;
        while (true)
        {
            uint8_t blockHeader[9];
            if (!ReadExact(src, blockHeader, sizeof(blockHeader)))
                return false;

            const uint32_t decompressedSize = DecodeU32(blockHeader);
            const uint32_t storedSize = DecodeU32(blockHeader + 4);
            const uint8_t flags = blockHeader[8];
            if (decompressedSize == 0)
                return storedSize == 0 && flags == 0;
            if (decompressedSize > StreamChunkSize || storedSize == 0 || flags > StreamBlockCompressed)
                return false;
            if ((flags & StreamBlockCompressed) != 0 && storedSize > CompressBound(decompressedSize, CompressionMethod::FastLZ))
                return false;
            if ((flags & StreamBlockCompressed) == 0 && storedSize != decompressedSize)
                return false;

            payload.resize(storedSize);
            if (!ReadExact(src, payload.data(), payload.size()))
                return false;

            if ((flags & StreamBlockCompressed) != 0)
            {
                if (!Decompress(decompressed, payload, decompressedSize, CompressionMethod::FastLZ))
                    return false;
                if (!WriteExact(dest, decompressed.data(), decompressed.size()))
                    return false;
            }
            else
            {
                if (!WriteExact(dest, payload.data(), payload.size()))
                    return false;
            }
        }
    }

    Ref<MemoryDataStream> Compression::Compress(const Ref<DataStream>& src, CompressionMethod method, FastLZLevel level)
    {
        Ref<MemoryDataStream> result = CreateRef<MemoryDataStream>();
        if (!Compress(src, result, method, level))
            return nullptr;
        result->Seek(0);
        return result;
    }

    Ref<MemoryDataStream> Compression::Decompress(const Ref<DataStream>& src)
    {
        Ref<MemoryDataStream> result = CreateRef<MemoryDataStream>();
        if (!Decompress(src, result))
            return nullptr;
        result->Seek(0);
        return result;
    }
} // namespace Crowny
