#pragma once

#include "Crowny/Common/Types.h"
#include "Crowny/RenderAPI/Shader.h"

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

    class ShaderCompiler
    {
    public:
        static ShaderDesc Compile(const Path& path, const String& source, ShaderLanguageFlags language = ShaderLanguage::VKSL,
                                  const UnorderedMap<String, String>& defines = {});
        static Ref<BlendStateDesc> PreparseBlendState(String& inOutShader);
        static Ref<BinaryShaderData> CompileStage(const String& source, ShaderType shaderType, ShaderLanguage language, ShaderLanguageFlags flags,
                                                  const UnorderedMap<String, String>& defines);

    private:
        static void Reflect(const Vector<uint8_t>& binaryShaderData, Ref<BinaryShaderData>& outData);
        static void ParseAnnotations(const String& source, Ref<UniformDesc>& uniformDesc);
        static void EvaluatePragmaDirectives(const String& source, ShaderRenderPassDesc& shaderPassDesc);
        static Vector<UnorderedMap<ShaderType, String>> Parse(const String& streamData);
        static Vector<Ref<ShaderRenderPass>> CompilePasses(const Vector<UnorderedMap<ShaderType, String>>& parsedPasses,
                                                           ShaderLanguage inputLanguage, ShaderLanguageFlags shaderLanguage,
                                                           const UnorderedMap<String, String>& defines,
                                                           const Ref<BlendStateDesc>& blendState);
    };

} // namespace Crowny
