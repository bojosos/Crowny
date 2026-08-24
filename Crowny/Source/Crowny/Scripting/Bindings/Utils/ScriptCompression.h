#pragma once

#include "Crowny/Scripting/ScriptObject.h"

#include "Crowny/Utils/Compression.h"

namespace Crowny
{

    class ScriptCompression : public ScriptObject<ScriptCompression>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Compression")
        ScriptCompression();

    private:
        static uint64_t Internal_Compress(MonoArray* dst, MonoArray* src, CompressionMethod method, FastLZLevel level);
        static uint64_t Internal_Decompress(MonoArray* dst, int32_t maxDstSize, MonoArray* src, int32_t srcSize, CompressionMethod method);
    };
} // namespace Crowny
