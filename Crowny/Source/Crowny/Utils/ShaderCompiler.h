#pragma once

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
        void* Data;
        size_t Size;
        Crowny::ShaderType ShaderType;
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