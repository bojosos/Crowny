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

    uint64_t ScriptCompression::Internal_Compress(MonoArray* dst, MonoArray* src, CompressionMethod method)
    {
        uint8_t* const rawSrc = mono_array_addr(src, uint8_t, 0);
        uint8_t* const rawDst = mono_array_addr(dst, uint8_t, 0);
        const uint64_t srcSize = mono_array_length(src);
        return Compression::Compress(rawDst, rawSrc, srcSize, method);
    }

    uint64_t ScriptCompression::Internal_Decompress(MonoArray* dst, int32_t maxDstSize, MonoArray* src, int32_t srcSize, CompressionMethod method)
    {
        uint8_t* const rawSrc = mono_array_addr(src, uint8_t, 0);
        uint8_t* const rawDst = mono_array_addr(dst, uint8_t, 0);
        return Compression::Decompress(rawDst, maxDstSize, rawSrc, srcSize, method);
    }

} // namespace Crowny