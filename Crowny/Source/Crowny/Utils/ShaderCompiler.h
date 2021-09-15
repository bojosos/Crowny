#pragma once

#include "Crowny/Common/Types.h"
#include "Crowny/RenderAPI/Shader.h"

namespace Crowny
{

    enum class ShaderInputLanguage
    {
        HLSL,
        GLSL
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
        void* Data; // TODO: use blob or vector
        size_t Size;
        std::string EntryPoint;
        ShaderType Type;
        Ref<UniformDesc> Description;
    };

    class ShaderCompiler
    {
    public:
        ShaderCompiler(ShaderInputLanguage inputLanguage = ShaderInputLanguage::GLSL,
                       ShaderOutputFormat outputFormat = ShaderOutputFormat::Vulkan);
        BinaryShaderData Compile(const std::string& filepath, ShaderType shaderType);

    private:
        ShaderInputLanguage m_InputLanguage;
        ShaderOutputFormat m_OutputFormat;
    };

} // namespace Crowny
