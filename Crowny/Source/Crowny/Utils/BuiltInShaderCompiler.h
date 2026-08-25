#pragma once

#include "Crowny/Common/Common.h"

namespace Crowny
{
    struct BuiltInShaderCompileStats
    {
        uint32_t Compiled = 0;
        uint32_t Skipped = 0;
        uint32_t Failed = 0;
    };

    class BuiltInShaderCompiler
    {
    public:
        static constexpr const char* SHADER_SOURCE_DIR = "Resources/Shaders";

        // Scans all .glsl files and compiles sources whose root or transitive
        // include content differs from the saved asset.
        static void CompileAll();
        static BuiltInShaderCompileStats CompileAll(const Path& sourceDirectory);

    private:
        static bool NeedsRecompile(const Path& assetPath, uint64_t sourceContentHash);
        static bool CompileAndSave(const Path& glslPath, const Path& assetPath, const String& source);
    };
} // namespace Crowny
