#include "cwpch.h"

#include "Crowny/Utils/ShaderCompiler.h"
#include "Crowny/Renderer/Shader.h"
#include "Crowny/Common/VirtualFileSystem.h"

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv-reflect/spirv_reflect.h>
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
    
    static UniformResourceType SPIRTypeToResourceType(const spirv_cross::SPIRType& type)
    {
        if (type.basetype == spirv_cross::SPIRType::SampledImage)
        {
            switch(type.image.dim)
            {
                case spv::Dim::Dim1D:   return SAMPLER1D;
                case spv::Dim::Dim2D:   return SAMPLER2D;
                case spv::Dim::Dim3D:   return SAMPLER3D;
                case spv::Dim::DimCube: return SAMPLERCUBE;
            }
        }
        
        if (type.basetype == spirv_cross::SPIRType::Image)
        {
            switch(type.image.dim)
            {
                case spv::Dim::Dim1D:   return TEXTURE1D;
                case spv::Dim::Dim2D:   return TEXTURE2D;
                case spv::Dim::Dim3D:   return TEXTURE3D;
                case spv::Dim::DimCube: return TEXTURECUBE;
            }
        }
        
        return TEXTURE_UNKNOWN;
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
        
        uint8_t* result = data;
        uint64_t sz = size;
        
        if (data == nullptr)
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
            //options.SetOptimizationLevel(shaderc_optimization_level_performance); // if set, we can't use uniform names
            
            std::string source = VirtualFileSystem::Get()->ReadTextFile(sourcePath);
            shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source.c_str(), source.size(), ShaderTypeToShaderC(shaderType), shaderFilename.c_str(), options);
            if (module.GetCompilationStatus() != shaderc_compilation_status_success)
            {
                CW_ENGINE_ERROR("Shader compilation error: {0}", module.GetErrorMessage());
                CW_ENGINE_ASSERT(false);
                //return { nullptr, 0 };
                result = nullptr;
                sz = 0;
            }
            else
            {
                dat = std::vector<uint32_t>(module.cbegin(), module.cend()); // TODO: memcpy
                CW_ENGINE_INFO(dat.size());
                VirtualFileSystem::Get()->WriteFile(shaderPath, (byte*)dat.data(), dat.size() * sizeof(uint32_t));
                result = (uint8_t*)dat.data();
                sz = dat.size() * sizeof(uint32_t);
            }
        }

        if (!result)
            return { nullptr, 0, "main", shaderType };
        spirv_cross::Compiler compiler((uint32_t*)result, sz / sizeof(uint32_t));
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();
        
        BinaryShaderData dataResult;
        dataResult.Data = result;
        dataResult.Size = sz;
        dataResult.Type = shaderType;
        dataResult.EntryPoint = "main";//compiler.get_entry_points_and_stages().front().name;
        //CW_ENGINE_INFO(dataResult.EntryPoint);
        dataResult.Description = CreateRef<UniformDesc>();
        CW_ENGINE_INFO("Uniform buffers: {0}", resources.uniform_buffers.size());
        for (const auto& uniform : resources.uniform_buffers)
        {
            const auto& bufferType = compiler.get_type(uniform.base_type_id);
            uint32_t bufferSize = compiler.get_declared_struct_size(bufferType);
            uint32_t binding = compiler.get_decoration(uniform.id, spv::DecorationBinding);
            uint32_t set = compiler.get_decoration(uniform.id, spv::DecorationDescriptorSet);
            uint32_t memberCount = bufferType.member_types.size();
            CW_ENGINE_INFO(uniform.name);
            
            UniformBufferBlockDesc buffer;
            buffer.Name = uniform.name;
            buffer.BlockSize = bufferSize;
            buffer.Slot = binding;
            buffer.Set = set;
            dataResult.Description->Uniforms[uniform.name] = buffer;
        }
        
        for (const auto& sampler : resources.sampled_images)
        {
            const auto& bufferType = compiler.get_type(sampler.base_type_id);
            uint32_t binding = compiler.get_decoration(sampler.id, spv::DecorationBinding);
            uint32_t set = compiler.get_decoration(sampler.id, spv::DecorationDescriptorSet);

            UniformResourceDesc resource;
            resource.Name = sampler.name;
            resource.Type = SPIRTypeToResourceType(bufferType);
            resource.Slot = binding;
            resource.Set = set;

            dataResult.Description->Samplers[resource.Name] = resource;
            dataResult.Description->Textures[resource.Name] = resource;
        }

        for (const auto& texture : resources.separate_images)
        {
            const auto& bufferType = compiler.get_type(texture.base_type_id);
            uint32_t binding = compiler.get_decoration(texture.id, spv::DecorationBinding);
            uint32_t set = compiler.get_decoration(texture.id, spv::DecorationDescriptorSet);
            
            UniformResourceDesc resource;
            resource.Name = texture.name;
            resource.Type = SPIRTypeToResourceType(bufferType);
            resource.Slot = binding;
            resource.Set = set;
                
            dataResult.Description->Textures[resource.Name] = resource;
        }

        for (const auto& sampler : resources.separate_samplers)
        {
            const auto& bufferType = compiler.get_type(sampler.base_type_id);
            uint32_t binding = compiler.get_decoration(sampler.id, spv::DecorationBinding);
            uint32_t set = compiler.get_decoration(sampler.id, spv::DecorationDescriptorSet);

            UniformResourceDesc resource;
            resource.Name = sampler.name;
            resource.Type = SPIRTypeToResourceType(bufferType);
            resource.Slot = binding;
            resource.Set = set;

            dataResult.Description->Samplers[resource.Name] = resource;
        }

        return dataResult;
    }
    
}
