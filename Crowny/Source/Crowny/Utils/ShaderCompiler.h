#pragma once

#include "Crowny/Common/Types.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Utils/ShaderSourceParser.h"

namespace Crowny
{

    enum class ShaderLanguage
    {
        VKSL = 1 << 0,
        GLSL = 1 << 1,
        HLSL = 1 << 2,
        MSL = 1 << 3,

        ALL = VKSL | GLSL | HLSL | MSL
    };
    typedef Flags<ShaderLanguage> ShaderLanguageFlags;
    CW_FLAGS_OPERATORS(ShaderLanguageFlags);

    struct ShaderCompileResult
    {
        ShaderDesc Description;
        Vector<ShaderDiagnostic> Diagnostics;

        bool Succeeded() const;
    };

    struct ShaderCompilerCacheStats
    {
        uint64_t Hits = 0;
        uint64_t Misses = 0;
        size_t Entries = 0;
    };

    class ShaderCompiler
    {
    public:
        static ShaderDesc Compile(const Path& path, const String& source, ShaderLanguageFlags language = ShaderLanguage::VKSL,
                                  const UnorderedMap<String, String>& defines = {});
        static ShaderCompileResult CompileWithDiagnostics(const Path& path, const String& source,
                                                          ShaderLanguageFlags language = ShaderLanguage::VKSL,
                                                          const UnorderedMap<String, String>& defines = {});
        static bool PreprocessIncludes(const Path& path, StringView source, String& output,
                                       Vector<ShaderDiagnostic>& diagnostics);
        static Ref<BlendStateDesc> PreparseBlendState(String& inOutShader);
        static Ref<BinaryShaderData> CompileStage(const String& source, ShaderType shaderType, ShaderLanguage language, ShaderLanguageFlags flags,
                                                  const UnorderedMap<String, String>& defines);
        static void ClearCache();
        static ShaderCompilerCacheStats GetCacheStats();
        static uint64_t HashSource(StringView source);

    private:
        static Ref<BlendStateDesc> ParseBlendState(const Path& path, String& inOutShader, Vector<ShaderDiagnostic>& diagnostics);
        static Ref<BinaryShaderData> CompileStage(const Path& path, const String& source, ShaderType shaderType, ShaderLanguage language,
                                                  ShaderLanguageFlags flags, const UnorderedMap<String, String>& defines,
                                                  Vector<ShaderDiagnostic>& diagnostics);
        static void Reflect(const Vector<uint8_t>& binaryShaderData, Ref<BinaryShaderData>& outData);
        static void ParseAnnotations(const String& source, Ref<UniformDesc>& uniformDesc);
        static void EvaluatePragmaDirectives(const Vector<ShaderPragma>& globalPragmas, const Vector<ShaderPragma>& passPragmas,
                                             ShaderRenderPassDesc& shaderPassDesc, Vector<ShaderDiagnostic>& diagnostics, const Path& path);
        static Vector<Ref<ShaderRenderPass>> CompilePasses(const Path& path, const ParsedShaderSource& parsedSource,
                                                           ShaderLanguage inputLanguage, ShaderLanguageFlags shaderLanguage,
                                                           const UnorderedMap<String, String>& defines,
                                                           const Ref<BlendStateDesc>& blendState, Vector<ShaderDiagnostic>& diagnostics);
    };

} // namespace Crowny
