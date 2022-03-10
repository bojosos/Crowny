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

    struct BinaryShaderData
    {
        Vector<uint8_t> Data;
        String EntryPoint;
        ShaderType Type = ShaderType::VERTEX_SHADER;
        Ref<UniformDesc> Description;

        BinaryShaderData() = default;
        BinaryShaderData(const Vector<uint8_t>& data, const String& entryPoint, ShaderType type,
                         const Ref<UniformDesc>& uniformDesc)
          : Data(data), EntryPoint(entryPoint), Type(type), Description(uniformDesc)
        {
        }
    };

    class ShaderCompiler
    {
    public:
        static ShaderDesc Compile(const String& source, ShaderLanguageFlags language = ShaderLanguage::VKSL);
        static Ref<BinaryShaderData> CompileStage(const String& source, ShaderType shaderType, ShaderLanguage language,
                                                  ShaderLanguageFlags flags);

    private:
        static Ref<UniformDesc> GetUniformDesc(const Vector<uint8_t>& binaryShaderData);
        static UnorderedMap<ShaderType, String> Parse(const String& streamData);
    };

} // namespace Crowny
