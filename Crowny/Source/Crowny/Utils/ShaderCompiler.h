#pragma once

#include "Crowny/Common/Types.h"
#include "Crowny/RenderAPI/Shader.h"

namespace Crowny
{

    enum class ShaderInputLanguage
    {
        VKSL = 0,
        GLSL = 1,
        HLSL = 2,
        MSL = 3
    };

    enum class ShaderOutputFormat
    {
        OpenGL,
        Vulkan,
        Metal,
        D3D
    };

    struct BinaryShaderData
    {
        std::vector<uint8_t> Data;
        String EntryPoint;
        ShaderType Type;
        Ref<UniformDesc> Description;
    };

    class ShaderCompiler
    {
    public:
        static Ref<BinaryShaderData> Compile(const Path& filepath, ShaderType shaderType,
                                             ShaderInputLanguage inputLanguage = ShaderInputLanguage::GLSL,
                                             ShaderOutputFormat outputFormat = ShaderOutputFormat::Vulkan);

    private:
        static Ref<UniformDesc> GetUniformDesc(const Vector<uint8_t>& binaryShaderData);
        static UnorderedMap<ShaderType, String> Parse(const String& streamData);
    };

} // namespace Crowny
