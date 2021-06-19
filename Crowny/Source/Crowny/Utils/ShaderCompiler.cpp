#include "cwpch.h"

#include "Crowny/Utils/ShaderCompiler.h"
#include "Crowny/Renderer/Shader.h"
#include "Crowny/Common/VirtualFileSystem.h"

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include <filesystem>

namespace Crowny
{

    static std::filesystem::path CachePath = "/Cache";
    
    static shaderc_shader_kind ShaderTypeToShaderC(ShaderType shaderType)
    {
        switch(shaderType)
        {
            case VERTEX_SHADER:   return shaderc_glsl_vertex_shader;
            case FRAGMENT_SHADER: return shaderc_glsl_fragment_shader;
            case GEOMETRY_SHADER: return shaderc_glsl_geometry_shader;
            case DOMAIN_SHADER:   return shaderc_glsl_tess_control_shader;
            case HULL_SHADER:     return shaderc_glsl_tess_evaluation_shader;
            case COMPUTE_SHADER:  return shaderc_glsl_compute_shader;
        }
        
        return shaderc_glsl_vertex_shader;
    }
    
    static std::string ShaderFormatToExtension(ShaderOutputFormat shaderFormat)
    {
        switch(shaderFormat)
        {
            case (ShaderOutputFormat::Vulkan): return "vk";
            case (ShaderOutputFormat::OpenGL): return "gl";
            case (ShaderOutputFormat::Metal):  return "msl";
            case (ShaderOutputFormat::D3D): return "d3d";
        }
        
        return std::string();
    }
    
    static std::string ShaderTypeToString(ShaderType shaderType)
    {
        switch(shaderType)
        {
            case VERTEX_SHADER:   return "vertex";
            case FRAGMENT_SHADER: return "fragment";
            case GEOMETRY_SHADER: return "geometry";
            case HULL_SHADER:     return "hull";
            case DOMAIN_SHADER:   return "domain";
            case COMPUTE_SHADER:  return "compute";
        }
        
        return std::string();
    }
    
    static std::string ShaderTypeToPath(ShaderType shaderType, const std::string& filename, ShaderOutputFormat outputFormat)
    {
        return CachePath / (filename + "." + ShaderTypeToString(shaderType) + "." + ShaderFormatToExtension(outputFormat));
    }
    
    ShaderCompiler::ShaderCompiler(ShaderInputLanguage inputLanguage, ShaderOutputFormat outputFormat)
        : m_InputLanguage(inputLanguage), m_OutputFormat(outputFormat)
    {
        
    }

    static std::vector<uint32_t> dat{}; // sorry

    BinaryShaderData ShaderCompiler::Compile(const std::string& filepath, ShaderType shaderType)
    {
        std::string sourcePath;
        VirtualFileSystem::Get()->ResolvePhyiscalPath(filepath, sourcePath);
        std::string shaderFilename = std::filesystem::path(sourcePath).filename();
        std::string shaderPath = ShaderTypeToPath(shaderType, shaderFilename, m_OutputFormat);
        auto [data, size] = VirtualFileSystem::Get()->ReadFile(ShaderTypeToPath(shaderType, shaderFilename, m_OutputFormat));
        if (data)
            return { data, size, shaderType };
        else
        {
            shaderc::Compiler compiler;
            shaderc::CompileOptions options;
            switch(m_InputLanguage)
            {
                case (ShaderInputLanguage::GLSL): options.SetSourceLanguage(shaderc_source_language_glsl); break;
                case (ShaderInputLanguage::HLSL): options.SetSourceLanguage(shaderc_source_language_hlsl); break;
            }
            options.SetSourceLanguage(shaderc_source_language_glsl);
            options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
            options.SetOptimizationLevel(shaderc_optimization_level_performance);
            
            std::string source = VirtualFileSystem::Get()->ReadTextFile(sourcePath);
            shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source.c_str(), source.size(), ShaderTypeToShaderC(shaderType), shaderFilename.c_str(), options);
            if (module.GetCompilationStatus() != shaderc_compilation_status_success)
            {
                CW_ENGINE_ERROR("Shader compilation error: {1}", module.GetErrorMessage());
                CW_ENGINE_ASSERT(false);
                return { nullptr, 0 };
            }
            dat = std::vector<uint32_t>(module.cbegin(), module.cend());
            VirtualFileSystem::Get()->WriteFile(shaderPath, (byte*)dat.data(), dat.size() * sizeof(uint32_t));
            
            spirv_cross::Compiler rfl(dat);
            spirv_cross::ShaderResources resources = rfl.get_shader_resources();
            for (const auto& buffer : resources.uniform_buffers)
            {
                const auto& bufferType = rfl.get_type(buffer.base_type_id);
                uint32_t bufferSize = rfl.get_declared_struct_size(bufferType);
                uint32_t binding = rfl.get_decoration(buffer.id, spv::DecorationBinding);
                uint32_t set = rfl.get_decoration(buffer.id, spv::DecorationDescriptorSet);
                uint32_t memberCount = bufferType.member_types.size();
            }

            for (const auto& sampler : resources.sampled_images)
            {
                const auto& bufferType = rfl.get_type(sampler.base_type_id);
                uint32_t bufferSize = rfl.get_declared_struct_size(bufferType);
                uint32_t binding = rfl.get_decoration(sampler.id, spv::DecorationBinding);
                uint32_t slot = rfl.get_decoration(sampler.id, spv::DecorationDescriptorSet);
                uint32_t memberCount = bufferType.member_types.size();
            }
            
            return { dat.data(), dat.size() * sizeof(uint32_t), shaderType };
        }
        
        return { nullptr, 0 };
    }
    
}