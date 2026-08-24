#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Utils/ScriptCompression.h"

#include <mono/metadata/object.h>

namespace Crowny
{
    ScriptCompression::ScriptCompression() : ScriptObject() {}

    void ScriptCompression::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_Compress", (void*)&ScriptCompression::Internal_Compress);
        MetaData.ScriptClass->AddInternalCall("Internal_Decompress", (void*)&ScriptCompression::Internal_Decompress);
    }

    uint64_t ScriptCompression::Internal_Compress(MonoArray* dst, MonoArray* src, CompressionMethod method, FastLZLevel level)
    {
        if (dst == nullptr || src == nullptr)
            return Compression::Error;

        const uint64_t srcSize = mono_array_length(src);
        const uint64_t dstSize = mono_array_length(dst);
        uint8_t* const rawSrc = srcSize > 0 ? mono_array_addr(src, uint8_t, 0) : nullptr;
        uint8_t* const rawDst = dstSize > 0 ? mono_array_addr(dst, uint8_t, 0) : nullptr;
        return Compression::Compress(rawDst, dstSize, rawSrc, srcSize, method, level);
    }

    uint64_t ScriptCompression::Internal_Decompress(MonoArray* dst, int32_t maxDstSize, MonoArray* src, int32_t srcSize, CompressionMethod method)
    {
        if (dst == nullptr || src == nullptr || maxDstSize < 0 || srcSize < 0 || static_cast<uint64_t>(maxDstSize) > mono_array_length(dst) ||
            static_cast<uint64_t>(srcSize) > mono_array_length(src))
            return Compression::Error;

        uint8_t* const rawSrc = srcSize > 0 ? mono_array_addr(src, uint8_t, 0) : nullptr;
        uint8_t* const rawDst = maxDstSize > 0 ? mono_array_addr(dst, uint8_t, 0) : nullptr;
        return Compression::Decompress(rawDst, static_cast<uint64_t>(maxDstSize), rawSrc, static_cast<uint64_t>(srcSize), method);
    }

} // namespace Crowny
