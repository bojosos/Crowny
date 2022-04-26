#include "cwpch.h"

#include "Crowny/Utils/Compression.h"
#include "Vendor/fastlz/fastlz.h"

namespace Crowny
{
    uint64_t Compression::Compress(uint8_t* dest, const uint8_t* src, uint64_t size, CompressionMethod method)
    {
        switch (method)
        {
        case CompressionMethod::FastLZ: {
            if (size < 32)
            {
                std::memcpy(dest, src, size);
                return size;
            }
            return fastlz_compress(src, size, dest);
        }
        default:
            CW_ENGINE_ERROR("Unsupported compression method.");
        }
    }

    uint64_t Compression::Decompress(uint8_t* dest, int maxDestSize, const uint8_t* src, int srcSize,
                                     CompressionMethod method)
    {
        switch (method)
        {
        case CompressionMethod::FastLZ: {
            uint64_t ret = 0;
            if (maxDestSize < 32)
            {
                std::memcpy(dest, src, srcSize);
                ret = maxDestSize;
            }
            else
                ret = fastlz_decompress(src, srcSize, dest, maxDestSize);
            return ret;
        }
        case CompressionMethod::Deflate:
            break;
        case CompressionMethod::Zstd:
            break;
        default:
            break;
        }
        return -1;
    }

} // namespace Crowny