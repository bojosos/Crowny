#include "cwpch.h"

#include "Crowny/Utils/ShaderCompiler.h"

#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/RenderAPI/Shader.h"

#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include <shaderc/shaderc.hpp>

#include <filesystem>

namespace Crowny
{

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

    static bool GetShaderTypeFromString(const String& type, ShaderType& outShaderType)
    {
        if (type == "vertex")
            outShaderType = VERTEX_SHADER;
        else if (type == "fragment" || type == "pixel")
            outShaderType = FRAGMENT_SHADER;
        else if (type == "geometry")
            outShaderType = GEOMETRY_SHADER;
        else if (type == "domain")
            outShaderType = DOMAIN_SHADER;
        else if (type == "hull")
            outShaderType = HULL_SHADER;
        else if (type == "compute")
            outShaderType = COMPUTE_SHADER;
        else
            return false;
        return true;
    }

    static bool GetShaderLanguage(const String& lang, ShaderLanguage& outShaderLanguage)
    {
        if (lang == "hlsl")
            outShaderLanguage = ShaderLanguage::HLSL;
        else if (lang == "glsl")
            outShaderLanguage = ShaderLanguage::GLSL;
        else
            return false;
        return true;
    }

    Ref<BinaryShaderData> ShaderCompiler::CompileStage(const String& source, ShaderType shaderType,
                                                       ShaderLanguage inputLanguage,
                                                       ShaderLanguageFlags outputLanguages, const UnorderedMap<String, String>& defines)
    {
        Vector<uint8_t> shaderBinaryData;

        shaderc::Compiler compiler;
        shaderc::CompileOptions options;
        for (auto& kv : defines)
            options.AddMacroDefinition(kv.first, kv.second);
        switch (inputLanguage)
        {
        case (ShaderLanguage::GLSL):
            options.SetSourceLanguage(shaderc_source_language_glsl);
            break;
        case (ShaderLanguage::HLSL):
            options.SetSourceLanguage(shaderc_source_language_hlsl);
            break;
        }

        options.SetSourceLanguage(shaderc_source_language_glsl);
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
        // options.SetOptimizationLevel(shaderc_optimization_level_performance); // if set, can't use uniform names, so maybe compile twice?

        shaderc::SpvCompilationResult module =
          compiler.CompileGlslToSpv(source.c_str(), source.size(), ShaderTypeToShaderC(shaderType),
                                    ShaderTypeToString(shaderType).c_str(), options);
        if (module.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            String shaderTypeString = ShaderTypeToString(shaderType);
            shaderTypeString[0] = std::toupper(shaderTypeString[0]);
            CW_ENGINE_ERROR("{0} shader compilation error: {1}", shaderTypeString, module.GetErrorMessage());
        }
        else
            shaderBinaryData = Vector<uint8_t>((uint8_t*)module.cbegin(), (uint8_t*)module.cend());

        if (shaderBinaryData.empty())
            return CreateRef<BinaryShaderData>(shaderBinaryData, "main", shaderType, nullptr);

        Ref<BinaryShaderData> dataResult = CreateRef<BinaryShaderData>();
        dataResult->Data = shaderBinaryData;
        dataResult->Type = shaderType;
        dataResult->EntryPoint = "main"; // compiler.get_entry_points_and_stages().front().name;
        dataResult->Description = GetUniformDesc(shaderBinaryData);
        return dataResult;
    }

    ShaderDesc ShaderCompiler::Compile(const String& source, ShaderLanguageFlags shaderLanguage, const UnorderedMap<String, String>& defines)
    {
        const char* langToken = "#lang";
        size_t langTokenLength = strlen(langToken);
        size_t pos = source.find(langToken, 0);
        ShaderLanguage inputLanguage = ShaderLanguage::GLSL;
        if (pos != String::npos)
        {
            size_t eol = source.find_first_of("\n\r", pos);
            if (eol != String::npos)
            {
                size_t begin = pos + langTokenLength + 1;
                String langString = source.substr(begin, eol - begin);
                if (!GetShaderLanguage(langString, inputLanguage))
                    CW_ENGINE_ERROR("Shader language string {0} not recognized. Assuming shader is in GLSL.",
                                    langString);
            }
        }
        else
            CW_ENGINE_WARN("#lang directive not found, assuming shader is in GLSL.");

        ShaderDesc shaderDesc;

        auto sourceShaders = Parse(source);
        for (auto entry : sourceShaders)
        {
            Ref<BinaryShaderData> shaderData = CompileStage(entry.second, entry.first, inputLanguage, shaderLanguage, defines);
            if (entry.first == VERTEX_SHADER)
                shaderDesc.VertexShader = shaderData;
            else if (entry.first == FRAGMENT_SHADER)
                shaderDesc.FragmentShader = shaderData;
            else if (entry.first == GEOMETRY_SHADER)
                shaderDesc.GeometryShader = shaderData;
            else if (entry.first == HULL_SHADER)
                shaderDesc.DomainShader = shaderData;
            else if (entry.first == DOMAIN_SHADER)
                shaderDesc.DomainShader = shaderData;
            else if (entry.first == COMPUTE_SHADER)
                shaderDesc.ComputeShader = shaderData;
        }

        return shaderDesc;
    }

    Ref<UniformDesc> ShaderCompiler::GetUniformDesc(const Vector<uint8_t>& shaderBinaryData)
    {
        spirv_cross::Compiler compiler((uint32_t*)shaderBinaryData.data(), shaderBinaryData.size() / sizeof(uint32_t));
        spirv_cross::ShaderResources resources = compiler.get_shader_resources();

        Ref<UniformDesc> uniformDesc = CreateRef<UniformDesc>();
        for (const auto& uniform : resources.uniform_buffers)
        {
            const auto& bufferType = compiler.get_type(uniform.base_type_id);
            uint32_t bufferSize = (uint32_t)compiler.get_declared_struct_size(bufferType);
            uint32_t binding = compiler.get_decoration(uniform.id, spv::DecorationBinding);
            uint32_t set = compiler.get_decoration(uniform.id, spv::DecorationDescriptorSet);
            uint32_t memberCount = (uint32_t)bufferType.member_types.size();

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

        return uniformDesc;
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
            ShaderType shaderType;
            if (!GetShaderTypeFromString(typeString, shaderType))
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
            CW_ENGINE_ERROR("You are required to provide at least a vertex and a fragment shader.");

        return shaderSources;
    }

} // namespace Crowny
