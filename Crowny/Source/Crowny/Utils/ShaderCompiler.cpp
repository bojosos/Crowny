#include "cwpch.h"

#include "Crowny/Utils/ShaderCompiler.h"

#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/RenderAPI/Shader.h"

#include <spirv-reflect/spirv_reflect.h>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include <shaderc/shaderc.hpp>

#include <filesystem>

namespace Crowny
{

    // TODO: Make sure this dir exists, in project save it as .cache, with options to show hidden files in the asset
    // browser
    static std::filesystem::path CachePath = "/Cache";

    static shaderc_shader_kind ShaderTypeToShaderC(ShaderType shaderType)
    {
        switch (shaderType)
        {
        case VERTEX_SHADER:
            return shaderc_glsl_vertex_shader;
        case FRAGMENT_SHADER:
            return shaderc_glsl_fragment_shader;
        case GEOMETRY_SHADER:
            return shaderc_glsl_geometry_shader;
        case DOMAIN_SHADER:
            return shaderc_glsl_tess_control_shader;
        case HULL_SHADER:
            return shaderc_glsl_tess_evaluation_shader;
        case COMPUTE_SHADER:
            return shaderc_glsl_compute_shader;
        default:
            return shaderc_glsl_vertex_shader;
        }
    }

    static String ShaderFormatToExtension(ShaderOutputFormat shaderFormat)
    {
        switch (shaderFormat)
        {
        case (ShaderOutputFormat::Vulkan):
            return "vk";
        case (ShaderOutputFormat::OpenGL):
            return "gl";
        case (ShaderOutputFormat::Metal):
            return "msl";
        case (ShaderOutputFormat::D3D):
            return "d3d";
        }

        return String();
    }

    static String ShaderTypeToString(ShaderType shaderType)
    {
        switch (shaderType)
        {
        case VERTEX_SHADER:
            return "vertex";
        case FRAGMENT_SHADER:
            return "fragment";
        case GEOMETRY_SHADER:
            return "geometry";
        case HULL_SHADER:
            return "hull";
        case DOMAIN_SHADER:
            return "domain";
        case COMPUTE_SHADER:
            return "compute";
        default:
            return String();
        }
    }

    static UniformResourceType SPIRTypeToResourceType(const spirv_cross::SPIRType& type)
    {
        if (type.basetype == spirv_cross::SPIRType::SampledImage)
        {
            switch (type.image.dim)
            {
            case spv::Dim::Dim1D:
                return SAMPLER1D;
            case spv::Dim::Dim2D:
                return SAMPLER2D;
            case spv::Dim::Dim3D:
                return SAMPLER3D;
            case spv::Dim::DimCube:
                return SAMPLERCUBE;
            default:
                return TEXTURE_UNKNOWN;
            }
        }

        if (type.basetype == spirv_cross::SPIRType::Image)
        {
            switch (type.image.dim)
            {
            case spv::Dim::Dim1D:
                return TEXTURE1D;
            case spv::Dim::Dim2D:
                return TEXTURE2D;
            case spv::Dim::Dim3D:
                return TEXTURE3D;
            case spv::Dim::DimCube:
                return TEXTURECUBE;
            default:
                return TEXTURE_UNKNOWN;
            }
        }

        return TEXTURE_UNKNOWN;
    }

    static String ShaderTypeToPath(ShaderType shaderType, const String& filename, ShaderOutputFormat outputFormat)
    {
        return CachePath /
               (filename + "." + ShaderTypeToString(shaderType) + "." + ShaderFormatToExtension(outputFormat));
    }

    static ShaderType GetShaderTypeFromString(const String& type)
    {
        if (type == "vertex")
            return VERTEX_SHADER;
        if (type == "fragment" || type == "pixel")
            return FRAGMENT_SHADER;
        if (type == "geometry")
            return GEOMETRY_SHADER;
        if (type == "domain")
            return DOMAIN_SHADER;
        if (type == "hull")
            return HULL_SHADER;
        if (type == "compute")
            return COMPUTE_SHADER;

        CW_ENGINE_ASSERT(false, "Unknown shader type!");
        return SHADER_COUNT;
    }

    Ref<BinaryShaderData> ShaderCompiler::Compile(const String& source, ShaderType shaderType,
                                                  ShaderInputLanguage inputLanguage, ShaderOutputFormat outputFormat)
    {
        String sourcePath;
        VirtualFileSystem::Get()->ResolvePhyiscalPath(filepath, sourcePath);
        String shaderFilename = std::filesystem::path(sourcePath).filename();
        String shaderPath = ShaderTypeToPath(shaderType, shaderFilename, outputFormat);
        auto [data, size] =
          VirtualFileSystem::Get()->ReadFile(ShaderTypeToPath(shaderType, shaderFilename, outputFormat));

        Vector<uint8_t> shaderBinaryData;
        uint8_t* result = data;
        uint64_t sz = size;

        if (data == nullptr)
        {
            shaderc::Compiler compiler;
            shaderc::CompileOptions options;
            switch (inputLanguage)
            {
            case (ShaderInputLanguage::GLSL):
                options.SetSourceLanguage(shaderc_source_language_glsl);
                break;
            case (ShaderInputLanguage::HLSL):
                options.SetSourceLanguage(shaderc_source_language_hlsl);
                break;
            }
            options.SetSourceLanguage(shaderc_source_language_glsl);
            options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
            // options.SetOptimizationLevel(shaderc_optimization_level_performance); // if set, can't use uniform names

            shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(
              source.c_str(), source.size(), ShaderTypeToShaderC(shaderType), shaderFilename.c_str(), options);
            if (module.GetCompilationStatus() != shaderc_compilation_status_success)
            {
                String shaderTypeString = ShaderTypeToString(shaderType);
                shaderTypeString[0] = std::toupper(shaderTypeString[0]);
                CW_ENGINE_ERROR("{0} shader compilation error: {1}", shaderTypeString, module.GetErrorMessage());
            }
            else
                shaderBinaryData = Vector<uint8_t>((uint8_t*)module.cbegin(), (uint8_t*)module.cend());
        }

        if (shaderBinaryData.empty())
            return CreateRef<BinaryShaderData>(shaderBinaryData, "main", shaderType);

        Ref<BinaryShaderData> dataResult = CreateRef<BinaryShaderData>();
        dataResult->Data = shaderBinaryData;
        dataResult->Type = shaderType;
        dataResult->EntryPoint = "main"; // compiler.get_entry_points_and_stages().front().name;
        dataResult->Description = GetUniformDesc(shaderBinaryData);

        return dataResult;
    }

    Ref<UniformDesc> ShaderCompiler::GetUniformDesc(const Vector<uint8_t>& shaderBinaryData)
    {
        spirv_cross::Compiler compiler((uint32_t*)shaderBinaryData.data(), shaderBinaryData.size() / sizeof(uint32_t));
        spirv_cross::ShaderResources resources = compiler.get_shader_resources();

        Ref<UniformDesc> uniformDesc = CreateRef<UniformDesc>();
        for (const auto& uniform : resources.uniform_buffers)
        {
            const auto& bufferType = compiler.get_type(uniform.base_type_id);
            uint32_t bufferSize = compiler.get_declared_struct_size(bufferType);
            uint32_t binding = compiler.get_decoration(uniform.id, spv::DecorationBinding);
            uint32_t set = compiler.get_decoration(uniform.id, spv::DecorationDescriptorSet);
            uint32_t memberCount = bufferType.member_types.size();

            UniformBufferBlockDesc buffer;
            buffer.Name = uniform.name;
            buffer.BlockSize = bufferSize;
            buffer.Slot = binding;
            buffer.Set = set;
            uniformDesc->Uniforms[uniform.name] = buffer;
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

            uniformDesc->Samplers[resource.Name] = resource;
            uniformDesc->Textures[resource.Name] = resource;
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

            uniformDesc->Textures[resource.Name] = resource;
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

            uniformDesc->Samplers[resource.Name] = resource;
        }
    }

    UnorderedMap<ShaderType, String> ShaderCompiler::Parse(const String& source)
    {
        UnorderedMap<ShaderType, String> shaderSources;

        const char* typeToken = "#type";
        size_t typeTokenLength = strlen(typeToken);
        size_t pos = source.find(typeToken, 0);
        while (pos != String::npos)
        {
            size_t eol = source.find_first_of("\r\n", pos);
            CW_ENGINE_ASSERT(eol != String::npos, "Syntax error");
            size_t begin = pos + typeTokenLength + 1;
            String typeString = source.substr(begin, eol - begin);
            ShaderType shaderType = GetShaderTypeFromString(typeString);
            if (shaderType == SHADER_COUNT)
            {
                CW_ENGINE_ERROR("Shader type string {0} not recognized.", typeString);
                break;
            }
            if (typeString == "compute")
            {
                shaderSources[shaderType] = source.substr(begin + typeString.size());
                CW_ENGINE_ASSERT(shaderSources.size() == 1);
                return shaderSources;
            }

            size_t nextLinePos = source.find_first_not_of("\r\n", eol);
            CW_ENGINE_ASSERT(nextLinePos != String::npos, "Syntax error");
            pos = source.find(typeToken, nextLinePos);

            shaderSources[shaderType] =
              (pos == String::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
        }
        if (shaderSources.size() < 2)
        {
            CW_ENGINE_ERROR("You are required to provide at least a vertex and a fragment shader.");
        }
        return shaderSources;
    }

} // namespace Crowny
