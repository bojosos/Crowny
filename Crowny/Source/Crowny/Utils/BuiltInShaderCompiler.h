#pragma once

#include "Crowny/Common/Common.h"

namespace Crowny
{

    class BuiltInShaderCompiler
    {
    public:
        static constexpr const char* SHADER_SOURCE_DIR = "Resources/Shaders";

        // Scans all .glsl files in SHADER_SOURCE_DIR.
        // Compiles any that are missing a .asset or whose .glsl is newer than the .asset.
        // Should be called once at editor startup, before ForwardRenderer::Init().
        static void CompileAll();

    private:
        static bool NeedsRecompile(const Path& glslPath, const Path& assetPath);
        static bool CompileAndSave(const Path& glslPath, const Path& assetPath);
    };

} // namespace Crowny
