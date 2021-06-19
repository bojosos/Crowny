#pragma once

#include "Crowny/Renderer/Shader.h"

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
        Crowny::ShaderType ShaderType;
        UniformDescription Description;
    };

    class ShaderCompiler
    {
    public:
        ShaderCompiler(ShaderInputLanguage inputLanguage = ShaderInputLanguage::GLSL, ShaderOutputFormat outputFormat = ShaderOutputFormat::Vulkan);
        BinaryShaderData Compile(const std::string& filepath, ShaderType shaderType);

    private:
        ShaderInputLanguage m_InputLanguage;
        ShaderOutputFormat m_OutputFormat;
    };
    
}